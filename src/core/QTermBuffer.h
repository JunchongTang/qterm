#ifndef QTERM_QTERMBUFFER_H
#define QTERM_QTERMBUFFER_H

#include <QStringList>
#include <QVariantList>
#include <QVector>

#include "QTermLine.h"

namespace QTerm {

class QTermBuffer
{
public:
    QTermBuffer(int columns = 80, int rows = 24);

    int rows() const noexcept;
    int columns() const noexcept;

    void resize(int columns, int rows);
    void clear();
    void clearVisible();
    void clearVisibleFrom(int row, int column);
    void clearVisibleTo(int row, int column);
    void scrollUp();
    void insertLines(int row, int count, int scrollTop, int scrollBottom);
    void deleteLines(int row, int count, int scrollTop, int scrollBottom);

    QTermLine &lineAt(int row);
    const QTermLine &lineAt(int row) const;

    QStringList visibleLineTexts() const;
    QVariantList visibleLineWrapFlags() const;
    QVariantList visibleLineColumnTexts() const;
    QVariantList visibleLineRuns() const;
    QString debugPlainText() const;

private:
    void appendEmptyVisibleLine();

    int m_columns = 80;
    int m_rows = 24;
    QVector<QTermLine> m_historyLines;
    QVector<QTermLine> m_visibleLines;
};

} // namespace QTerm

#endif // QTERM_QTERMBUFFER_H