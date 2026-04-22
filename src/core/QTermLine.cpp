#include "QTermLine.h"

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
    for (QTermCell &cell : m_cells) {
        cell = QTermCell();
    }

    m_wrappedToNextLine = false;
}

void QTermLine::clearToEnd(int column)
{
    for (int index = column; index < m_cells.size(); ++index) {
        m_cells[index] = QTermCell();
    }

    m_wrappedToNextLine = false;
}

void QTermLine::clearToColumn(int column)
{
    for (int index = 0; index <= column && index < m_cells.size(); ++index) {
        m_cells[index] = QTermCell();
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

bool QTermLine::wrappedToNextLine() const noexcept
{
    return m_wrappedToNextLine;
}

void QTermLine::setWrappedToNextLine(bool wrapped)
{
    m_wrappedToNextLine = wrapped;
}

QString QTermLine::plainText() const
{
    QString text;
    text.reserve(m_cells.size());

    for (const QTermCell &cell : m_cells) {
        text.append(cell.text.isEmpty() ? QStringLiteral(" ") : cell.text);
    }

    while (!text.isEmpty() && text.back() == u' ') {
        text.chop(1);
    }

    return text;
}

} // namespace QTerm