#include <QTerm/QTermTerminal.h>

#include <limits>

#include <QVariantMap>

#include "QTermCore.h"
#include "QTermSelectionModel.h"
#include "QTermInputEncoder.h"

#include <memory>
#include <QString>
#include <QtGlobal>

namespace QTerm {

namespace {
} // namespace

// static
void QTermTerminal::syncSurfaceCursor(QTermSurfaceModel &surfaceModel, QTermCore *core, int viewportTopProjectionRow)
{
    const QTermBuffer &buffer = core->buffer();
    const QTermCursorState cursorState = core->cursorState();
    const int cursorProjectionRow = buffer.visibleRowOffset() + cursorState.row;
    const bool cursorInViewport = cursorProjectionRow >= viewportTopProjectionRow &&
        cursorProjectionRow < viewportTopProjectionRow + core->rows();
    surfaceModel.setCursor(cursorInViewport ? cursorProjectionRow - viewportTopProjectionRow : 0,
                           cursorState.column,
                           core->modeState().cursorVisible && cursorInViewport,
                           static_cast<int>(core->modeState().cursorShape));
}

QTermTerminal::QTermTerminal(QObject *parent)
    : QObject(parent)
    , m_core(new QTermCore(this))
    , m_surfaceModel(this)
    , m_selectionModel(std::make_unique<QTermSelectionModel>())
{
    m_surfaceModel.setVisibleLinesProvider([this] {
        return m_core->buffer().viewportLineTexts(m_viewportTopProjectionRow, rows());
    });
    m_surfaceModel.setVisibleLineRunsProvider([this] {
        return m_core->buffer().viewportLineRuns(m_viewportTopProjectionRow, rows());
    });
    m_surfaceModel.setSelectionController(this);
    m_selectionModel->setTerminalSize(m_core->columns(), m_core->rows());
    m_viewportTopProjectionRow = qMax(0, m_core->buffer().projectionRowCount() - m_core->rows());
    m_selectionModel->setViewport(m_viewportTopProjectionRow);

    connect(m_core, &QTermCore::sizeChanged, this, [this]() {
        const int previousScrollOffset = scrollOffset();
        m_surfaceModel.setSize(m_core->columns(), m_core->rows());
        m_selectionModel->completeResize(m_core->buffer(), m_core->columns(), m_core->rows());
        clampViewportToBuffer();
        m_selectionModel->setViewport(m_viewportTopProjectionRow);
        m_selectionModel->refreshSelectionText(m_core->buffer());
        syncSurfaceSelection();
        syncSurfaceViewport();
        syncSurfaceCursor(m_surfaceModel, m_core, m_viewportTopProjectionRow);
        emit sizeChanged();
        if (scrollOffset() != previousScrollOffset) {
            emit viewportChanged();
        }
    });

    connect(m_core, &QTermCore::dumpPlainTextChanged, this, [this]() {
        const int previousScrollOffset = scrollOffset();
        clampViewportToBuffer();
        m_selectionModel->setViewport(m_viewportTopProjectionRow);
        m_selectionModel->refreshSelectionText(m_core->buffer());
        syncSurfaceSelection();
        syncSurfaceViewport();
        refreshSearch();  // new output shifted projection rows: re-find (no scroll)
        syncSurfaceCursor(m_surfaceModel, m_core, m_viewportTopProjectionRow);
        if (scrollOffset() != previousScrollOffset) {
            emit viewportChanged();
        }
    });

    connect(m_core, &QTermCore::cursorStateChanged, this, [this]() {
        syncSurfaceCursor(m_surfaceModel, m_core, m_viewportTopProjectionRow);
    });

    connect(m_core, &QTermCore::bell, this, &QTermTerminal::bell);
    connect(m_core, &QTermCore::titleChanged, this, &QTermTerminal::setTitle);
    connect(m_core, &QTermCore::currentDirectoryChanged, this, &QTermTerminal::setCurrentDirectory);
    connect(m_core, &QTermCore::shellZoneChanged, this, &QTermTerminal::shellZoneChanged);
    connect(m_core, &QTermCore::clipboardWriteRequested, this, &QTermTerminal::clipboardWriteRequested);
    connect(m_core, &QTermCore::modeStateChanged, this, &QTermTerminal::modeStateChanged);
    connect(m_core, &QTermCore::outboundData, this, &QTermTerminal::outboundData);

    m_surfaceModel.setSize(m_core->columns(), m_core->rows());
    syncSurfaceViewport();
    m_selectionModel->refreshSelectionText(m_core->buffer());
    syncSurfaceSelection();
    syncSurfaceCursor(m_surfaceModel, m_core, m_viewportTopProjectionRow);
}

QTermTerminal::~QTermTerminal() = default;

int QTermTerminal::rows() const noexcept
{
    return m_core->rows();
}

int QTermTerminal::columns() const noexcept
{
    return m_core->columns();
}

int QTermTerminal::maximumScrollbackLines() const noexcept
{
    return m_core->maximumScrollbackLines();
}

int QTermTerminal::scrollOffset() const noexcept
{
    return maxViewportTopProjectionRow() - m_viewportTopProjectionRow;
}

int QTermTerminal::viewportTopProjectionRow() const noexcept
{
    return m_viewportTopProjectionRow;
}

int QTermTerminal::maxScrollOffset() const noexcept
{
    return maxViewportTopProjectionRow();
}

QTermSession *QTermTerminal::session() const noexcept
{
    return m_session;
}

QString QTermTerminal::title() const
{
    return m_title;
}

QString QTermTerminal::currentDirectory() const
{
    return m_currentDirectory;
}

int QTermTerminal::shellZone() const noexcept
{
    return m_core->shellZone();
}

int QTermTerminal::lastExitCode() const noexcept
{
    return m_core->lastExitCode();
}

bool QTermTerminal::isMouseProtocolActive() const noexcept
{
    return m_core->modeState().mouseTracking != MouseTracking::Disabled;
}

bool QTermTerminal::isHoverTrackingActive() const noexcept
{
    return m_core->modeState().mouseTracking == MouseTracking::AnyEvent;
}

bool QTermTerminal::isButtonTrackingActive() const noexcept
{
    const MouseTracking mt = m_core->modeState().mouseTracking;
    return mt == MouseTracking::Button || mt == MouseTracking::AnyEvent;
}

QTermSurfaceModel *QTermTerminal::surfaceModel() noexcept
{
    return &m_surfaceModel;
}

QString QTermTerminal::hyperlinkUrl(int id) const
{
    return m_core->hyperlinkUrl(id);
}

QString QTermTerminal::dumpPlainText() const
{
    return m_core->dumpPlainText();
}

QByteArray QTermTerminal::dumpAnsi(int maxLines) const
{
    return m_core->dumpAnsi(maxLines);
}

void QTermTerminal::clear()
{
    m_core->clear();
    clearSelection();
}

void QTermTerminal::feedText(const QString &text)
{
    m_core->writePlainText(text);
}

void QTermTerminal::setMaximumScrollbackLines(int maximumScrollbackLines)
{
    const int previousScrollOffset = scrollOffset();
    const int boundedMaximum = qMax(0, maximumScrollbackLines);
    if (m_core->maximumScrollbackLines() == boundedMaximum) {
        return;
    }

    m_core->setMaximumScrollbackLines(boundedMaximum);
    clampViewportToBuffer();
    m_selectionModel->setViewport(m_viewportTopProjectionRow);
    m_selectionModel->refreshSelectionText(m_core->buffer());
    syncSurfaceSelection();
    syncSurfaceViewport();
    syncSurfaceCursor(m_surfaceModel, m_core, m_viewportTopProjectionRow);
    emit maximumScrollbackLinesChanged();
    if (scrollOffset() != previousScrollOffset) {
        emit viewportChanged();
    }
}

void QTermTerminal::setTerminalSize(int columns, int rows)
{
    const int boundedColumns = qMax(columns, 2);
    const int boundedRows = qMax(rows, 1);
    if (boundedColumns == m_core->columns() && boundedRows == m_core->rows()) {
        return;
    }

    m_selectionModel->prepareForResize(m_core->buffer());
    m_core->setTerminalSize(columns, rows);
}

void QTermTerminal::clearSelection()
{
    if (!m_selectionModel->snapshot().hasSelection) {
        return;
    }

    m_selectionModel->clearSelection();
    syncSurfaceSelection();
}

void QTermTerminal::setSelectionRange(int startRow, int startColumn, int endRow, int endColumn)
{
    const QTermSelectionSnapshot previousSnapshot = m_selectionModel->snapshot();
    m_selectionModel->setSelectionRange(startRow, startColumn, endRow, endColumn);
    m_selectionModel->refreshSelectionText(m_core->buffer());
    const QTermSelectionSnapshot &currentSnapshot = m_selectionModel->snapshot();
    if (previousSnapshot.hasSelection == currentSnapshot.hasSelection &&
        previousSnapshot.startRow == currentSnapshot.startRow &&
        previousSnapshot.startColumn == currentSnapshot.startColumn &&
        previousSnapshot.endRow == currentSnapshot.endRow &&
        previousSnapshot.endColumn == currentSnapshot.endColumn &&
        previousSnapshot.selectedText == currentSnapshot.selectedText) {
        return;
    }

    syncSurfaceSelection();
}

void QTermTerminal::setSelectionDrag(int anchorProjectionRow, int anchorColumn,
                                     int dragProjectionRow, int dragColumn)
{
    m_selectionModel->setSelectionFromDragCells(m_core->buffer(),
                                                anchorProjectionRow, anchorColumn,
                                                dragProjectionRow, dragColumn);
    m_selectionModel->refreshSelectionText(m_core->buffer());
    syncSurfaceSelection();
}

void QTermTerminal::selectAll()
{
    const QTermBuffer &buffer = m_core->buffer();
    const int projectionRowCount = buffer.projectionRowCount();
    if (projectionRowCount <= 0) {
        clearSelection();
        return;
    }

    // Stop at the last row that actually has content. A terminal buffer is always
    // at least one screen tall, so the rows below the output are real but blank —
    // selecting them too would make Select All + Copy yield a pile of trailing
    // newlines, which is the classic annoyance this avoids.
    int lastRow = projectionRowCount - 1;
    while (lastRow >= 0 && buffer.projectionLineAt(lastRow).plainText().trimmed().isEmpty()) {
        --lastRow;
    }
    if (lastRow < 0) {
        // Nothing but blanks in the whole buffer: there is nothing to select.
        clearSelection();
        return;
    }

    // Reuse the drag path rather than setSelectionRange(): the latter normalizes
    // against the viewport size, so it can never reach past the visible screen
    // into the scrollback.
    const int lastColumn = buffer.projectionLineAt(lastRow).columnTexts().size();
    m_selectionModel->setSelectionFromDragCells(buffer, 0, 0, lastRow, lastColumn);
    m_selectionModel->refreshSelectionText(buffer);
    syncSurfaceSelection();
}

void QTermTerminal::selectWordAt(int row, int column)
{
    m_selectionModel->selectWordAt(m_core->buffer(), row, column);
    m_selectionModel->refreshSelectionText(m_core->buffer());
    syncSurfaceSelection();
}

void QTermTerminal::selectLogicalLineAt(int row)
{
    m_selectionModel->selectLogicalLineAt(m_core->buffer(), row);
    m_selectionModel->refreshSelectionText(m_core->buffer());
    syncSurfaceSelection();
}

void QTermTerminal::scrollByLines(int deltaRows)
{
    const int previousScrollOffset = scrollOffset();
    const int nextViewportTop = qBound(0,
                                       m_viewportTopProjectionRow - deltaRows,
                                       maxViewportTopProjectionRow());
    if (nextViewportTop == m_viewportTopProjectionRow) {
        return;
    }

    m_viewportTopProjectionRow = nextViewportTop;
    m_viewportPinnedToBottom = m_viewportTopProjectionRow == maxViewportTopProjectionRow();
    m_selectionModel->setViewport(m_viewportTopProjectionRow);
    m_selectionModel->refreshSelectionText(m_core->buffer());
    syncSurfaceSelection();
    syncSurfaceViewport();
    syncSurfaceSearch();
    syncSurfaceCursor(m_surfaceModel, m_core, m_viewportTopProjectionRow);
    if (scrollOffset() != previousScrollOffset) {
        emit viewportChanged();
    }
}

void QTermTerminal::scrollToBottom()
{
    const int previousScrollOffset = scrollOffset();
    m_viewportPinnedToBottom = true;
    clampViewportToBuffer();
    m_selectionModel->setViewport(m_viewportTopProjectionRow);
    m_selectionModel->refreshSelectionText(m_core->buffer());
    syncSurfaceSelection();
    syncSurfaceViewport();
    syncSurfaceSearch();
    syncSurfaceCursor(m_surfaceModel, m_core, m_viewportTopProjectionRow);
    if (scrollOffset() != previousScrollOffset) {
        emit viewportChanged();
    }
}

// ── In-buffer search ───────────────────────────────────────────────────────

int QTermTerminal::searchMatchCount() const noexcept
{
    return int(m_searchMatches.size());
}

int QTermTerminal::searchCurrentIndex() const noexcept
{
    return m_searchCurrent < 0 ? 0 : m_searchCurrent + 1; // 1-based; 0 = none
}

int QTermTerminal::search(const QString &query, bool caseSensitive)
{
    m_searchQuery = query;
    m_searchCaseSensitive = caseSensitive;
    m_searchMatches.clear();
    m_searchCurrent = -1;

    if (!query.isEmpty()) {
        const Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive
                                                     : Qt::CaseInsensitive;
        const QTermBuffer &buffer = m_core->buffer();
        const int total = buffer.projectionRowCount();
        for (int row = 0; row < total; ++row) {
            const QString text = buffer.projectionLineAt(row).plainText();
            int from = 0;
            for (;;) {
                const int idx = text.indexOf(query, from, cs);
                if (idx < 0)
                    break;
                // Column == char index (exact for ASCII; wide-char column
                // precision is a future refinement).
                m_searchMatches.append({ row, idx, int(query.size()) });
                from = idx + int(query.size());
            }
        }
        // Pick the match nearest the current viewport top as the starting one.
        if (!m_searchMatches.isEmpty()) {
            m_searchCurrent = 0;
            int best = std::numeric_limits<int>::max();
            for (int i = 0; i < m_searchMatches.size(); ++i) {
                const int d = qAbs(m_searchMatches[i].projectionRow
                                   - m_viewportTopProjectionRow);
                if (d < best) {
                    best = d;
                    m_searchCurrent = i;
                }
            }
        }
    }

    scrollMatchIntoView();
    syncSurfaceSearch();
    emit searchChanged();
    return int(m_searchMatches.size());
}

void QTermTerminal::refreshSearch()
{
    if (m_searchQuery.isEmpty())
        return;
    const int prevCount = int(m_searchMatches.size());
    const int prevCurrent = m_searchCurrent;

    m_searchMatches.clear();
    const Qt::CaseSensitivity cs = m_searchCaseSensitive ? Qt::CaseSensitive
                                                         : Qt::CaseInsensitive;
    const QTermBuffer &buffer = m_core->buffer();
    const int total = buffer.projectionRowCount();
    for (int row = 0; row < total; ++row) {
        const QString text = buffer.projectionLineAt(row).plainText();
        int from = 0;
        for (;;) {
            const int idx = text.indexOf(m_searchQuery, from, cs);
            if (idx < 0)
                break;
            m_searchMatches.append({ row, idx, int(m_searchQuery.size()) });
            from = idx + int(m_searchQuery.size());
        }
    }
    // Keep the current index where possible (clamp); no scroll — output is
    // flowing, jumping the viewport would fight the user.
    if (m_searchMatches.isEmpty())
        m_searchCurrent = -1;
    else
        m_searchCurrent = qBound(0, prevCurrent, int(m_searchMatches.size()) - 1);

    syncSurfaceSearch();
    if (int(m_searchMatches.size()) != prevCount || m_searchCurrent != prevCurrent)
        emit searchChanged();
}

void QTermTerminal::findNext()
{
    if (m_searchMatches.isEmpty())
        return;
    m_searchCurrent = (m_searchCurrent + 1) % m_searchMatches.size();
    scrollMatchIntoView();
    syncSurfaceSearch();
    emit searchChanged();
}

void QTermTerminal::findPrevious()
{
    if (m_searchMatches.isEmpty())
        return;
    m_searchCurrent = (m_searchCurrent - 1 + int(m_searchMatches.size()))
                      % int(m_searchMatches.size());
    scrollMatchIntoView();
    syncSurfaceSearch();
    emit searchChanged();
}

void QTermTerminal::clearSearch()
{
    if (m_searchMatches.isEmpty() && m_searchQuery.isEmpty())
        return;
    m_searchQuery.clear();
    m_searchMatches.clear();
    m_searchCurrent = -1;
    syncSurfaceSearch();
    emit searchChanged();
}

void QTermTerminal::scrollMatchIntoView()
{
    if (m_searchCurrent < 0 || m_searchCurrent >= m_searchMatches.size())
        return;
    const int matchRow = m_searchMatches[m_searchCurrent].projectionRow;
    const int top = m_viewportTopProjectionRow;
    const int bottom = top + rows() - 1;
    if (matchRow >= top && matchRow <= bottom)
        return; // already visible

    // Center the match in the viewport, clamped to the buffer.
    const int desiredTop = qBound(0, matchRow - rows() / 2,
                                  maxViewportTopProjectionRow());
    if (desiredTop == m_viewportTopProjectionRow)
        return;
    const int previousScrollOffset = scrollOffset();
    m_viewportTopProjectionRow = desiredTop;
    m_viewportPinnedToBottom = m_viewportTopProjectionRow == maxViewportTopProjectionRow();
    m_selectionModel->setViewport(m_viewportTopProjectionRow);
    m_selectionModel->refreshSelectionText(m_core->buffer());
    syncSurfaceSelection();
    syncSurfaceViewport();
    syncSurfaceSearch();
    syncSurfaceCursor(m_surfaceModel, m_core, m_viewportTopProjectionRow);
    if (scrollOffset() != previousScrollOffset)
        emit viewportChanged();
}

void QTermTerminal::syncSurfaceSearch()
{
    // Project matches (projection rows) onto the current viewport and hand the
    // visible ones to the surface model in viewport-row coordinates.
    QVariantList highlights;
    const int top = m_viewportTopProjectionRow;
    const int viewRows = rows();
    for (int i = 0; i < m_searchMatches.size(); ++i) {
        const SearchMatch &m = m_searchMatches[i];
        const int viewRow = m.projectionRow - top;
        if (viewRow < 0 || viewRow >= viewRows)
            continue;
        QVariantMap h;
        h.insert(QStringLiteral("row"), viewRow);
        h.insert(QStringLiteral("startColumn"), m.startColumn);
        h.insert(QStringLiteral("endColumn"), m.startColumn + m.length);
        h.insert(QStringLiteral("current"), i == m_searchCurrent);
        highlights.append(h);
    }
    m_surfaceModel.setSearchHighlights(highlights);
}

void QTermTerminal::sendKey(int key, const QString &text, Qt::KeyboardModifiers modifiers)
{
    m_core->sendKey(key, text, modifiers);
}

void QTermTerminal::sendPaste(const QString &text)
{
    m_core->sendPaste(text);
}

void QTermTerminal::sendMouse(int row, int column, int button, int modifiers, bool isPress, bool isMotion)
{
    const Qt::MouseButton qtButton = static_cast<Qt::MouseButton>(button);
    const Qt::KeyboardModifiers qtModifiers = static_cast<Qt::KeyboardModifiers>(modifiers);
    const QByteArray mouseSequence = QTermInputEncoder::encodeMouse(
        row, column, qtButton, qtModifiers, isPress, m_core->modeState(), isMotion);

    if (!mouseSequence.isEmpty()) {
        emit outboundData(mouseSequence);
    }
}

void QTermTerminal::setSession(QTermSession *session)
{
    if (m_session == session) {
        return;
    }

    QObject::disconnect(m_sessionDataConnection);
    QObject::disconnect(m_sessionDestroyedConnection);
    QObject::disconnect(m_coreOutboundConnection);
    QObject::disconnect(m_sizeToSessionResizeConnection);

    m_sessionUtf8Decoder = QStringDecoder(QStringDecoder::Utf8);

    m_session = session;

    if (m_session) {
        m_sessionDataConnection = connect(m_session, &QTermSession::dataReceived, this, [this](const QByteArray &data) {
            m_core->writePlainText(m_sessionUtf8Decoder(data));
        });
        m_sessionDestroyedConnection = connect(m_session, &QObject::destroyed, this, [this]() {
            QObject::disconnect(m_sessionDataConnection);
            QObject::disconnect(m_sessionDestroyedConnection);
            QObject::disconnect(m_coreOutboundConnection);
            QObject::disconnect(m_sizeToSessionResizeConnection);
            m_sessionUtf8Decoder = QStringDecoder(QStringDecoder::Utf8);
            m_session = nullptr;
            emit sessionChanged();
        });
        m_coreOutboundConnection = connect(this, &QTermTerminal::outboundData, m_session, &QTermSession::writeData);
        m_sizeToSessionResizeConnection = connect(this, &QTermTerminal::sizeChanged, this, [this]() {
            if (m_session) {
                m_session->resize(columns(), rows());
            }
        });
        m_session->resize(columns(), rows());
    }

    emit sessionChanged();
}

void QTermTerminal::setTitle(const QString &title)
{
    if (m_title == title) {
        return;
    }

    m_title = title;
    emit titleChanged();
}

void QTermTerminal::setCurrentDirectory(const QString &url)
{
    if (m_currentDirectory == url) {
        return;
    }

    m_currentDirectory = url;
    emit currentDirectoryChanged();
}

void QTermTerminal::syncSurfaceSelection()
{
    const QTermSelectionSnapshot &snapshot = m_selectionModel->snapshot();
    m_surfaceModel.setSelectionSnapshot(snapshot.hasSelection,
                                        snapshot.startRow,
                                        snapshot.startColumn,
                                        snapshot.endRow,
                                        snapshot.endColumn,
                                        snapshot.selectedText);
}

int QTermTerminal::maxViewportTopProjectionRow() const noexcept
{
    return qMax(0, m_core->buffer().projectionRowCount() - rows());
}

void QTermTerminal::clampViewportToBuffer()
{
    const int maxTop = maxViewportTopProjectionRow();
    if (m_viewportPinnedToBottom) {
        m_viewportTopProjectionRow = maxTop;
    } else {
        m_viewportTopProjectionRow = qBound(0, m_viewportTopProjectionRow, maxTop);
        if (m_viewportTopProjectionRow == maxTop) {
            m_viewportPinnedToBottom = true;
        }
    }
}

void QTermTerminal::syncSurfaceViewport()
{
    QTermBuffer &buf = m_core->buffer();
    m_surfaceModel.markVisibleLinesDirty();

    // If the viewport offset changed since last sync, or the buffer is fully dirty,
    // do a full rebuild — the incremental dirty-row set only covers buffer-write
    // changes and is meaningless when we are showing a different slice of history.
    const bool viewportMoved = (m_viewportTopProjectionRow != m_lastSyncedViewportTop);
    m_lastSyncedViewportTop = m_viewportTopProjectionRow;

    if (buf.allRowsDirty() || viewportMoved) {
        m_surfaceModel.markVisibleLineRunsDirty();
        buf.clearDirtyRows();
        return;
    }

    // Incremental update: only rebuild runs for dirty visible rows.
    const QVector<bool> &dirty = buf.dirtyRows();
    const int visOffset = buf.visibleRowOffset();
    const int rowCount = rows();
    QVector<int> dirtyViewportRows;
    QVariantList dirtyRowRuns;

    for (int visRow = 0; visRow < dirty.size() && visRow < rowCount; ++visRow) {
        if (!dirty.at(visRow)) {
            continue;
        }
        const int viewportSlot = visOffset + visRow - m_viewportTopProjectionRow;
        if (viewportSlot < 0 || viewportSlot >= rowCount) {
            continue;
        }
        const int projRow = visOffset + visRow;
        dirtyViewportRows.append(viewportSlot);
        if (projRow < buf.projectionRowCount()) {
            dirtyRowRuns.append(QVariant::fromValue(buf.projectionLineAt(projRow).styleRuns()));
        } else {
            dirtyRowRuns.append(QVariant::fromValue(QVariantList()));
        }
    }

    if (!dirtyViewportRows.isEmpty()) {
        m_surfaceModel.setVisibleLineRunsPartial(dirtyViewportRows, dirtyRowRuns);
    }
    buf.clearDirtyRows();
}

} // namespace QTerm