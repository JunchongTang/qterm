#ifndef QTERM_QTERMLINE_H
#define QTERM_QTERMLINE_H

#include <QStringList>
#include <QVariantList>
#include <QVector>

#include "QTermCell.h"

namespace QTerm {

class QTermLine
{
public:
    explicit QTermLine(int columns = 0);

    int columns() const noexcept;
    void resize(int columns);
    void clear();
    void clearToEnd(int column);
    void clearToColumn(int column);
    void insertCells(int column, int count);
    void deleteCells(int column, int count);
    void clearCharacterAt(int column);
    bool appendCombiningMark(int column, const QString &mark);
    void setCharacter(int column, const QString &text, int width, const QTermCellAttributes &attributes);
    // Writes a run of single-width characters starting at \a column. The caller
    // guarantees every character is narrow and non-combining, which lets this
    // skip the per-character width and continuation-cell handling in
    // setCharacter().
    void setNarrowRun(int column, QStringView text, const QTermCellAttributes &attributes);
    // Prepares a recycled line for reuse as a blank row. Only the columns that
    // were actually written are reset, so a line that used a handful of columns
    // costs a handful of assignments rather than a full-width rebuild.
    void resetForReuse(int columns);
    int leadingColumnFor(int column) const;

    const QTermCell &cellAt(int column) const;
    void setCell(int column, const QTermCell &cell);

    bool wrappedToNextLine() const noexcept;
    void setWrappedToNextLine(bool wrapped);

    QString textInColumnRange(int startColumn, int endColumn) const;
    QStringList columnTexts() const;
    QVariantList styleRuns() const;
    QString plainText() const;

private:
    // One past the highest column ever written; the tail beyond it is known to
    // still be default-constructed.
    void markWritten(int endColumn);

    QVector<QTermCell> m_cells;
    int m_usedColumns = 0;
    bool m_wrappedToNextLine = false;
};

} // namespace QTerm

#endif // QTERM_QTERMLINE_H