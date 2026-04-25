#ifndef QTERM_QTERMBUFFER_H
#define QTERM_QTERMBUFFER_H

#include <QStringList>
#include <QVariantList>
#include <QVector>

#include "QTermCursorState.h"
#include "QTermLine.h"

namespace QTerm {

class QTermBuffer
{
public:
    QTermBuffer(int columns = 80, int rows = 24);

    int rows() const noexcept;
    int columns() const noexcept;
    int historyLineCount() const noexcept;
    int projectionRowCount() const noexcept;
    int visibleRowOffset() const noexcept;

    QTermCursorState resize(int columns, int rows, const QTermCursorState &cursorState);
    void clear();
    void clearVisible();
    void clearVisibleFrom(int row, int column);
    void clearVisibleTo(int row, int column);
    int severPredecessorWrapChain(int visibleRow);
    void scrollUp();
    void insertLines(int row, int count, int scrollTop, int scrollBottom);
    void deleteLines(int row, int count, int scrollTop, int scrollBottom);

    QTermLine &lineAt(int row);
    const QTermLine &lineAt(int row) const;
    const QTermLine &projectionLineAt(int projectionRow) const;

    QStringList viewportLineTexts(int topProjectionRow, int rowCount) const;
    QVariantList viewportLineRuns(int topProjectionRow, int rowCount) const;
    QStringList visibleLineTexts() const;
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