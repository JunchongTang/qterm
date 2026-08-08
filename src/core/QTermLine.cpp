#include "QTermLine.h"

#include <algorithm>

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
    m_usedColumns = qMin(m_usedColumns, columns);
}

void QTermLine::clear()
{
    for (int index = 0; index < m_cells.size(); ++index) {
        clearCharacterAt(index);
    }

    m_wrappedToNextLine = false;
    m_usedColumns = 0;
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
    // Shifts cells around; keep the watermark conservative.
    m_usedColumns = m_cells.size();
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
    // Shifts cells around; keep the watermark conservative.
    m_usedColumns = m_cells.size();
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
    markWritten(column + 1);
    m_cells[column] = cell;
}

void QTermLine::setNarrowRun(int column, QStringView text, const QTermCellAttributes &attributes)
{
    if (column < 0 || text.isEmpty()) {
        return;
    }

    const int count = qMin(text.size(), m_cells.size() - column);
    if (count <= 0) {
        return;
    }

    // Only the two ends can overlap a wide character: the run's start may land
    // on a continuation cell, and its end may cut a wide character in half.
    // Everything in between is overwritten wholesale.
    clearCharacterAt(column);
    if (column > 0 && m_cells.at(column - 1).width > 1) {
        clearCharacterAt(column - 1);
    }
    clearCharacterAt(column + count - 1);

    markWritten(column + count);
    QTermCell *cells = m_cells.data();
    for (int offset = 0; offset < count; ++offset) {
        QTermCell &cell = cells[column + offset];
        cell.codepoint = text.at(offset).unicode();
        cell.combiningId = 0;
        cell.width = 1;
        cell.continuation = 0;
        cell.attributes = attributes;
    }
}

QString QTermLine::textAt(int column) const
{
    if (column < 0 || column >= m_cells.size()) {
        return {};
    }
    const QTermCell &cell = m_cells.at(column);
    if (cell.combiningId != 0) {
        return m_combining.value(cell.combiningId);
    }
    if (cell.codepoint == 0) {
        return {};
    }
    return QString::fromUcs4(&cell.codepoint, 1);
}

quint16 QTermLine::internCombining(const QString &text)
{
    // Combining sequences are rare; ids are line-local and reset on clear.
    // Saturating at the maximum simply stops interning further sequences,
    // which degrades to dropping marks rather than corrupting other cells.
    if (m_nextCombiningId == 0xffff) {
        return 0;
    }
    const quint16 id = m_nextCombiningId++;
    m_combining.insert(id, text);
    return id;
}

void QTermLine::writeCell(QTermCell &cell, const QString &text, int width,
                          const QTermCellAttributes &attributes)
{
    cell.combiningId = 0;
    cell.codepoint = 0;
    if (!text.isEmpty()) {
        const QChar first = text.at(0);
        const bool singleUnit = text.size() == 1;
        const bool surrogatePair = text.size() == 2 && first.isHighSurrogate();
        if (singleUnit) {
            cell.codepoint = first.unicode();
        } else if (surrogatePair) {
            cell.codepoint = QChar::surrogateToUcs4(first, text.at(1));
        } else {
            // Base character plus combining marks: keep the whole grapheme.
            cell.codepoint = QChar::isHighSurrogate(first.unicode())
                    ? QChar::surrogateToUcs4(first, text.at(1))
                    : first.unicode();
            cell.combiningId = internCombining(text);
        }
    }
    cell.width = quint8(width);
    cell.continuation = 0;
    cell.attributes = attributes;
}

void QTermLine::markWritten(int endColumn)
{
    m_usedColumns = qMax(m_usedColumns, qMin(endColumn, m_cells.size()));
}

void QTermLine::resetForReuse(int columns)
{
    if (m_cells.size() != columns) {
        m_cells = QVector<QTermCell>(columns);
    } else {
        // Detach once and reset through the raw pointer: QList's non-const
        // operator[] re-checks for detach on every index, which dominated this
        // loop when it was written the obvious way.
        const int limit = qMin(m_usedColumns, m_cells.size());
        // The cells are trivially constructible now, so the used range can be
        // reset in bulk instead of per column.
        std::fill_n(m_cells.data(), limit, QTermCell{});
    }
    m_combining.clear();
    m_nextCombiningId = 1;
    m_usedColumns = 0;
    m_wrappedToNextLine = false;
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

    const int width = qMax(1, int(m_cells.at(baseColumn).width));
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
    if (baseCell.isBlank()) {
        return false;
    }

    const QString combined = textAt(baseColumn) + mark;
    if (baseCell.combiningId != 0) {
        m_combining.insert(baseCell.combiningId, combined);
    } else {
        baseCell.combiningId = internCombining(combined);
    }
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
    markWritten(column + boundedWidth);
    writeCell(m_cells[column], text, boundedWidth, attributes);

    for (int offset = 1; offset < boundedWidth && column + offset < m_cells.size(); ++offset) {
        QTermCell &continuation = m_cells[column + offset];
        continuation = QTermCell{};
        continuation.width = 0;
        continuation.continuation = 1;
        continuation.attributes = attributes;
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

        const QString cellText = textAt(column);
        text.append(cellText.isEmpty() ? QStringLiteral(" ") : cellText);
    }

    return text;
}

QStringList QTermLine::columnTexts() const
{
    QStringList columns;
    columns.reserve(m_cells.size());

    for (int column = 0; column < m_cells.size(); ++column) {
        const QTermCell &cell = m_cells.at(column);
        const QString cellText = textAt(column);
        columns.append(cell.continuation ? QString()
                                         : (cellText.isEmpty() ? QStringLiteral(" ") : cellText));
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
        run.insert(QStringLiteral("hyperlinkId"), currentAttributes.hyperlinkId);
        runs.append(run);

        currentText.clear();
        currentColumns = 0;
        hasCurrentRun = false;
    };

    for (int column = 0; column < m_cells.size(); ++column) {
        const QTermCell &cell = m_cells.at(column);
        if (cell.continuation) {
            continue;
        }

        const QString resolved = textAt(column);
        const QString cellText = resolved.isEmpty() ? QStringLiteral(" ") : resolved;
        const int cellColumns = qMax(1, int(cell.width));
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
            currentAttributes.backgroundRgb == cell.attributes.backgroundRgb &&
            currentAttributes.hyperlinkId == cell.attributes.hyperlinkId) {
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

    for (int column = 0; column < m_cells.size(); ++column) {
        const QTermCell &cell = m_cells.at(column);
        if (cell.continuation) {
            continue;
        }
        const QString cellText = textAt(column);
        text.append(cellText.isEmpty() ? QStringLiteral(" ") : cellText);
    }

    if (!m_wrappedToNextLine) {
        while (!text.isEmpty() && text.back() == u' ') {
            text.chop(1);
        }
    }

    return text;
}

} // namespace QTerm