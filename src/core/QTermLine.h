#ifndef QTERM_QTERMLINE_H
#define QTERM_QTERMLINE_H

#include <QStringList>
#include <QVariantList>
#include <QHash>
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
    // Resolves a cell to its text, joining any combining marks held in the
    // line's side table. Blank cells return an empty string.
    QString textAt(int column) const;
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

    // Stores the full grapheme for a cell that carries combining marks and
    // returns its key; keys are line-local and reset whenever the line is
    // cleared or recycled.
    quint16 internCombining(const QString &text);
    void writeCell(QTermCell &cell, const QString &text, int width,
                   const QTermCellAttributes &attributes);

    QVector<QTermCell> m_cells;
    QHash<quint16, QString> m_combining;
    quint16 m_nextCombiningId = 1;
    int m_usedColumns = 0;
    bool m_wrappedToNextLine = false;
};

} // namespace QTerm

#endif // QTERM_QTERMLINE_H