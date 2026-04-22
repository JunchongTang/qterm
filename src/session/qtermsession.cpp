#include "qtermsession.h"

#include "backend/qtermlocalbackend.h"
#include "vterm/qtermvtermprocessor.h"

#include <QDebug>

// Default terminal size before the first geometry-driven resize
static constexpr int kDefaultRows = 24;
static constexpr int kDefaultCols = 80;

QTermSession::QTermSession(QObject *parent)
    : QObject(parent)
{
    m_processor = new QTermVTermProcessor(kDefaultRows, kDefaultCols, this);
    m_backend   = new QTermLocalBackend(this);

    // PTY output → vterm
    connect(m_backend, &QTermSessionBackend::dataReceived,
            this, &QTermSession::onDataReceived);

    // vterm keyboard/mouse output → PTY
    connect(m_processor, &QTermVTermProcessor::outputData,
            m_backend, &QTermSessionBackend::sendData);

    // vterm screen damage → view repaint
    connect(m_processor, &QTermVTermProcessor::screenUpdated,
            this, &QTermSession::onScreenUpdated);

    // cursor, title, bell
    connect(m_processor, &QTermVTermProcessor::cursorMoved,
            this, [this](VTermPos /*pos*/, bool /*visible*/) {
                Q_EMIT displayChanged(0, m_processor->rows());
            });

    connect(m_processor, &QTermVTermProcessor::titleChanged,
            this, &QTermSession::titleChanged);

    connect(m_processor, &QTermVTermProcessor::bellRang,
            this, &QTermSession::bellRang);

    // Backend finished
    connect(m_backend, &QTermSessionBackend::finished,
            this, &QTermSession::finished);
}

QTermSession::~QTermSession() = default;

void QTermSession::start()
{
    m_backend->start();
    // Sync initial size
    m_backend->resize(m_processor->rows(), m_processor->cols());
}

// ---------------------------------------------------------------------------
// Size
// ---------------------------------------------------------------------------

void QTermSession::resizeTerminal(int rows, int cols)
{
    if (rows <= 0 || cols <= 0)
        return;
    if (rows == m_processor->rows() && cols == m_processor->cols())
        return;

    m_backend->resize(rows, cols);
    m_processor->resize(rows, cols);

    // Clamp scroll offset in case scrollback shrank conceptually
    if (m_scrollOffset > maxScrollOffset())
        setScrollOffset(maxScrollOffset());
}

int QTermSession::terminalRows() const
{
    return m_processor->rows();
}

int QTermSession::terminalCols() const
{
    return m_processor->cols();
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void QTermSession::sendKey(VTermKey key, VTermModifier mod)
{
    // Any key press while scrolled should snap back to the live view
    if (m_scrollOffset != 0)
        setScrollOffset(0);

    m_processor->sendKey(key, mod);
}

void QTermSession::sendChar(uint32_t unichar, VTermModifier mod)
{
    if (m_scrollOffset != 0)
        setScrollOffset(0);

    m_processor->sendChar(unichar, mod);
}

void QTermSession::sendData(const QByteArray &data)
{
    m_backend->sendData(data);
}

void QTermSession::startPaste()
{
    m_processor->startPaste();
}

void QTermSession::endPaste()
{
    m_processor->endPaste();
}

// ---------------------------------------------------------------------------
// Mouse
// ---------------------------------------------------------------------------

void QTermSession::sendMouseMove(int row, int col, VTermModifier mod)
{
    m_processor->sendMouseMove(row, col, mod);
}

void QTermSession::sendMouseButton(int button, bool pressed, VTermModifier mod)
{
    m_processor->sendMouseButton(button, pressed, mod);
}

// ---------------------------------------------------------------------------
// Scrollback
// ---------------------------------------------------------------------------

int QTermSession::maxScrollOffset() const
{
    return m_processor->scrollbackLineCount();
}

void QTermSession::setScrollOffset(int offset)
{
    offset = qBound(0, offset, maxScrollOffset());
    if (offset == m_scrollOffset)
        return;

    m_scrollOffset = offset;
    Q_EMIT scrollOffsetChanged(m_scrollOffset);
    Q_EMIT displayChanged(0, m_processor->rows());
}

void QTermSession::scrollBy(int delta)
{
    setScrollOffset(m_scrollOffset + delta);
}

// ---------------------------------------------------------------------------
// Cell query (scrollback-aware)
// ---------------------------------------------------------------------------

bool QTermSession::cellAt(int row, int col, VTermScreenCell *cell) const
{
    const int sbLines = m_processor->scrollbackLineCount();

    // How many of the visible rows are actually scrollback lines?
    // scrollOffset == 0: all rows are live screen rows.
    // scrollOffset == N: the top N rows come from scrollback.
    const int sbRowsVisible = qMin(m_scrollOffset, sbLines);

    if (row < sbRowsVisible) {
        // This row lives in the scrollback buffer.
        // Scrollback index 0 = line just above the live screen.
        // Display row 0 with scrollOffset S means scrollback[S - 1 - 0].
        int sbIdx = m_scrollOffset - 1 - row;
        if (sbIdx < 0 || sbIdx >= sbLines)
            return false;

        const QVector<VTermScreenCell> &line = m_processor->scrollbackLine(sbIdx);
        if (col < 0 || col >= line.size())
            return false;

        *cell = line.at(col);
        return true;
    } else {
        // This row is within the live screen.
        int screenRow = row - sbRowsVisible;
        return m_processor->screenCellAt(screenRow, col, cell);
    }
}

// ---------------------------------------------------------------------------
// Cursor
// ---------------------------------------------------------------------------

VTermPos QTermSession::cursorPos() const
{
    return m_processor->cursorPos();
}

bool QTermSession::isCursorVisible() const
{
    return m_processor->isCursorVisible();
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void QTermSession::onDataReceived(const QByteArray &data)
{
    // If we're scrolled up and new data arrives, push scrollback grows
    // automatically through libvterm callbacks.
    m_processor->feedData(data);
}

void QTermSession::onScreenUpdated(int startRow, int endRow)
{
    // If we're scrolled, the live screen rows are offset downward
    const int offset = qMin(m_scrollOffset, m_processor->scrollbackLineCount());
    Q_EMIT displayChanged(startRow + offset, endRow + offset);
}

void QTermSession::onScrollbackChanged()
{
    // Scrollback count changed; notify the view so it can update scrollbar
    Q_EMIT scrollOffsetChanged(m_scrollOffset);
}
