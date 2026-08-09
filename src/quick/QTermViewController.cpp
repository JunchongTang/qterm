#include "QTermViewController.h"

#include <QTerm/QTermSurfaceModel.h>
#include <QTerm/QTermTerminal.h>

#include <QFont>
#include <QFontMetricsF>
#include <QHoverEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPoint>
#include <QWheelEvent>

#include <cmath>
#include <mutex>

#ifdef Q_OS_MACOS
namespace QTerm { void disableMacOSPressAndHold(); }
#endif

namespace QTerm {

namespace {

constexpr int kMinimumColumns         = 20;
constexpr int kMinimumRows            = 8;
constexpr int kWheelScrollRowsPerStep = 3;
constexpr int kResizeDebounceIntervalMs = 30;

int runColumnCount(const QVariantMap &run)
{
    const int columns = run.value(QStringLiteral("columns"), 0).toInt();
    if (columns > 0)
        return columns;
    return qMax(1, run.value(QStringLiteral("text")).toString().size());
}

QFont buildFont(const QString &family, int pixelSize)
{
    QFont font(family);
    font.setPixelSize(pixelSize);
    return font;
}

} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

QTermViewController::QTermViewController(QObject *parent)
    : QObject(parent)
    , m_resizeDebounceTimer(new QTimer(this))
    , m_selectionAutoScrollTimer(new QTimer(this))
    , m_clickResetTimer(new QTimer(this))
{
    m_resizeDebounceTimer->setSingleShot(true);
    m_resizeDebounceTimer->setInterval(kResizeDebounceIntervalMs);
    connect(m_resizeDebounceTimer, &QTimer::timeout, this, [this]() {
        syncTerminalSize();
    });

    // A click run ends when nothing follows within 360 ms.
    m_clickResetTimer->setSingleShot(true);
    m_clickResetTimer->setInterval(360);
    connect(m_clickResetTimer, &QTimer::timeout, this, [this]() {
        m_clickStreak   = 0;
        m_lastClickRow    = -1;
        m_lastClickColumn = -1;
    });

    // While a drag is past an edge, auto-scroll every 35 ms.
    m_selectionAutoScrollTimer->setSingleShot(false);
    m_selectionAutoScrollTimer->setInterval(35);
    connect(m_selectionAutoScrollTimer, &QTimer::timeout, this, [this]() {
        if (!m_terminal || m_autoScrollDirection == 0) {
            m_selectionAutoScrollTimer->stop();
            return;
        }
        const qreal overflow = m_autoScrollDirection > 0
            ? qMax<qreal>(0.0, -m_dragY)
            : qMax<qreal>(0.0, m_dragY - m_viewHeight);
        const int rowsPerTick = qMax(1, static_cast<int>(
            std::ceil(overflow / qMax<qreal>(1.0, m_cellHeight))));
        m_terminal->scrollByLines(m_autoScrollDirection * rowsPerTick);
        updateSelectionFromDrag(m_dragX, m_dragY);
    });

    updateMetrics();

#ifdef Q_OS_MACOS
    static std::once_flag s_pressAndHoldDisabled;
    std::call_once(s_pressAndHoldDisabled, disableMacOSPressAndHold);
#endif
}

// ── Terminal binding ─────────────────────────────────────────────────────────

QTermTerminal *QTermViewController::terminal() const noexcept
{
    return m_terminal;
}

void QTermViewController::setTerminal(QTermTerminal *terminal)
{
    if (m_terminal == terminal)
        return;

    disconnectSurfaceModel();
    QObject::disconnect(m_viewportConnection);
    QObject::disconnect(m_modeStateConnection);
    m_terminal = terminal;
    reconnectSurfaceModel();

    if (m_terminal) {
        m_viewportConnection = connect(
            m_terminal, &QTermTerminal::viewportChanged,
            this, [this]() { emit scrollChanged(); });
        m_modeStateConnection = connect(
            m_terminal, &QTermTerminal::modeStateChanged,
            this, [this]() { emit mouseAcceptanceChanged(); });
    }

    scheduleTerminalSizeSync();
    emit terminalChanged();
    emit scrollChanged();
}

// ── Font and metrics ─────────────────────────────────────────────────────────

QString QTermViewController::fontFamily() const
{
    return m_fontFamily;
}

void QTermViewController::setFontFamily(const QString &family)
{
    if (m_fontFamily == family)
        return;
    m_fontFamily = family;
    updateMetrics();
}

int QTermViewController::fontPixelSize() const noexcept
{
    return m_fontPixelSize;
}

void QTermViewController::setFontPixelSize(int size)
{
    const int bounded = qMax(1, size);
    if (m_fontPixelSize == bounded)
        return;
    m_fontPixelSize = bounded;
    updateMetrics();
}

qreal QTermViewController::cellWidth() const noexcept
{
    return m_cellWidth;
}

qreal QTermViewController::cellHeight() const noexcept
{
    return m_cellHeight;
}

// ── Geometry ─────────────────────────────────────────────────────────────────

void QTermViewController::notifyGeometryChanged(qreal w, qreal h)
{
    const bool wasEmpty = (m_viewWidth <= 0.0 || m_viewHeight <= 0.0);
    m_viewWidth  = w;
    m_viewHeight = h;

    if (w > 0.0 && h > 0.0) {
        if (wasEmpty)
            syncTerminalSize();        // first real size: apply it at once
        else
            scheduleTerminalSizeSync(); // later changes: debounce, a drag sends many
    }
}

// ── Scrolling ────────────────────────────────────────────────────────────────

qreal QTermViewController::scrollSize() const noexcept
{
    if (!m_terminal) return 1.0;
    const int rows  = m_terminal->rows();
    const int total = rows + m_terminal->maxScrollOffset();
    if (total <= 0) return 1.0;
    return qMax(0.08, qMin(1.0, static_cast<qreal>(rows) / total));
}

qreal QTermViewController::scrollPosition() const noexcept
{
    if (!m_terminal || m_terminal->maxScrollOffset() <= 0) return 0.0;
    const qreal trackSpan = 1.0 - scrollSize();
    if (trackSpan <= 0.0) return 0.0;
    // scrollOffset 0 is the bottom (position = trackSpan); max is the top (0).
    return trackSpan * (1.0 - static_cast<qreal>(m_terminal->scrollOffset())
                                   / m_terminal->maxScrollOffset());
}

void QTermViewController::setScrollPosition(qreal position)
{
    if (!m_terminal || m_terminal->maxScrollOffset() <= 0) return;
    const qreal trackSpan = 1.0 - scrollSize();
    if (trackSpan <= 0.0) return;
    const qreal normalized = qBound(0.0, position / trackSpan, 1.0);
    const int targetOffset = qRound(m_terminal->maxScrollOffset() * (1.0 - normalized));
    const int delta = targetOffset - m_terminal->scrollOffset();
    if (delta != 0) m_terminal->scrollByLines(delta);
}

// ── Coordinate helpers ───────────────────────────────────────────────────────

int QTermViewController::rowAtPosition(qreal y) const
{
    const int rowCount = m_terminal ? m_terminal->rows() : 0;
    if (rowCount <= 0) return 0;
    const int row = static_cast<int>(std::floor(y / qMax<qreal>(1.0, m_cellHeight)));
    return qBound(0, row, rowCount - 1);
}

int QTermViewController::columnAtPosition(qreal x) const
{
    const int columnCount = m_terminal ? m_terminal->columns() : 0;
    if (columnCount <= 0) return 0;
    const int column = static_cast<int>(std::floor(x / qMax<qreal>(1.0, m_cellWidth)));
    return qBound(0, column, columnCount);
}

int QTermViewController::hyperlinkIdAtPosition(int row, int col) const
{
    if (!m_terminal) return 0;
    QTermSurfaceModel *surfaceModel = m_terminal->surfaceModel();
    if (!surfaceModel) return 0;
    const QVariantList allRuns = surfaceModel->visibleLineRuns();
    if (row < 0 || row >= allRuns.size()) return 0;
    const QVariantList lineRuns = allRuns.at(row).toList();
    int x = 0;
    for (const QVariant &rv : lineRuns) {
        const QVariantMap run = rv.toMap();
        const int columns = runColumnCount(run);
        if (col < x + columns)
            return run.value(QStringLiteral("hyperlinkId")).toInt();
        x += columns;
    }
    return 0;
}

// ── Mouse protocol state ─────────────────────────────────────────────────────

bool QTermViewController::mouseProtocolEnabled() const
{
    if (!m_terminal) return false;
    return m_terminal->isMouseProtocolActive();
}

bool QTermViewController::hoverEventsNeeded() const
{
    if (!m_terminal) return false;
    return m_terminal->isHoverTrackingActive();
}

// ── IME cursor rectangle ─────────────────────────────────────────────────────

QRectF QTermViewController::cursorRect() const
{
    if (!m_terminal) return {};
    QTermSurfaceModel *sm = m_terminal->surfaceModel();
    if (!sm) return {};
    return QRectF(sm->cursorColumn() * m_cellWidth,
                  sm->cursorRow()    * m_cellHeight,
                  m_cellWidth,
                  m_cellHeight);
}

// ── Input dispatch ───────────────────────────────────────────────────────────

bool QTermViewController::handleKeyPress(QKeyEvent *event)
{
    if (!m_terminal) return false;

    if (m_terminal->scrollOffset() > 0)
        m_terminal->scrollToBottom();
    // On macOS Ctrl+letter can arrive with empty text; the encoder in sendKey
    // synthesises the control character from the key and modifiers instead.
    m_terminal->sendKey(event->key(), event->text());
    return true;
}

bool QTermViewController::handleInputMethod(QInputMethodEvent *event)
{
    if (!m_terminal) return false;

    const QString commit = event->commitString();
    if (!commit.isEmpty()) {
        if (m_terminal->scrollOffset() > 0)
            m_terminal->scrollToBottom();
        m_terminal->sendPaste(commit);
    }
    // Preedit text is drawn by the platform plugin in its own candidate window,
    // so there is nothing to do here.
    return true;
}

bool QTermViewController::handleMousePress(QMouseEvent *event)
{
    if (!m_terminal || event->button() != Qt::LeftButton)
        return false;

    emit focusRequested();

    if (m_terminal->isMouseProtocolActive()) {
        m_terminal->sendMouse(rowAtPosition(event->position().y()),
                              columnAtPosition(event->position().x()),
                              event->button(), event->modifiers(), true);
        return true;
    }

    const int row = rowAtPosition(event->position().y());
    const int col = columnAtPosition(event->position().x());

    // OSC 8 hyperlink activation is Cmd+click (Qt maps ControlModifier to Cmd).
    if (event->modifiers() & Qt::ControlModifier) {
        const int hyperlinkId = hyperlinkIdAtPosition(row, col);
        if (hyperlinkId > 0) {
            const QString url = m_terminal->hyperlinkUrl(hyperlinkId);
            if (!url.isEmpty()) {
                emit hyperlinkActivated(url);
                return true;
            }
        }
    }

    // Click-run detection
    if (m_clickResetTimer->isActive() && m_lastClickRow == row && m_lastClickColumn == col)
        m_clickStreak += 1;
    else
        m_clickStreak = 1;

    m_lastClickRow    = row;
    m_lastClickColumn = col;
    m_clickResetTimer->start();

    if (m_clickStreak >= 3) {
        // Triple click selects the whole logical line.
        m_selectionAnchorRow    = -1;
        m_selectionAnchorColumn = -1;
        m_selectionAnchorProjectionRow = -1;
        m_suppressSelectionRelease = true;
        m_autoScrollDirection = 0;
        m_selectionAutoScrollTimer->stop();
        m_terminal->selectLogicalLineAt(row);
        return true;
    }

    m_selectionAnchorRow    = row;
    m_selectionAnchorColumn = col;
    m_selectionAnchorProjectionRow = m_terminal->viewportTopProjectionRow() + row;
    m_dragX = event->position().x();
    m_dragY = event->position().y();
    m_suppressSelectionRelease = false;
    m_autoScrollDirection = 0;
    m_selectionAutoScrollTimer->stop();
    m_terminal->clearSelection();
    return true;
}

bool QTermViewController::handleMouseDoubleClick(QMouseEvent *event)
{
    if (!m_terminal || event->button() != Qt::LeftButton)
        return false;

    emit focusRequested();

    m_selectionAnchorRow    = -1;
    m_selectionAnchorColumn = -1;
    m_selectionAnchorProjectionRow = -1;
    m_autoScrollDirection = 0;
    m_selectionAutoScrollTimer->stop();
    m_terminal->selectWordAt(rowAtPosition(event->position().y()),
                             columnAtPosition(event->position().x()));
    return true;
}

bool QTermViewController::handleMouseMove(QMouseEvent *event)
{
    if (!m_terminal) return false;

    if (m_terminal->isHoverTrackingActive() ||
        (m_terminal->isButtonTrackingActive() && (event->buttons() & Qt::LeftButton))) {
        const Qt::MouseButton heldButton =
            (event->buttons() & Qt::LeftButton)   ? Qt::LeftButton   :
            (event->buttons() & Qt::MiddleButton) ? Qt::MiddleButton :
            (event->buttons() & Qt::RightButton)  ? Qt::RightButton  : Qt::NoButton;
        m_terminal->sendMouse(rowAtPosition(event->position().y()),
                              columnAtPosition(event->position().x()),
                              heldButton, event->modifiers(), false, /*isMotion=*/true);
        return true;
    }

    if (!(event->buttons() & Qt::LeftButton) || m_suppressSelectionRelease
        || m_selectionAnchorRow < 0 || m_selectionAnchorColumn < 0)
        return false;

    m_dragX = event->position().x();
    m_dragY = event->position().y();
    updateSelectionFromDrag(m_dragX, m_dragY);

    if (m_dragY < 0)
        m_autoScrollDirection = 1;           // past the top edge: scroll up
    else if (m_dragY > m_viewHeight)
        m_autoScrollDirection = -1;          // past the bottom edge: scroll down
    else
        m_autoScrollDirection = 0;

    if (m_autoScrollDirection != 0 && !m_selectionAutoScrollTimer->isActive())
        m_selectionAutoScrollTimer->start();
    else if (m_autoScrollDirection == 0)
        m_selectionAutoScrollTimer->stop();

    return true;
}

bool QTermViewController::handleMouseRelease(QMouseEvent *event)
{
    if (!m_terminal) return false;

    if (m_terminal->isMouseProtocolActive()) {
        m_terminal->sendMouse(rowAtPosition(event->position().y()),
                              columnAtPosition(event->position().x()),
                              event->button(), event->modifiers(), false);
        return true;
    }

    if (event->button() != Qt::LeftButton)
        return false;

    m_autoScrollDirection = 0;
    m_selectionAutoScrollTimer->stop();

    if (m_suppressSelectionRelease) {
        m_suppressSelectionRelease = false;
        return true;
    }

    updateSelectionFromDrag(event->position().x(), event->position().y());
    m_selectionAnchorRow    = -1;
    m_selectionAnchorColumn = -1;
    m_selectionAnchorProjectionRow = -1;
    return true;
}

bool QTermViewController::handleHoverMove(QHoverEvent *event)
{
    if (!m_terminal) return false;

    if (m_terminal->isHoverTrackingActive()) {
        const QPointF pos = event->position();
        m_terminal->sendMouse(rowAtPosition(pos.y()), columnAtPosition(pos.x()),
                              Qt::NoButton, Qt::NoModifier, false, /*isMotion=*/true);
        return true;
    }
    return false;
}

bool QTermViewController::handleWheel(QWheelEvent *event)
{
    if (!m_terminal) return false;

    const QPoint angleDelta = event->angleDelta();

    // Ctrl (⌘ on macOS) + wheel = zoom intent, takes priority over everything
    // (including the mouse protocol): does not scroll the scrollback; reports
    // zoomRequested for the host to resize the font. Zoom must be gentle: pixel
    // delta is ~120px per step (≈ one detent on a precise mouse = one step, matching
    // the keyboard's 1 step, no longer jumping 3 per detent); a wheel detent is 120 units.
    if (event->modifiers() & Qt::ControlModifier) {
        // Don't zoom during the momentum phase, or the inertia after release keeps
        // zooming wildly. (Only macOS reports this phase.)
        if (event->phase() == Qt::ScrollMomentum)
            return true;                                   // consume, neither scroll nor zoom
        const QPoint pixelDelta = event->pixelDelta();
        qreal stepDelta = 0.0;
        if (pixelDelta.y() != 0)
            stepDelta = pixelDelta.y() / 120.0;
        else if (angleDelta.y() != 0)
            stepDelta = angleDelta.y() / 120.0;
        if (!qFuzzyIsNull(stepDelta)) {
            if ((stepDelta > 0.0) != (m_zoomStepAccumulator > 0.0))
                m_zoomStepAccumulator = 0.0;               // direction reversed → drop remainder
            m_zoomStepAccumulator += stepDelta;
            const int steps = static_cast<int>(m_zoomStepAccumulator);
            if (steps != 0) {
                m_zoomStepAccumulator -= steps;
                emit zoomRequested(steps);                 // + zoom in / - zoom out
            }
        }
        return true;                                       // consume, don't scroll
    }

    if (m_terminal->isMouseProtocolActive() && angleDelta.y() != 0) {
        // With mouse reporting on, a wheel event is a button press: the X10 and
        // SGR conventions use the synthetic codes 64 for up and 65 for down.
        const int wheelButton = angleDelta.y() > 0 ? 64 : 65;
        m_terminal->sendMouse(
            rowAtPosition(event->position().y()),
            columnAtPosition(event->position().x()),
            wheelButton,
            static_cast<int>(event->modifiers()),
            true);
        return true;
    }

    // Mouse protocol disabled: scroll the scrollback.
    // Prefer high resolution: trackpads / Magic Mouse send pixelDelta (and emit
    // high-frequency events during momentum); map it 1:1 to fractional rows by line
    // height — tracks the finger, no overshoot. Classic wheels have no pixelDelta,
    // so use angleDelta (120 units = one detent, kWheelScrollRowsPerStep rows each).
    // On macOS those devices' angleDelta is a coarse derived value, so whenever
    // pixelDelta is present it always wins (same approach as VS Code).
    const QPoint pixelDelta = event->pixelDelta();
    qreal rowsDelta = 0.0;
    if (pixelDelta.y() != 0) {
        rowsDelta = pixelDelta.y() / qMax<qreal>(1.0, m_cellHeight);
    } else if (angleDelta.y() != 0) {
        rowsDelta = (angleDelta.y() / 120.0) * kWheelScrollRowsPerStep;
    }
    if (qFuzzyIsNull(rowsDelta)) return false;

    // Direction reversed → drop the remainder so it tracks the finger immediately.
    if ((rowsDelta > 0.0) != (m_wheelRowAccumulator > 0.0))
        m_wheelRowAccumulator = 0.0;
    m_wheelRowAccumulator += rowsDelta;

    // Truncate toward zero to whole rows; if less than a row, accumulate and
    // swallow this event (don't force a 1-row scroll).
    const int wholeRows = static_cast<int>(m_wheelRowAccumulator);
    if (wholeRows == 0)
        return true;
    m_wheelRowAccumulator -= wholeRows;

    m_terminal->scrollByLines(wholeRows);
    emit wheelScrolled(m_terminal->scrollOffset());
    emit scrollChanged();
    return true;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void QTermViewController::reconnectSurfaceModel()
{
    if (!m_terminal) return;

    QTermSurfaceModel *surfaceModel = m_terminal->surfaceModel();
    if (!surfaceModel) return;

    m_surfaceSizeConnection = connect(surfaceModel, &QTermSurfaceModel::sizeChanged, this, [this]() {
        emit repaintNeeded();
    });
    m_surfaceCursorConnection = connect(surfaceModel, &QTermSurfaceModel::cursorChanged, this, [this]() {
        emit repaintNeeded();
        emit cursorUpdateNeeded();
    });
    m_surfaceSelectionConnection = connect(surfaceModel, &QTermSurfaceModel::selectionChanged, this, [this]() {
        emit repaintNeeded();
        emit selectionChanged();
    });
    // Search highlights are background-fill rectangles like the selection;
    // relay through the same dirty path (the view rebuilds both together).
    m_surfaceSearchConnection = connect(surfaceModel, &QTermSurfaceModel::searchHighlightsChanged, this, [this]() {
        emit repaintNeeded();
        emit selectionChanged();
    });
    m_surfaceVisibleRunsConnection = connect(surfaceModel, &QTermSurfaceModel::visibleLineRunsChanged, this, [this]() {
        emit repaintNeeded();
    });
    m_surfacePartialRunsConnection = connect(surfaceModel, &QTermSurfaceModel::visibleLineRunsChangedPartial, this, [this](QVector<int> rows) {
        emit contentRowsDirty(rows);
    });
    m_surfaceDestroyedConnection = connect(surfaceModel, &QObject::destroyed, this, [this]() {
        disconnectSurfaceModel();
        emit repaintNeeded();
    });
}

void QTermViewController::disconnectSurfaceModel()
{
    QObject::disconnect(m_surfaceSizeConnection);
    QObject::disconnect(m_surfaceCursorConnection);
    QObject::disconnect(m_surfaceSelectionConnection);
    QObject::disconnect(m_surfaceSearchConnection);
    QObject::disconnect(m_surfaceVisibleRunsConnection);
    QObject::disconnect(m_surfacePartialRunsConnection);
    QObject::disconnect(m_surfaceDestroyedConnection);
}

void QTermViewController::updateMetrics()
{
    const QFontMetricsF metrics(buildFont(m_fontFamily, m_fontPixelSize));
    const qreal previousCellWidth  = m_cellWidth;
    const qreal previousCellHeight = m_cellHeight;

    m_cellWidth  = qMax<qreal>(1.0, metrics.horizontalAdvance(QLatin1Char('M')));
    m_cellHeight = qMax<qreal>(1.0, metrics.lineSpacing());

    if (!qFuzzyCompare(previousCellWidth,  m_cellWidth) ||
        !qFuzzyCompare(previousCellHeight, m_cellHeight)) {
        emit metricsChanged();
        scheduleTerminalSizeSync();
    }
}

void QTermViewController::scheduleTerminalSizeSync()
{
    if (!m_terminal || m_viewWidth <= 0.0 || m_viewHeight <= 0.0)
        return;
    m_resizeDebounceTimer->start();
}

void QTermViewController::syncTerminalSize()
{
    if (!m_terminal || m_viewWidth <= 0.0 || m_viewHeight <= 0.0)
        return;
    const int columns = qMax(kMinimumColumns,
        static_cast<int>(std::floor(m_viewWidth  / qMax<qreal>(1.0, m_cellWidth))));
    const int rows = qMax(kMinimumRows,
        static_cast<int>(std::floor(m_viewHeight / qMax<qreal>(1.0, m_cellHeight))));
    m_terminal->setTerminalSize(columns, rows);
}

void QTermViewController::updateSelectionFromDrag(qreal x, qreal y)
{
    if (!m_terminal || m_selectionAnchorProjectionRow < 0 || m_selectionAnchorColumn < 0)
        return;
    // Map both anchor and drag point to **absolute projection rows** (viewport-top
    // row + row within the viewport). This keeps the anchor pinned to the content
    // and not drifting during auto-scroll; rowAtPosition clamps an out-of-bounds y
    // to the visible edge, and as the viewport-top row advances with scrollByLines
    // the drag point keeps extending deeper into the buffer.
    // The half-open interval [start, end) + grapheme alignment (wide chars taken
    // whole) is handled inside setSelectionDrag.
    const int dragProjectionRow = m_terminal->viewportTopProjectionRow() + rowAtPosition(y);
    const int dragColumn = columnAtPosition(x);
    m_terminal->setSelectionDrag(m_selectionAnchorProjectionRow, m_selectionAnchorColumn,
                                 dragProjectionRow, dragColumn);
}

} // namespace QTerm
