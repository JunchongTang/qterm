#include "QTermSelectionModel.h"

#include "QTermBuffer.h"

#include <optional>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace QTerm {

namespace {

struct NormalizedSelectionRange
{
    bool hasSelection = false;
    int startRow = 0;
    int startColumn = 0;
    int endRow = 0;
    int endColumn = 0;
};

enum class TokenClass {
    Empty,
    Space,
    Word,
    Punctuation,
};

struct LogicalColumnRef
{
    int row = 0;
    int column = 0;
    QString text;
};

struct LogicalRowSpan
{
    int startRow = 0;
    int endRow = 0;
};

struct ProjectionRowSpan
{
    int startProjectionRow = 0;
    int endProjectionRow = 0;
};

struct ProjectionEndpoint
{
    int projectionRow = 0;
    int column = 0;
};

NormalizedSelectionRange normalizeSelectionRange(int rows,
                                                 int columns,
                                                 int startRow,
                                                 int startColumn,
                                                 int endRow,
                                                 int endColumn)
{
    NormalizedSelectionRange range;

    const int boundedStartRow = qBound(0, startRow, qMax(0, rows - 1));
    const int boundedEndRow = qBound(0, endRow, qMax(0, rows - 1));
    const int boundedStartColumn = qBound(0, startColumn, columns);
    const int boundedEndColumn = qBound(0, endColumn, columns);

    const bool startBeforeEnd = boundedStartRow < boundedEndRow ||
        (boundedStartRow == boundedEndRow && boundedStartColumn <= boundedEndColumn);

    range.startRow = startBeforeEnd ? boundedStartRow : boundedEndRow;
    range.startColumn = startBeforeEnd ? boundedStartColumn : boundedEndColumn;
    range.endRow = startBeforeEnd ? boundedEndRow : boundedStartRow;
    range.endColumn = startBeforeEnd ? boundedEndColumn : boundedStartColumn;
    range.hasSelection = range.startRow != range.endRow || range.startColumn != range.endColumn;

    return range;
}

int graphemeEndColumn(const QStringList &lineColumns, int startColumn)
{
    int column = startColumn + 1;
    while (column < lineColumns.size() && lineColumns.at(column).isEmpty()) {
        ++column;
    }

    return column;
}

TokenClass classifyColumnText(const QString &columnText)
{
    if (columnText.isEmpty()) {
        return TokenClass::Empty;
    }

    if (columnText == QStringLiteral(" ")) {
        return TokenClass::Space;
    }

    const QChar firstCharacter = columnText.at(0);
    if (firstCharacter.isLetterOrNumber() ||
        firstCharacter == u'_' ||
        firstCharacter == u'-' ||
        firstCharacter == u'.' ||
        firstCharacter == u'/' ||
        firstCharacter == u'\\' ||
        firstCharacter == u':' ||
        firstCharacter == u'~' ||
        firstCharacter == u'=' ||
        firstCharacter == u'?' ||
        firstCharacter == u'&' ||
        firstCharacter == u'#' ||
        firstCharacter == u'%' ||
        firstCharacter == u'@' ||
        firstCharacter == u'+') {
        return TokenClass::Word;
    }

    return TokenClass::Punctuation;
}

LogicalRowSpan logicalLineRowSpan(const QTermBuffer &buffer, int row)
{
    LogicalRowSpan span{row, row};
    while (span.startRow > 0 && buffer.lineAt(span.startRow - 1).wrappedToNextLine()) {
        --span.startRow;
    }

    while (span.endRow < buffer.rows() - 1 && buffer.lineAt(span.endRow).wrappedToNextLine()) {
        ++span.endRow;
    }

    return span;
}

QVector<LogicalColumnRef> collectLogicalLineColumns(const QTermBuffer &buffer, int row)
{
    QVector<LogicalColumnRef> logicalColumns;
    const LogicalRowSpan rowSpan = logicalLineRowSpan(buffer, row);

    for (int currentRow = rowSpan.startRow; currentRow <= rowSpan.endRow; ++currentRow) {
        const QStringList lineColumns = buffer.lineAt(currentRow).columnTexts();
        for (int column = 0; column < lineColumns.size(); ++column) {
            logicalColumns.append(LogicalColumnRef{currentRow, column, lineColumns.at(column)});
        }
    }

    return logicalColumns;
}

QString textForLogicalRange(const QVector<LogicalColumnRef> &logicalColumns, int startIndex, int endIndex)
{
    QString text;

    for (int index = startIndex; index <= endIndex && index < logicalColumns.size(); ++index) {
        const QString &columnText = logicalColumns.at(index).text;
        if (!columnText.isEmpty()) {
            text.append(columnText);
        }
    }

    return text;
}

bool isUrlLikeSelectionText(const QString &text)
{
    if (text.startsWith(QStringLiteral("www."))) {
        return true;
    }

    const int colonIndex = text.indexOf(u':');
    if (colonIndex <= 0) {
        return false;
    }

    for (int index = 0; index < colonIndex; ++index) {
        const QChar character = text.at(index);
        if (!(character.isLetterOrNumber() || character == u'+' || character == u'-' || character == u'.')) {
            return false;
        }
    }

    return text.at(0).isLetter();
}

bool isTrimmedTrailingUrlPunctuation(const QString &text, int index)
{
    const QChar character = text.at(index);
    if (character == u'.' || character == u',' || character == u';' || character == u':' ||
        character == u'!' || character == u'?') {
        return true;
    }

    if ((character == u')' || character == u']' || character == u'}') && index > 0) {
        int openCount = 0;
        int closeCount = 0;
        const QChar openCharacter = character == u')' ? u'(' : (character == u']' ? u'[' : u'{');
        for (int cursor = 0; cursor <= index; ++cursor) {
            if (text.at(cursor) == openCharacter) {
                ++openCount;
            } else if (text.at(cursor) == character) {
                ++closeCount;
            }
        }

        return closeCount > openCount;
    }

    return false;
}

void trimUrlLikeSelectionBounds(const QVector<LogicalColumnRef> &logicalColumns, int &selectionStartIndex, int &selectionEndIndex)
{
    const QString selectionText = textForLogicalRange(logicalColumns, selectionStartIndex, selectionEndIndex);
    if (!isUrlLikeSelectionText(selectionText)) {
        return;
    }

    int trimCount = 0;
    for (int index = selectionText.size() - 1; index >= 0; --index) {
        if (!isTrimmedTrailingUrlPunctuation(selectionText, index)) {
            break;
        }

        ++trimCount;
    }

    while (trimCount > 0 && selectionEndIndex >= selectionStartIndex) {
        if (!logicalColumns.at(selectionEndIndex).text.isEmpty()) {
            --trimCount;
        }
        --selectionEndIndex;
    }
}

int lastRelevantColumn(const QStringList &lineColumns)
{
    for (int column = lineColumns.size() - 1; column >= 0; --column) {
        const QString &columnText = lineColumns.at(column);
        if (!columnText.isEmpty() && columnText != QStringLiteral(" ")) {
            return column + 1;
        }
    }

    return 0;
}

QVector<LogicalRowSpan> collectVisibleLogicalLineSpans(const QTermBuffer &buffer)
{
    QVector<LogicalRowSpan> spans;

    for (int row = 0; row < buffer.rows(); ++row) {
        const int startRow = row;
        while (row < buffer.rows() - 1 && buffer.lineAt(row).wrappedToNextLine()) {
            ++row;
        }

        spans.append(LogicalRowSpan{startRow, row});
    }

    return spans;
}

QVector<ProjectionRowSpan> collectProjectionLogicalLineSpans(const QTermBuffer &buffer)
{
    QVector<ProjectionRowSpan> spans;

    for (int projectionRow = 0; projectionRow < buffer.projectionRowCount(); ++projectionRow) {
        const int startProjectionRow = projectionRow;
        while (projectionRow < buffer.projectionRowCount() - 1 && buffer.projectionLineAt(projectionRow).wrappedToNextLine()) {
            ++projectionRow;
        }

        spans.append(ProjectionRowSpan{startProjectionRow, projectionRow});
    }

    return spans;
}

int logicalLineDisplayColumns(const QTermBuffer &buffer, const LogicalRowSpan &span)
{
    int totalColumns = 0;
    for (int row = span.startRow; row <= span.endRow; ++row) {
        if (row < span.endRow) {
            totalColumns += buffer.columns();
        } else {
            totalColumns += lastRelevantColumn(buffer.lineAt(row).columnTexts());
        }
    }

    return totalColumns;
}

int displayOffsetWithinLogicalLine(const QTermBuffer &buffer,
                                   const LogicalRowSpan &span,
                                   int row,
                                   int column)
{
    int offset = 0;
    for (int currentRow = span.startRow; currentRow < row; ++currentRow) {
        offset += currentRow < span.endRow
            ? buffer.columns()
            : lastRelevantColumn(buffer.lineAt(currentRow).columnTexts());
    }

    return offset + qMax(0, column);
}

QString selectionTextFromBuffer(const QTermBuffer &buffer, const QTermSelectionSnapshot &snapshot)
{
    QString selectionText;

    for (int row = snapshot.startRow; row <= snapshot.endRow; ++row) {
        if (row > snapshot.startRow && !buffer.lineAt(row - 1).wrappedToNextLine()) {
            selectionText.append(u'\n');
        }

        const QStringList lineColumns = buffer.lineAt(row).columnTexts();
        if (snapshot.startRow == snapshot.endRow) {
            for (int column = snapshot.startColumn; column < snapshot.endColumn && column < lineColumns.size(); ++column) {
                selectionText.append(lineColumns.at(column));
            }
            continue;
        }

        if (row == snapshot.startRow) {
            const int lineEnd = lastRelevantColumn(lineColumns);
            for (int column = snapshot.startColumn; column < lineEnd && column < lineColumns.size(); ++column) {
                selectionText.append(lineColumns.at(column));
            }
        } else if (row == snapshot.endRow) {
            for (int column = 0; column < snapshot.endColumn && column < lineColumns.size(); ++column) {
                selectionText.append(lineColumns.at(column));
            }
        } else {
            const int lineEnd = lastRelevantColumn(lineColumns);
            for (int column = 0; column < lineEnd && column < lineColumns.size(); ++column) {
                selectionText.append(lineColumns.at(column));
            }
        }
    }

    return selectionText;
}

QString selectionTextFromProjection(const QTermBuffer &buffer,
                                   const ProjectionEndpoint &start,
                                   const ProjectionEndpoint &end)
{
    QString selectionText;

    for (int projectionRow = start.projectionRow; projectionRow <= end.projectionRow; ++projectionRow) {
        if (projectionRow > start.projectionRow && !buffer.projectionLineAt(projectionRow - 1).wrappedToNextLine()) {
            selectionText.append(u'\n');
        }

        const QStringList lineColumns = buffer.projectionLineAt(projectionRow).columnTexts();
        if (start.projectionRow == end.projectionRow) {
            for (int column = start.column; column < end.column && column < lineColumns.size(); ++column) {
                selectionText.append(lineColumns.at(column));
            }
            continue;
        }

        if (projectionRow == start.projectionRow) {
            const int lineEnd = lastRelevantColumn(lineColumns);
            for (int column = start.column; column < lineEnd && column < lineColumns.size(); ++column) {
                selectionText.append(lineColumns.at(column));
            }
        } else if (projectionRow == end.projectionRow) {
            for (int column = 0; column < end.column && column < lineColumns.size(); ++column) {
                selectionText.append(lineColumns.at(column));
            }
        } else {
            const int lineEnd = lastRelevantColumn(lineColumns);
            for (int column = 0; column < lineEnd && column < lineColumns.size(); ++column) {
                selectionText.append(lineColumns.at(column));
            }
        }
    }

    return selectionText;
}

int projectionRowDisplayColumns(const QTermBuffer &buffer, const ProjectionRowSpan &span, int projectionRow)
{
    if (projectionRow < span.endProjectionRow) {
        return buffer.columns();
    }

    return lastRelevantColumn(buffer.projectionLineAt(projectionRow).columnTexts());
}

int displayOffsetWithinProjectionLogicalLine(const QTermBuffer &buffer,
                                             const ProjectionRowSpan &span,
                                             int projectionRow,
                                             int column)
{
    int offset = 0;
    for (int currentProjectionRow = span.startProjectionRow; currentProjectionRow < projectionRow; ++currentProjectionRow) {
        offset += projectionRowDisplayColumns(buffer, span, currentProjectionRow);
    }

    return offset + qMax(0, column);
}

QTermSelectionModel::LogicalEndpointAnchor captureLogicalAnchor(const QTermBuffer &buffer,
                                                                const QVector<ProjectionRowSpan> &spans,
                                                                int row,
                                                                int column)
{
    const int boundedRow = qBound(0, row, qMax(0, buffer.rows() - 1));
    const int projectionRow = buffer.visibleRowOffset() + boundedRow;
    for (int index = 0; index < spans.size(); ++index) {
        const ProjectionRowSpan &span = spans.at(index);
        if (projectionRow < span.startProjectionRow || projectionRow > span.endProjectionRow) {
            continue;
        }

        return QTermSelectionModel::LogicalEndpointAnchor{
            index,
            displayOffsetWithinProjectionLogicalLine(buffer, span, projectionRow, column)
        };
    }

    return QTermSelectionModel::LogicalEndpointAnchor();
}

ProjectionEndpoint mapLogicalAnchorToProjectionEndpoint(const QTermBuffer &buffer,
                                                        const QVector<ProjectionRowSpan> &spans,
                                                        const QTermSelectionModel::LogicalEndpointAnchor &anchor)
{
    if (spans.isEmpty()) {
        return ProjectionEndpoint();
    }

    const ProjectionRowSpan &span = spans.at(qBound(0, anchor.logicalLineIndex, spans.size() - 1));
    int remainingColumns = qMax(0, anchor.displayColumn);

    for (int projectionRow = span.startProjectionRow; projectionRow <= span.endProjectionRow; ++projectionRow) {
        const int rowColumns = projectionRowDisplayColumns(buffer, span, projectionRow);
        if (remainingColumns <= rowColumns || projectionRow == span.endProjectionRow) {
            return ProjectionEndpoint{projectionRow, qMin(remainingColumns, rowColumns)};
        }

        remainingColumns -= rowColumns;
    }

    return ProjectionEndpoint{span.endProjectionRow,
                              qMax(0, projectionRowDisplayColumns(buffer, span, span.endProjectionRow))};
}

bool selectionEndpointsAreVisible(const QTermBuffer &buffer,
                                  const ProjectionEndpoint &start,
                                  const ProjectionEndpoint &end)
{
    const int firstVisibleProjectionRow = buffer.visibleRowOffset();
    const int lastVisibleProjectionRow = firstVisibleProjectionRow + qMax(0, buffer.rows() - 1);
    return start.projectionRow >= firstVisibleProjectionRow &&
        start.projectionRow <= lastVisibleProjectionRow &&
        end.projectionRow >= firstVisibleProjectionRow &&
        end.projectionRow <= lastVisibleProjectionRow;
}

} // namespace

void QTermSelectionModel::setTerminalSize(int columns, int rows)
{
    if (m_columns == columns && m_rows == rows) {
        return;
    }

    m_columns = columns;
    m_rows = rows;

    if (m_snapshot.hasSelection && m_snapshot.startRow >= 0 && m_snapshot.endRow >= 0) {
        const NormalizedSelectionRange range = normalizeSelectionRange(m_rows,
                                                                      m_columns,
                                                                      m_snapshot.startRow,
                                                                      m_snapshot.startColumn,
                                                                      m_snapshot.endRow,
                                                                      m_snapshot.endColumn);
        m_snapshot.hasSelection = range.hasSelection;
        m_snapshot.startRow = range.startRow;
        m_snapshot.startColumn = range.startColumn;
        m_snapshot.endRow = range.endRow;
        m_snapshot.endColumn = range.endColumn;
    }

    if (!m_snapshot.hasSelection) {
        m_snapshot.selectedText.clear();
    }
}

void QTermSelectionModel::prepareForResize(const QTermBuffer &buffer)
{
    if (!m_snapshot.hasSelection || m_snapshot.startRow < 0 || m_snapshot.endRow < 0 || buffer.rows() <= 0) {
        return;
    }

    const QVector<ProjectionRowSpan> spans = collectProjectionLogicalLineSpans(buffer);
    if (spans.isEmpty()) {
        return;
    }

    m_selectionAnchors = LogicalSelectionAnchors{
        captureLogicalAnchor(buffer, spans, m_snapshot.startRow, m_snapshot.startColumn),
        captureLogicalAnchor(buffer, spans, m_snapshot.endRow, m_snapshot.endColumn)
    };
}

void QTermSelectionModel::completeResize(const QTermBuffer &buffer, int columns, int rows)
{
    m_columns = columns;
    m_rows = rows;

    if (!m_snapshot.hasSelection) {
        m_selectionAnchors.reset();
        m_snapshot.selectedText.clear();
        return;
    }

    if (!m_selectionAnchors.has_value()) {
        if (m_snapshot.startRow >= 0 && m_snapshot.endRow >= 0) {
            prepareForResize(buffer);
        }

        if (!m_selectionAnchors.has_value()) {
            m_snapshot.selectedText.clear();
            m_snapshot.hasSelection = false;
        }
        return;
    }

    const QVector<ProjectionRowSpan> spans = collectProjectionLogicalLineSpans(buffer);
    if (spans.isEmpty()) {
        clearSelection();
        return;
    }

    const ProjectionEndpoint start = mapLogicalAnchorToProjectionEndpoint(buffer, spans, m_selectionAnchors->start);
    const ProjectionEndpoint end = mapLogicalAnchorToProjectionEndpoint(buffer, spans, m_selectionAnchors->end);

    m_snapshot.hasSelection = true;
    if (selectionEndpointsAreVisible(buffer, start, end)) {
        const int visibleRowOffset = buffer.visibleRowOffset();
        m_snapshot.startRow = start.projectionRow - visibleRowOffset;
        m_snapshot.startColumn = start.column;
        m_snapshot.endRow = end.projectionRow - visibleRowOffset;
        m_snapshot.endColumn = end.column;
    } else {
        m_snapshot.startRow = -1;
        m_snapshot.startColumn = 0;
        m_snapshot.endRow = -1;
        m_snapshot.endColumn = 0;
    }

    m_snapshot.selectedText = selectionTextFromProjection(buffer, start, end);
}

void QTermSelectionModel::refreshSelectionText(const QTermBuffer &buffer)
{
    if (m_snapshot.hasSelection && m_snapshot.startRow >= 0 && m_snapshot.endRow >= 0) {
        prepareForResize(buffer);
    }

    updateSelectedText(buffer);
}

void QTermSelectionModel::clearSelection()
{
    m_selectionAnchors.reset();
    m_snapshot = QTermSelectionSnapshot();
}

void QTermSelectionModel::setSelectionRange(int startRow, int startColumn, int endRow, int endColumn)
{
    const NormalizedSelectionRange range = normalizeSelectionRange(m_rows,
                                                                  m_columns,
                                                                  startRow,
                                                                  startColumn,
                                                                  endRow,
                                                                  endColumn);
    m_snapshot.hasSelection = range.hasSelection;
    m_snapshot.startRow = range.startRow;
    m_snapshot.startColumn = range.startColumn;
    m_snapshot.endRow = range.endRow;
    m_snapshot.endColumn = range.endColumn;
}

void QTermSelectionModel::selectWordAt(const QTermBuffer &buffer, int row, int column)
{
    if (buffer.rows() <= 0) {
        clearSelection();
        return;
    }

    const int boundedRow = qBound(0, row, buffer.rows() - 1);
    const QVector<LogicalColumnRef> logicalColumns = collectLogicalLineColumns(buffer, boundedRow);
    if (logicalColumns.isEmpty()) {
        clearSelection();
        return;
    }

    int logicalIndex = -1;
    for (int index = 0; index < logicalColumns.size(); ++index) {
        const LogicalColumnRef &logicalColumn = logicalColumns.at(index);
        if (logicalColumn.row == boundedRow && logicalColumn.column == qMax(0, column)) {
            logicalIndex = index;
            break;
        }
    }

    if (logicalIndex < 0) {
        const int fallbackColumn = qMax(0, column);
        for (int index = logicalColumns.size() - 1; index >= 0; --index) {
            const LogicalColumnRef &logicalColumn = logicalColumns.at(index);
            if (logicalColumn.row == boundedRow && logicalColumn.column <= fallbackColumn) {
                logicalIndex = index;
                break;
            }
        }
    }

    if (logicalIndex < 0) {
        clearSelection();
        return;
    }

    while (logicalIndex > 0 && logicalColumns.at(logicalIndex).text.isEmpty()) {
        --logicalIndex;
    }

    const TokenClass tokenClass = classifyColumnText(logicalColumns.at(logicalIndex).text);
    if (tokenClass == TokenClass::Empty) {
        clearSelection();
        return;
    }

    int selectionStartIndex = logicalIndex;
    while (selectionStartIndex > 0) {
        int previousIndex = selectionStartIndex - 1;
        while (previousIndex >= 0 && logicalColumns.at(previousIndex).text.isEmpty()) {
            --previousIndex;
        }

        if (previousIndex < 0 || classifyColumnText(logicalColumns.at(previousIndex).text) != tokenClass) {
            break;
        }

        selectionStartIndex = previousIndex;
    }

    int selectionEndIndex = logicalIndex;
    while (selectionEndIndex + 1 < logicalColumns.size()) {
        int nextIndex = selectionEndIndex + 1;
        while (nextIndex < logicalColumns.size() && logicalColumns.at(nextIndex).text.isEmpty()) {
            ++nextIndex;
        }

        if (nextIndex >= logicalColumns.size() || classifyColumnText(logicalColumns.at(nextIndex).text) != tokenClass) {
            break;
        }

        selectionEndIndex = nextIndex;
    }

    trimUrlLikeSelectionBounds(logicalColumns, selectionStartIndex, selectionEndIndex);

    const LogicalColumnRef &selectionStart = logicalColumns.at(selectionStartIndex);
    const LogicalColumnRef &selectionEnd = logicalColumns.at(selectionEndIndex);
    const int exclusiveEndColumn = graphemeEndColumn(buffer.lineAt(selectionEnd.row).columnTexts(), selectionEnd.column);
    setSelectionRange(selectionStart.row, selectionStart.column, selectionEnd.row, exclusiveEndColumn);
}

void QTermSelectionModel::selectLogicalLineAt(const QTermBuffer &buffer, int row)
{
    if (buffer.rows() <= 0) {
        clearSelection();
        return;
    }

    const int boundedRow = qBound(0, row, buffer.rows() - 1);
    const LogicalRowSpan rowSpan = logicalLineRowSpan(buffer, boundedRow);
    const int endColumn = lastRelevantColumn(buffer.lineAt(rowSpan.endRow).columnTexts());
    if (endColumn <= 0) {
        clearSelection();
        return;
    }

    setSelectionRange(rowSpan.startRow, 0, rowSpan.endRow, endColumn);
}

const QTermSelectionSnapshot &QTermSelectionModel::snapshot() const noexcept
{
    return m_snapshot;
}

void QTermSelectionModel::updateSelectedText(const QTermBuffer &buffer)
{
    if (!m_snapshot.hasSelection) {
        m_selectionAnchors.reset();
        m_snapshot.selectedText.clear();
        return;
    }

    if (!m_selectionAnchors.has_value()) {
        if (m_snapshot.startRow < 0 || m_snapshot.endRow < 0) {
            m_snapshot.selectedText.clear();
            return;
        }

        const QTermSelectionSnapshot visibleSnapshot = m_snapshot;
        m_snapshot.selectedText = selectionTextFromBuffer(buffer, visibleSnapshot);
        return;
    }

    const QVector<ProjectionRowSpan> spans = collectProjectionLogicalLineSpans(buffer);
    if (spans.isEmpty()) {
        m_snapshot.selectedText.clear();
        return;
    }

    const ProjectionEndpoint start = mapLogicalAnchorToProjectionEndpoint(buffer, spans, m_selectionAnchors->start);
    const ProjectionEndpoint end = mapLogicalAnchorToProjectionEndpoint(buffer, spans, m_selectionAnchors->end);
    if (selectionEndpointsAreVisible(buffer, start, end)) {
        const int visibleRowOffset = buffer.visibleRowOffset();
        m_snapshot.startRow = start.projectionRow - visibleRowOffset;
        m_snapshot.startColumn = start.column;
        m_snapshot.endRow = end.projectionRow - visibleRowOffset;
        m_snapshot.endColumn = end.column;
    } else {
        m_snapshot.startRow = -1;
        m_snapshot.startColumn = 0;
        m_snapshot.endRow = -1;
        m_snapshot.endColumn = 0;
    }

    m_snapshot.selectedText = selectionTextFromProjection(buffer, start, end);
}

} // namespace QTerm