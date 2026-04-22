#ifndef QTERM_QTERMLINE_H
#define QTERM_QTERMLINE_H

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

    const QTermCell &cellAt(int column) const;
    void setCell(int column, const QTermCell &cell);

    bool wrappedToNextLine() const noexcept;
    void setWrappedToNextLine(bool wrapped);

    QString plainText() const;

private:
    QVector<QTermCell> m_cells;
    bool m_wrappedToNextLine = false;
};

} // namespace QTerm

#endif // QTERM_QTERMLINE_H