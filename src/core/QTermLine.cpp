#include "QTermLine.h"

#include <QVariantMap>
#include <QtGlobal>

namespace QTerm {

QTermLine::QTermLine(int columns)
    : m_cells(columns)
{
}

int QTermLine::columns() const noexcept
{
    return m_cells.size();
}

void QTermLine::resize(int columns)
{
    m_cells.resize(columns);
}

void QTermLine::clear()
{
    for (int index = 0; index < m_cells.size(); ++index) {
        clearCharacterAt(index);
    }

    m_wrappedToNextLine = false;
}

void QTermLine::clearToEnd(int column)
{
    for (int index = column; index < m_cells.size(); ++index) {
        clearCharacterAt(index);
    }

    m_wrappedToNextLine = false;
}

void QTermLine::clearToColumn(int column)
{
    for (int index = 0; index <= column && index < m_cells.size(); ++index) {
        clearCharacterAt(index);
    }

    m_wrappedToNextLine = false;
}

void QTermLine::insertCells(int column, int count)
{
    if (column < 0 || column >= m_cells.size() || count <= 0) {
        return;
    }

    const int boundedCount = qMin(count, m_cells.size() - column);
    for (int index = m_cells.size() - 1; index >= column + boundedCount; --index) {
        m_cells[index] = m_cells.at(index - boundedCount);
    }

    for (int index = column; index < column + boundedCount; ++index) {
        m_cells[index] = QTermCell();
    }

    m_wrappedToNextLine = false;
}

void QTermLine::deleteCells(int column, int count)
{
    if (column < 0 || column >= m_cells.size() || count <= 0) {
        return;
    }

    const int boundedCount = qMin(count, m_cells.size() - column);
    for (int index = column; index < m_cells.size() - boundedCount; ++index) {
        m_cells[index] = m_cells.at(index + boundedCount);
    }

    for (int index = m_cells.size() - boundedCount; index < m_cells.size(); ++index) {
        m_cells[index] = QTermCell();
    }

    m_wrappedToNextLine = false;
}

const QTermCell &QTermLine::cellAt(int column) const
{
    return m_cells.at(column);
}

void QTermLine::setCell(int column, const QTermCell &cell)
{
    m_cells[column] = cell;
}

void QTermLine::clearCharacterAt(int column)
{
    if (column < 0 || column >= m_cells.size()) {
        return;
    }

    const int baseColumn = leadingColumnFor(column);
    if (baseColumn < 0 || baseColumn >= m_cells.size()) {
        return;
    }

    const int width = qMax(1, m_cells.at(baseColumn).width);
    m_cells[baseColumn] = QTermCell();

    for (int offset = 1; offset < width && baseColumn + offset < m_cells.size(); ++offset) {
        m_cells[baseColumn + offset] = QTermCell();
    }
}

bool QTermLine::appendCombiningMark(int column, const QString &mark)
{
    if (column < 0 || column >= m_cells.size()) {
        return false;
    }

    const int baseColumn = leadingColumnFor(column);
    if (baseColumn < 0 || baseColumn >= m_cells.size()) {
        return false;
    }

    QTermCell &baseCell = m_cells[baseColumn];
    if (baseCell.text.isEmpty()) {
        return false;
    }

    baseCell.text.append(mark);
    return true;
}

void QTermLine::setCharacter(int column, const QString &text, int width, const QTermCellAttributes &attributes)
{
    if (column < 0 || column >= m_cells.size()) {
        return;
    }

    clearCharacterAt(column);
    if (column > 0 && m_cells.at(column - 1).width > 1) {
        clearCharacterAt(column - 1);
    }

    const int boundedWidth = qBound(1, width, m_cells.size() - column);
    m_cells[column] = QTermCell{text, boundedWidth, false, attributes};

    for (int offset = 1; offset < boundedWidth && column + offset < m_cells.size(); ++offset) {
        m_cells[column + offset] = QTermCell{QString(), 0, true, attributes};
    }
}

int QTermLine::leadingColumnFor(int column) const
{
    if (column < 0 || column >= m_cells.size()) {
        return column;
    }

    int baseColumn = column;
    while (baseColumn > 0 && m_cells.at(baseColumn).continuation) {
        --baseColumn;
    }

    return baseColumn;
}

bool QTermLine::wrappedToNextLine() const noexcept
{
    return m_wrappedToNextLine;
}

void QTermLine::setWrappedToNextLine(bool wrapped)
{
    m_wrappedToNextLine = wrapped;
}

QString QTermLine::textInColumnRange(int startColumn, int endColumn) const
{
    QString text;

    const int boundedStart = qBound(0, startColumn, m_cells.size());
    const int boundedEnd = qBound(0, endColumn, m_cells.size());
    if (boundedStart >= boundedEnd) {
        return text;
    }

    for (int column = boundedStart; column < boundedEnd; ++column) {
        const QTermCell &cell = m_cells.at(column);
        if (cell.continuation) {
            continue;
        }

        text.append(cell.text.isEmpty() ? QStringLiteral(" ") : cell.text);
    }

    return text;
}

QStringList QTermLine::columnTexts() const
{
    QStringList columns;
    columns.reserve(m_cells.size());

    for (const QTermCell &cell : m_cells) {
        columns.append(cell.continuation ? QString() : (cell.text.isEmpty() ? QStringLiteral(" ") : cell.text));
    }

    return columns;
}

QVariantList QTermLine::styleRuns() const
{
    QVariantList runs;

    QString currentText;
    int currentColumns = 0;
    QTermCellAttributes currentAttributes;
    bool hasCurrentRun = false;

    const auto flushRun = [&runs, &currentText, &currentColumns, &currentAttributes, &hasCurrentRun]() {
        if (!hasCurrentRun) {
            return;
        }

        QVariantMap run;
        run.insert(QStringLiteral("text"), currentText);
        run.insert(QStringLiteral("columns"), currentColumns);
        run.insert(QStringLiteral("bold"), currentAttributes.bold);
        run.insert(QStringLiteral("dim"), currentAttributes.dim);
        run.insert(QStringLiteral("italic"), currentAttributes.italic);
        run.insert(QStringLiteral("underline"), currentAttributes.underline);
        run.insert(QStringLiteral("strikethrough"), currentAttributes.strikethrough);
        run.insert(QStringLiteral("inverse"), currentAttributes.inverse);
        run.insert(QStringLiteral("foregroundIndex"), currentAttributes.foregroundIndex);
        run.insert(QStringLiteral("backgroundIndex"), currentAttributes.backgroundIndex);
        run.insert(QStringLiteral("foregroundRgb"), currentAttributes.foregroundRgb);
        run.insert(QStringLiteral("backgroundRgb"), currentAttributes.backgroundRgb);
        runs.append(run);

        currentText.clear();
        currentColumns = 0;
        hasCurrentRun = false;
    };

    for (const QTermCell &cell : m_cells) {
        if (cell.continuation) {
            continue;
        }

        const QString cellText = cell.text.isEmpty() ? QStringLiteral(" ") : cell.text;
        const int cellColumns = qMax(1, cell.width);
        if (!hasCurrentRun) {
            currentText = cellText;
            currentColumns = cellColumns;
            currentAttributes = cell.attributes;
            hasCurrentRun = true;
            continue;
        }

        if (currentAttributes.bold == cell.attributes.bold &&
            currentAttributes.dim == cell.attributes.dim &&
            currentAttributes.italic == cell.attributes.italic &&
            currentAttributes.underline == cell.attributes.underline &&
            currentAttributes.strikethrough == cell.attributes.strikethrough &&
            currentAttributes.inverse == cell.attributes.inverse &&
            currentAttributes.foregroundIndex == cell.attributes.foregroundIndex &&
            currentAttributes.backgroundIndex == cell.attributes.backgroundIndex &&
            currentAttributes.foregroundRgb == cell.attributes.foregroundRgb &&
            currentAttributes.backgroundRgb == cell.attributes.backgroundRgb) {
            currentText.append(cellText);
            currentColumns += cellColumns;
            continue;
        }

        flushRun();
        currentText = cellText;
        currentColumns = cellColumns;
        currentAttributes = cell.attributes;
        hasCurrentRun = true;
    }

    flushRun();
    return runs;
}

QString QTermLine::plainText() const
{
    QString text;
    text.reserve(m_cells.size());

    for (const QTermCell &cell : m_cells) {
        if (cell.continuation) {
            continue;
        }
        text.append(cell.text.isEmpty() ? QStringLiteral(" ") : cell.text);
    }

    if (!m_wrappedToNextLine) {
        while (!text.isEmpty() && text.back() == u' ') {
            text.chop(1);
        }
    }

    return text;
}

} // namespace QTerm