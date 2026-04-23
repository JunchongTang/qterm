#include <QTerm/QTermSurfaceModel.h>

#include <QTerm/QTermTerminal.h>

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

} // namespace

QTermSurfaceModel::QTermSurfaceModel(QObject *parent)
    : QObject(parent)
{
}

int QTermSurfaceModel::rows() const noexcept
{
    return m_rows;
}

int QTermSurfaceModel::columns() const noexcept
{
    return m_columns;
}

int QTermSurfaceModel::cursorRow() const noexcept
{
    return m_cursorRow;
}

int QTermSurfaceModel::cursorColumn() const noexcept
{
    return m_cursorColumn;
}

bool QTermSurfaceModel::cursorVisible() const noexcept
{
    return m_cursorVisible;
}

bool QTermSurfaceModel::hasSelection() const noexcept
{
    return m_hasSelection;
}

int QTermSurfaceModel::selectionStartRow() const noexcept
{
    return m_selectionStartRow;
}

int QTermSurfaceModel::selectionStartColumn() const noexcept
{
    return m_selectionStartColumn;
}

int QTermSurfaceModel::selectionEndRow() const noexcept
{
    return m_selectionEndRow;
}

int QTermSurfaceModel::selectionEndColumn() const noexcept
{
    return m_selectionEndColumn;
}

QString QTermSurfaceModel::selectedText() const
{
    return m_selectedText;
}

QStringList QTermSurfaceModel::visibleLines() const
{
    return m_visibleLines;
}

QVariantList QTermSurfaceModel::visibleLineRuns() const
{
    return m_visibleLineRuns;
}

QString QTermSurfaceModel::plainText() const
{
    return m_plainText;
}

void QTermSurfaceModel::setSize(int columns, int rows)
{
    if (m_columns == columns && m_rows == rows) {
        return;
    }

    m_columns = columns;
    m_rows = rows;
    emit sizeChanged();
}

void QTermSurfaceModel::setCursor(int row, int column, bool visible)
{
    if (m_cursorRow == row && m_cursorColumn == column && m_cursorVisible == visible) {
        return;
    }

    m_cursorRow = row;
    m_cursorColumn = column;
    m_cursorVisible = visible;
    emit cursorChanged();
}

void QTermSurfaceModel::clearSelection()
{
    if (m_selectionController) {
        m_selectionController->clearSelection();
        return;
    }

    if (!m_hasSelection && m_selectedText.isEmpty()) {
        return;
    }

    setSelectionSnapshot(false, 0, 0, 0, 0);
}

void QTermSurfaceModel::setSelectionRange(int startRow, int startColumn, int endRow, int endColumn)
{
    if (m_selectionController) {
        m_selectionController->setSelectionRange(startRow, startColumn, endRow, endColumn);
        return;
    }

    const NormalizedSelectionRange range = normalizeSelectionRange(m_rows, m_columns, startRow, startColumn, endRow, endColumn);
    setSelectionSnapshot(range.hasSelection, range.startRow, range.startColumn, range.endRow, range.endColumn);
}

void QTermSurfaceModel::setSelectionController(QTermTerminal *terminal)
{
    m_selectionController = terminal;
}

void QTermSurfaceModel::setSelectionSnapshot(bool hasSelection, int startRow, int startColumn, int endRow, int endColumn)
{
    const bool selectionChangedState = m_hasSelection != hasSelection ||
        m_selectionStartRow != startRow ||
        m_selectionStartColumn != startColumn ||
        m_selectionEndRow != endRow ||
        m_selectionEndColumn != endColumn;
    const QString previousSelectedText = m_selectedText;

    m_hasSelection = hasSelection;
    m_selectionStartRow = startRow;
    m_selectionStartColumn = startColumn;
    m_selectionEndRow = endRow;
    m_selectionEndColumn = endColumn;
    updateSelectedText();

    if (selectionChangedState || m_selectedText != previousSelectedText) {
        emit selectionChanged();
    }
}

void QTermSurfaceModel::setSelectionSnapshot(bool hasSelection,
                                             int startRow,
                                             int startColumn,
                                             int endRow,
                                             int endColumn,
                                             const QString &selectedText)
{
    const bool selectionChangedState = m_hasSelection != hasSelection ||
        m_selectionStartRow != startRow ||
        m_selectionStartColumn != startColumn ||
        m_selectionEndRow != endRow ||
        m_selectionEndColumn != endColumn ||
        m_selectedText != selectedText;

    m_hasSelection = hasSelection;
    m_selectionStartRow = startRow;
    m_selectionStartColumn = startColumn;
    m_selectionEndRow = endRow;
    m_selectionEndColumn = endColumn;
    m_selectedText = selectedText;

    if (selectionChangedState) {
        emit selectionChanged();
    }
}

void QTermSurfaceModel::setVisibleLineWrapFlags(const QVariantList &visibleLineWrapFlags)
{
    if (m_visibleLineWrapFlags == visibleLineWrapFlags) {
        return;
    }

    const QString previousSelectedText = m_selectedText;
    m_visibleLineWrapFlags = visibleLineWrapFlags;
    updateSelectedText();

    if (m_hasSelection && m_selectedText != previousSelectedText) {
        emit selectionChanged();
    }
}

void QTermSurfaceModel::setVisibleLineColumnTexts(const QVariantList &visibleLineColumnTexts)
{
    if (m_visibleLineColumnTexts == visibleLineColumnTexts) {
        return;
    }

    const QString previousSelectedText = m_selectedText;
    m_visibleLineColumnTexts = visibleLineColumnTexts;
    updateSelectedText();

    if (m_hasSelection && m_selectedText != previousSelectedText) {
        emit selectionChanged();
    }
}

void QTermSurfaceModel::setVisibleLines(const QStringList &visibleLines)
{
    if (m_visibleLines == visibleLines) {
        return;
    }

    m_visibleLines = visibleLines;
    emit visibleLinesChanged();
}

void QTermSurfaceModel::setVisibleLineRuns(const QVariantList &visibleLineRuns)
{
    if (m_visibleLineRuns == visibleLineRuns) {
        return;
    }

    m_visibleLineRuns = visibleLineRuns;
    emit visibleLineRunsChanged();
}

void QTermSurfaceModel::setPlainText(const QString &plainText)
{
    if (m_plainText == plainText) {
        return;
    }

    m_plainText = plainText;
    emit plainTextChanged();
}

void QTermSurfaceModel::updateSelectedText()
{
    QString selectionText;

    if (m_hasSelection) {
        const auto lastRelevantColumn = [](const QStringList &lineColumns) {
            for (int column = lineColumns.size() - 1; column >= 0; --column) {
                const QString &columnText = lineColumns.at(column);
                if (!columnText.isEmpty() && columnText != QStringLiteral(" ")) {
                    return column + 1;
                }
            }

            return 0;
        };

        for (int row = m_selectionStartRow; row <= m_selectionEndRow; ++row) {
            if (row > m_selectionStartRow) {
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
            if (m_selectionStartRow == m_selectionEndRow) {
                for (int column = m_selectionStartColumn; column < m_selectionEndColumn && column < lineColumns.size(); ++column) {
                    selectionText.append(lineColumns.at(column));
                }
                continue;
            }

            if (row == m_selectionStartRow) {
                const int lineEnd = lastRelevantColumn(lineColumns);
                for (int column = m_selectionStartColumn; column < lineEnd && column < lineColumns.size(); ++column) {
                    selectionText.append(lineColumns.at(column));
                }
            } else if (row == m_selectionEndRow) {
                for (int column = 0; column < m_selectionEndColumn && column < lineColumns.size(); ++column) {
                    selectionText.append(lineColumns.at(column));
                }
            } else {
                const int lineEnd = lastRelevantColumn(lineColumns);
                for (int column = 0; column < lineEnd && column < lineColumns.size(); ++column) {
                    selectionText.append(lineColumns.at(column));
                }
            }
        }
    }

    m_selectedText = selectionText;
}

} // namespace QTerm