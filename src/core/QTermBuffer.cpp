#include "QTermBuffer.h"

#include <QStringList>
#include <QtGlobal>

namespace QTerm {

QTermBuffer::QTermBuffer(int columns, int rows)
    : m_columns(columns)
    , m_rows(rows)
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

void QTermBuffer::resize(int columns, int rows)
{
    m_columns = columns;
    m_rows = rows;

    for (QTermLine &line : m_historyLines) {
        line.resize(columns);
    }

    for (QTermLine &line : m_visibleLines) {
        line.resize(columns);
    }

    while (m_visibleLines.size() > rows) {
        m_historyLines.append(m_visibleLines.takeFirst());
    }

    while (m_visibleLines.size() < rows) {
        appendEmptyVisibleLine();
    }
}

void QTermBuffer::clear()
{
    m_historyLines.clear();
    m_visibleLines.clear();
    m_visibleLines.reserve(m_rows);
    for (int row = 0; row < m_rows; ++row) {
        appendEmptyVisibleLine();
    }
}

void QTermBuffer::clearVisible()
{
    for (QTermLine &line : m_visibleLines) {
        line.clear();
    }
}

void QTermBuffer::clearVisibleFrom(int row, int column)
{
    if (row < 0 || row >= m_visibleLines.size()) {
        return;
    }

    m_visibleLines[row].clearToEnd(column);
    for (int index = row + 1; index < m_visibleLines.size(); ++index) {
        m_visibleLines[index].clear();
    }
}

void QTermBuffer::clearVisibleTo(int row, int column)
{
    if (row < 0 || row >= m_visibleLines.size()) {
        return;
    }

    for (int index = 0; index < row; ++index) {
        m_visibleLines[index].clear();
    }
    m_visibleLines[row].clearToColumn(column);
}

void QTermBuffer::scrollUp()
{
    m_historyLines.append(m_visibleLines.takeFirst());
    appendEmptyVisibleLine();
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
}

QTermLine &QTermBuffer::lineAt(int row)
{
    return m_visibleLines[row];
}

const QTermLine &QTermBuffer::lineAt(int row) const
{
    return m_visibleLines.at(row);
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

} // namespace QTerm