#include "QTermBuffer.h"

#include <QStringList>
#include <QtGlobal>

namespace QTerm {

namespace {

struct LogicalLineProjection
{
    QVector<QTermCell> cells;
    int displayColumns = 0;
    bool containsCursor = false;
    int cursorDisplayOffset = 0;
};

int lastRelevantColumn(const QTermLine &line)
{
    for (int column = line.columns() - 1; column >= 0; --column) {
        const QTermCell &cell = line.cellAt(column);
        if (cell.continuation) {
            continue;
        }

        if (!cell.text.isEmpty()) {
            return column + qMax(1, cell.width);
        }
    }

    return 0;
}

void appendLineCells(LogicalLineProjection &logicalLine, const QTermLine &line, int includeColumns)
{
    const int boundedColumns = qBound(0, includeColumns, line.columns());
    for (int column = 0; column < boundedColumns; ++column) {
        const QTermCell &cell = line.cellAt(column);
        if (cell.continuation) {
            continue;
        }

        logicalLine.cells.append(cell);
        logicalLine.displayColumns += qMax(1, cell.width);
    }
}

QVector<LogicalLineProjection> snapshotLogicalLines(const QVector<QTermLine> &historyLines,
                                                    const QVector<QTermLine> &visibleLines,
                                                    int cursorRow,
                                                    int cursorColumn)
{
    QVector<QTermLine> projection;
    projection.reserve(historyLines.size() + visibleLines.size());
    projection.append(historyLines);
    projection.append(visibleLines);

    int firstRelevantIndex = -1;
    int lastRelevantIndex = -1;
    for (int index = 0; index < projection.size(); ++index) {
        if (lastRelevantColumn(projection.at(index)) > 0) {
            if (firstRelevantIndex < 0) {
                firstRelevantIndex = index;
            }
            lastRelevantIndex = index;
        }
    }

    QVector<LogicalLineProjection> logicalLines;
    LogicalLineProjection currentLine;
    const int cursorProjectionRow = historyLines.size() + qBound(0, cursorRow, qMax(0, visibleLines.size() - 1));
    const int startIndex = firstRelevantIndex >= 0 ? qMin(firstRelevantIndex, cursorProjectionRow) : cursorProjectionRow;
    const int endIndex = lastRelevantIndex >= 0 ? qMax(lastRelevantIndex, cursorProjectionRow) : cursorProjectionRow;

    for (int index = startIndex; index <= endIndex && index < projection.size(); ++index) {
        const QTermLine &line = projection.at(index);
        const bool isCursorRow = !visibleLines.isEmpty() && index == cursorProjectionRow;
        const int includeColumns = line.wrappedToNextLine()
            ? line.columns()
            : qMax(lastRelevantColumn(line), isCursorRow ? qBound(0, cursorColumn, line.columns()) : 0);

        if (isCursorRow) {
            currentLine.containsCursor = true;
            currentLine.cursorDisplayOffset = currentLine.displayColumns + qBound(0, cursorColumn, line.columns());
        }

        appendLineCells(currentLine, line, includeColumns);

        if (!line.wrappedToNextLine()) {
            logicalLines.append(currentLine);
            currentLine = LogicalLineProjection();
        }
    }

    if (!currentLine.cells.isEmpty() || currentLine.containsCursor) {
        logicalLines.append(currentLine);
    }

    if (logicalLines.isEmpty()) {
        logicalLines.append(LogicalLineProjection());
    }

    return logicalLines;
}

} // namespace

QTermBuffer::QTermBuffer(int columns, int rows)
    : m_columns(columns)
    , m_rows(rows)
    , m_dirtyRows(rows, false)
    , m_allRowsDirty(true)
{
    m_visibleLines.reserve(rows);
    for (int row = 0; row < rows; ++row) {
        appendEmptyVisibleLine();
    }
}

int QTermBuffer::rows() const noexcept
{
    return m_rows;
}

int QTermBuffer::columns() const noexcept
{
    return m_columns;
}

int QTermBuffer::historyLineCount() const noexcept
{
    return m_historyLines.size();
}

int QTermBuffer::projectionRowCount() const noexcept
{
    return m_historyLines.size() + m_visibleLines.size();
}

int QTermBuffer::visibleRowOffset() const noexcept
{
    return m_historyLines.size();
}

QTermCursorState QTermBuffer::resize(int columns, int rows, const QTermCursorState &cursorState)
{
    const QVector<LogicalLineProjection> logicalLines = snapshotLogicalLines(m_historyLines,
                                                                             m_visibleLines,
                                                                             cursorState.row,
                                                                             cursorState.column);

    QVector<QTermLine> reflowedLines;
    reflowedLines.reserve(logicalLines.size() + rows);
    QTermCursorState reflowedCursor;
    bool cursorResolved = false;
    int cursorLogicalLineStartRow = 0;

    for (const LogicalLineProjection &logicalLine : logicalLines) {
        const int logicalStartRow = reflowedLines.size();

        if (logicalLine.cells.isEmpty()) {
            reflowedLines.append(QTermLine(columns));
        } else {
            QTermLine currentLine(columns);
            int currentColumn = 0;

            for (const QTermCell &cell : logicalLine.cells) {
                const int cellWidth = qMax(1, cell.width);
                if (currentColumn > 0 && currentColumn + cellWidth > columns) {
                    currentLine.setWrappedToNextLine(true);
                    reflowedLines.append(currentLine);
                    currentLine = QTermLine(columns);
                    currentColumn = 0;
                }

                currentLine.setCharacter(currentColumn, cell.text, cellWidth, cell.attributes);
                currentColumn += cellWidth;
            }

            reflowedLines.append(currentLine);
        }

        if (logicalLine.containsCursor) {
            const int logicalCursorRow = logicalLine.cursorDisplayOffset / columns;
            const int logicalCursorColumn = logicalLine.cursorDisplayOffset % columns;
            const int lastLogicalRow = reflowedLines.size() - 1;
            reflowedCursor.row = qMin(logicalStartRow + logicalCursorRow, lastLogicalRow);
            reflowedCursor.column = logicalCursorColumn;
            cursorResolved = true;
            cursorLogicalLineStartRow = logicalStartRow;
        }
    }

    const int oldRows = m_rows;
    m_columns = columns;
    m_rows = rows;

    if (!cursorResolved) {
        reflowedCursor = QTermCursorState();
    }

    // Maintain the cursor's relative distance from the bottom of the visible
    // area across resize — consistent with xterm / iTerm2 behaviour.
    // When narrowing, lines above the cursor may split and flow into history;
    // the cursor stays anchored to the same logical content near the bottom.
    // Lines that must overflow (reflowedLines.size() > rows) always go to
    // history first; on top of that we push just enough extra rows so the
    // cursor ends up at targetCursorVisibleRow.
    //
    // Hard constraint: never push any part of the cursor's own logical line
    // into history (cursorLogicalLineStartRow is the first physical row of
    // that logical line in the reflowed array).
    const int defaultHistoryCount = qMax(0, reflowedLines.size() - rows);
    const int distanceFromBottom = qMax(0, (oldRows - 1) - cursorState.row);
    const int targetCursorVisibleRow = qBound(0, (rows - 1) - distanceFromBottom, rows - 1);
    const int rawMinHistoryForCursor = cursorResolved
        ? qMax(0, reflowedCursor.row - targetCursorVisibleRow)
        : 0;
    const int minHistoryForCursor = cursorResolved
        ? qMin(rawMinHistoryForCursor, cursorLogicalLineStartRow)
        : rawMinHistoryForCursor;
    const int historyCount = qMax(defaultHistoryCount, minHistoryForCursor);
    m_historyLines = reflowedLines.first(historyCount);
    m_visibleLines = reflowedLines.mid(historyCount);

    // Trim any rows beyond `rows` that may appear when historyCount was reduced.
    // This can only happen when the logical line fits entirely in visible, so
    // trimming only removes empty tail rows appended after the content.
    if (m_visibleLines.size() > rows) {
        m_visibleLines.resize(rows);
    }

    while (m_visibleLines.size() < rows) {
        appendEmptyVisibleLine();
    }

    if (m_visibleLines.isEmpty()) {
        appendEmptyVisibleLine();
    }

    markAllRowsDirty();
    reflowedCursor.row = qBound(0, reflowedCursor.row - historyCount, qMax(0, rows - 1));
    reflowedCursor.column = qBound(0, reflowedCursor.column, qMax(0, columns - 1));
    return reflowedCursor;
}

void QTermBuffer::clearHistory()
{
    m_historyLines.clear();
}

void QTermBuffer::clear()
{
    m_historyLines.clear();
    m_visibleLines.clear();
    m_visibleLines.reserve(m_rows);
    for (int row = 0; row < m_rows; ++row) {
        appendEmptyVisibleLine();
    }
    markAllRowsDirty();
}

void QTermBuffer::clearVisible()
{
    for (QTermLine &line : m_visibleLines) {
        line.clear();
    }
    markAllRowsDirty();
}

void QTermBuffer::clearVisibleFrom(int row, int column)
{
    if (row < 0 || row >= m_visibleLines.size()) {
        return;
    }

    m_visibleLines[row].clearToEnd(column);
    markDirtyRow(row);
    for (int index = row + 1; index < m_visibleLines.size(); ++index) {
        m_visibleLines[index].clear();
        markDirtyRow(index);
    }
}

void QTermBuffer::clearVisibleTo(int row, int column)
{
    if (row < 0 || row >= m_visibleLines.size()) {
        return;
    }

    for (int index = 0; index < row; ++index) {
        m_visibleLines[index].clear();
        markDirtyRow(index);
    }
    m_visibleLines[row].clearToColumn(column);
    markDirtyRow(row);
}

int QTermBuffer::severPredecessorWrapChain(int visibleRow)
{
    // Walk backwards through the projection (history + visible) from the row
    // immediately preceding visibleRow. For each physical row that is part of
    // the same wrap chain (wrappedToNextLine == true on its predecessor),
    // clear the row's content and sever its wrap flag. Returns the visible row
    // index where the chain started (the first cleared row), so the caller can
    // reposition the cursor there to avoid leaving orphaned blank rows.
    //
    // QTermBuffer::resize guarantees that the cursor's logical line is kept
    // entirely in the visible region when it fits, so we only need to look at
    // visible rows here; the walk still starts at visibleRow - 1 and goes to 0.
    int chainStartVisibleRow = visibleRow;
    int projectionRow = m_historyLines.size() + visibleRow - 1;
    while (projectionRow >= 0) {
        if (projectionRow < m_historyLines.size()) {
            QTermLine &line = m_historyLines[projectionRow];
            if (!line.wrappedToNextLine()) {
                break;
            }
            line.clear();  // clears content and sets wrappedToNextLine = false
        } else {
            QTermLine &line = m_visibleLines[projectionRow - m_historyLines.size()];
            if (!line.wrappedToNextLine()) {
                break;
            }
            line.clear();  // clears content and sets wrappedToNextLine = false
            chainStartVisibleRow = projectionRow - m_historyLines.size();
            markDirtyRow(chainStartVisibleRow);
        }
        --projectionRow;
    }
    return chainStartVisibleRow;
}

void QTermBuffer::scrollUp()
{
    m_historyLines.append(m_visibleLines.takeFirst());
    appendEmptyVisibleLine();
    markAllRowsDirty();
}

void QTermBuffer::insertLines(int row, int count, int scrollTop, int scrollBottom)
{
    if (row < scrollTop || row > scrollBottom || count <= 0) {
        return;
    }

    const int boundedCount = qMin(count, scrollBottom - row + 1);
    for (int index = 0; index < boundedCount; ++index) {
        m_visibleLines.insert(row, QTermLine(m_columns));
        m_visibleLines.removeAt(scrollBottom + 1);
    }
    markAllRowsDirty();
}

void QTermBuffer::deleteLines(int row, int count, int scrollTop, int scrollBottom)
{
    if (row < scrollTop || row > scrollBottom || count <= 0) {
        return;
    }

    const int boundedCount = qMin(count, scrollBottom - row + 1);
    for (int index = 0; index < boundedCount; ++index) {
        m_visibleLines.removeAt(row);
        m_visibleLines.insert(scrollBottom, QTermLine(m_columns));
    }
    markAllRowsDirty();
}

QTermLine &QTermBuffer::lineAt(int row)
{
    markDirtyRow(row);
    return m_visibleLines[row];
}

const QTermLine &QTermBuffer::lineAt(int row) const
{
    return m_visibleLines.at(row);
}

const QTermLine &QTermBuffer::projectionLineAt(int projectionRow) const
{
    if (projectionRow < m_historyLines.size()) {
        return m_historyLines.at(projectionRow);
    }

    return m_visibleLines.at(projectionRow - m_historyLines.size());
}

QStringList QTermBuffer::viewportLineTexts(int topProjectionRow, int rowCount) const
{
    QStringList lines;
    lines.reserve(qMax(0, rowCount));

    for (int row = 0; row < rowCount; ++row) {
        const int projectionRow = topProjectionRow + row;
        if (projectionRow >= 0 && projectionRow < projectionRowCount()) {
            lines.append(projectionLineAt(projectionRow).plainText());
        } else {
            lines.append(QString());
        }
    }

    return lines;
}

QVariantList QTermBuffer::viewportLineRuns(int topProjectionRow, int rowCount) const
{
    QVariantList lines;
    lines.reserve(qMax(0, rowCount));

    for (int row = 0; row < rowCount; ++row) {
        const int projectionRow = topProjectionRow + row;
        if (projectionRow >= 0 && projectionRow < projectionRowCount()) {
            lines.append(QVariant::fromValue(projectionLineAt(projectionRow).styleRuns()));
        } else {
            lines.append(QVariant::fromValue(QVariantList()));
        }
    }

    return lines;
}

QStringList QTermBuffer::visibleLineTexts() const
{
    return viewportLineTexts(visibleRowOffset(), m_visibleLines.size());
}

QVariantList QTermBuffer::visibleLineRuns() const
{
    return viewportLineRuns(visibleRowOffset(), m_visibleLines.size());
}

QString QTermBuffer::debugPlainText() const
{
    QVector<QTermLine> projection;
    projection.reserve(m_historyLines.size() + m_visibleLines.size());
    projection.append(m_historyLines);
    projection.append(m_visibleLines);

    int lastRelevantIndex = -1;
    for (int index = 0; index < projection.size(); ++index) {
        if (!projection.at(index).plainText().isEmpty()) {
            lastRelevantIndex = index;
        }
    }

    if (lastRelevantIndex < 0) {
        return QString();
    }

    QString text;
    for (int index = 0; index <= lastRelevantIndex; ++index) {
        if (index > 0 && !projection.at(index - 1).wrappedToNextLine()) {
            text.append(u'\n');
        }
        text.append(projection.at(index).plainText());
    }

    return text;
}

void QTermBuffer::appendEmptyVisibleLine()
{
    m_visibleLines.append(QTermLine(m_columns));
}

void QTermBuffer::markDirtyRow(int visibleRow)
{
    if (m_allRowsDirty) {
        return;
    }
    if (visibleRow >= 0 && visibleRow < m_dirtyRows.size()) {
        m_dirtyRows[visibleRow] = true;
    }
}

void QTermBuffer::markAllRowsDirty()
{
    m_allRowsDirty = true;
    m_dirtyRows.fill(false);
}

void QTermBuffer::clearDirtyRows()
{
    m_allRowsDirty = false;
    if (m_dirtyRows.size() != m_rows) {
        m_dirtyRows.assign(m_rows, false);
    } else {
        m_dirtyRows.fill(false);
    }
}

} // namespace QTerm