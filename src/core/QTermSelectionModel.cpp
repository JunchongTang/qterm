#include "QTermSelectionModel.h"

#include "QTermBuffer.h"

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
    return text.startsWith(QStringLiteral("http://")) ||
        text.startsWith(QStringLiteral("https://")) ||
        text.startsWith(QStringLiteral("ftp://"));
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

} // namespace

void QTermSelectionModel::setTerminalSize(int columns, int rows)
{
    if (m_columns == columns && m_rows == rows) {
        return;
    }

    m_columns = columns;
    m_rows = rows;

    if (m_snapshot.hasSelection) {
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

    updateSelectedText();
}

void QTermSelectionModel::setVisibleProjection(const QVariantList &visibleLineWrapFlags,
                                               const QVariantList &visibleLineColumnTexts)
{
    m_visibleLineWrapFlags = visibleLineWrapFlags;
    m_visibleLineColumnTexts = visibleLineColumnTexts;
    updateSelectedText();
}

void QTermSelectionModel::clearSelection()
{
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
    updateSelectedText();
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

void QTermSelectionModel::updateSelectedText()
{
    QString selectionText;

    if (m_snapshot.hasSelection) {
        const auto projectionLastRelevantColumn = [](const QStringList &lineColumns) {
            for (int column = lineColumns.size() - 1; column >= 0; --column) {
                const QString &columnText = lineColumns.at(column);
                if (!columnText.isEmpty() && columnText != QStringLiteral(" ")) {
                    return column + 1;
                }
            }

            return 0;
        };

        for (int row = m_snapshot.startRow; row <= m_snapshot.endRow; ++row) {
            if (row > m_snapshot.startRow) {
                const bool previousRowWrapped = row - 1 < m_visibleLineWrapFlags.size()
                    ? m_visibleLineWrapFlags.at(row - 1).toBool()
                    : false;
                if (!previousRowWrapped) {
                    selectionText.append(u'\n');
                }
            }

            const QStringList lineColumns = row < m_visibleLineColumnTexts.size()
                ? m_visibleLineColumnTexts.at(row).toStringList()
                : QStringList();
            if (m_snapshot.startRow == m_snapshot.endRow) {
                for (int column = m_snapshot.startColumn; column < m_snapshot.endColumn && column < lineColumns.size(); ++column) {
                    selectionText.append(lineColumns.at(column));
                }
                continue;
            }

            if (row == m_snapshot.startRow) {
                const int lineEnd = projectionLastRelevantColumn(lineColumns);
                for (int column = m_snapshot.startColumn; column < lineEnd && column < lineColumns.size(); ++column) {
                    selectionText.append(lineColumns.at(column));
                }
            } else if (row == m_snapshot.endRow) {
                for (int column = 0; column < m_snapshot.endColumn && column < lineColumns.size(); ++column) {
                    selectionText.append(lineColumns.at(column));
                }
            } else {
                const int lineEnd = projectionLastRelevantColumn(lineColumns);
                for (int column = 0; column < lineEnd && column < lineColumns.size(); ++column) {
                    selectionText.append(lineColumns.at(column));
                }
            }
        }
    }

    m_snapshot.selectedText = selectionText;
}

} // namespace QTerm