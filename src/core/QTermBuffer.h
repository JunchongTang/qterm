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
    QTermBuffer(int columns = 80, int rows = 24, int maximumHistoryLines = 10000);

    int rows() const noexcept;
    int columns() const noexcept;
    int historyLineCount() const noexcept;
    int maximumHistoryLines() const noexcept;
    void setMaximumHistoryLines(int maximumHistoryLines);
    int projectionRowCount() const noexcept;
    int visibleRowOffset() const noexcept;

    QTermCursorState resize(int columns, int rows, const QTermCursorState &cursorState);
    void clear();
    void clearHistory();
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
    QString dumpPlainText() const;
    // Dump current buffer (scrollback + visible) as UTF-8 bytes annotated with
    // SGR (CSI ...m) sequences for each style transition. Re-feeding the output
    // into a fresh terminal via feedText() reproduces the same visual state.
    // `maxLines` caps how many tail lines are emitted (0 = no cap).
    QByteArray dumpAnsi(int maxLines = 5000) const;

    // ── Dirty-row tracking ────────────────────────────────────────────────────
    // Returns the set of *visible* row indices that have been modified since the
    // last call to clearDirtyRows().  m_allRowsDirty is a fast-path flag set
    // when the entire viewport should be repainted (scroll, resize, clear).
    const QVector<bool> &dirtyRows() const noexcept { return m_dirtyRows; }
    bool allRowsDirty() const noexcept { return m_allRowsDirty; }
    void clearDirtyRows();

private:
    void appendEmptyVisibleLine();
    void markDirtyRow(int visibleRow);
    void markAllRowsDirty();

    int m_columns = 80;
    int m_rows = 24;
    int m_maximumHistoryLines = 10000;
    QVector<QTermLine> m_historyLines;
    QVector<QTermLine> m_visibleLines;

    // m_dirtyRows[i] is true if visible row i was touched since clearDirtyRows().
    QVector<bool> m_dirtyRows;
    bool m_allRowsDirty = true; // start dirty so first paint is full
};

} // namespace QTerm

#endif // QTERM_QTERMBUFFER_H