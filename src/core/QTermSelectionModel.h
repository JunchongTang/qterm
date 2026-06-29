#ifndef QTERM_QTERMSELECTIONMODEL_H
#define QTERM_QTERMSELECTIONMODEL_H

#include <optional>

#include <QString>

namespace QTerm {

class QTermBuffer;

struct QTermSelectionSnapshot
{
    bool hasSelection = false;
    int startRow = 0;
    int startColumn = 0;
    int endRow = 0;
    int endColumn = 0;
    QString selectedText;
};

class QTermSelectionModel
{
public:
    struct LogicalEndpointAnchor
    {
        int logicalLineIndex = 0;
        int displayColumn = 0;
    };

    struct LogicalSelectionAnchors
    {
        LogicalEndpointAnchor start;
        LogicalEndpointAnchor end;
    };

    void setTerminalSize(int columns, int rows);
    void setViewport(int topProjectionRow);
    void prepareForResize(const QTermBuffer &buffer);
    void completeResize(const QTermBuffer &buffer, int columns, int rows);
    void refreshSelectionText(const QTermBuffer &buffer);

    void clearSelection();
    void setSelectionRange(int startRow, int startColumn, int endRow, int endColumn);
    // 拖拽选区(projection 绝对行 + 单元格列)。内部排序端点、对端点做字素对齐
    // (起点回退到字首格、终点扩到字尾的下一格 → 宽字符/CJK 整字纳入),再走
    // logical 锚点路径(可跨 scrollback、自动滚动不丢起点)。视口行做不到这些,
    // 故拖拽不再用 setSelectionRange。
    void setSelectionFromDragCells(const QTermBuffer &buffer,
                                   int anchorProjectionRow, int anchorColumn,
                                   int dragProjectionRow, int dragColumn);
    void selectWordAt(const QTermBuffer &buffer, int row, int column);
    void selectLogicalLineAt(const QTermBuffer &buffer, int row);

    const QTermSelectionSnapshot &snapshot() const noexcept;

private:
    void captureAnchorsFromSnapshot(const QTermBuffer &buffer);
    void setSelectionFromProjectionEndpoints(const QTermBuffer &buffer,
                                            int startProjectionRow,
                                            int startColumn,
                                            int endProjectionRow,
                                            int endColumn);
    void updateSelectedText(const QTermBuffer &buffer);

    int m_rows = 24;
    int m_columns = 80;
    int m_viewportTopProjectionRow = 0;
    std::optional<LogicalSelectionAnchors> m_selectionAnchors;
    QTermSelectionSnapshot m_snapshot;
};

} // namespace QTerm

#endif // QTERM_QTERMSELECTIONMODEL_H