// SPDX-License-Identifier: MIT
// Shared input/geometry/scroll controller – shared by all rendering backends.
// Paint-related logic (colours, QPainter, QSGNode) stays in the backend widget.

#pragma once

#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QString>
#include <QTimer>

// Forward-declare Qt event types so the header stays lightweight.
class QHoverEvent;
class QInputMethodEvent;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

namespace QTerm {

class QTermSurfaceModel;
class QTermTerminal;

class QTermViewController : public QObject
{
    Q_OBJECT
public:
    explicit QTermViewController(QObject *parent = nullptr);

    // ── Terminal 绑定 ──────────────────────────────────────────────────────
    QTermTerminal *terminal() const noexcept;
    void setTerminal(QTermTerminal *terminal);

    // ── 字体 / 字元尺寸 ────────────────────────────────────────────────────
    QString fontFamily() const;
    void setFontFamily(const QString &family);

    int fontPixelSize() const noexcept;
    void setFontPixelSize(int size);

    qreal cellWidth() const noexcept;
    qreal cellHeight() const noexcept;

    // ── 几何通知（Widget 在 geometryChange / resizeEvent 里调用） ──────────
    void notifyGeometryChanged(qreal w, qreal h);

    // ── 滚动位置（对接 QML ScrollBar 的 position / size） ─────────────────
    qreal scrollPosition() const noexcept;
    void setScrollPosition(qreal position);
    qreal scrollSize() const noexcept;

    // ── 坐标辅助 ───────────────────────────────────────────────────────────
    int rowAtPosition(qreal y) const;
    int columnAtPosition(qreal x) const;
    int hyperlinkIdAtPosition(int row, int col) const;

    // ── 鼠标协议状态（Widget 据此设置 setAcceptedMouseButtons / setAcceptHoverEvents） ──
    bool mouseProtocolEnabled() const;
    bool hoverEventsNeeded() const;

    // ── IME 光标矩形（用于 Qt::ImCursorRectangle 查询） ───────────────────
    QRectF cursorRect() const;

    // ── 输入事件分发（返回 true = 事件已消耗，Widget 调用 event->accept()） ─
    bool handleKeyPress(QKeyEvent *event);
    bool handleInputMethod(QInputMethodEvent *event);
    bool handleMousePress(QMouseEvent *event);
    bool handleMouseDoubleClick(QMouseEvent *event);
    bool handleMouseMove(QMouseEvent *event);
    bool handleMouseRelease(QMouseEvent *event);
    bool handleHoverMove(QHoverEvent *event);
    bool handleWheel(QWheelEvent *event);

signals:
    void terminalChanged();
    void metricsChanged();          // cellWidth/Height 变化 → Widget 需要 repaint + resize
    void scrollChanged();
    void wheelScrolled(int scrollOffset);
    void copyRequested(const QString &text);
    void hyperlinkActivated(const QString &url);
    void mouseAcceptanceChanged();  // Widget 应重新调用 updateMouseAcceptance()
    void focusRequested();          // Widget 应调用 forceActiveFocus()
    void repaintNeeded();           // surface 内容或滚动变化 → Widget 调用 update()
    void cursorUpdateNeeded();      // 光标位置/可见性变化 → Widget 更新 delegate 几何
    void selectionChanged();        // 选区变化 → SG Widget 仅更新 selection geometry
    void contentRowsDirty(QVector<int> rows); // 增量更新：部分行内容变化

private:
    void reconnectSurfaceModel();
    void disconnectSurfaceModel();
    void updateMetrics();
    void scheduleTerminalSizeSync();
    void syncTerminalSize();
    void updateSelectionFromDrag(qreal x, qreal y);

    // ── signal 连接句柄 ─────────────────────────────────────────────────────
    QMetaObject::Connection m_viewportConnection;
    QMetaObject::Connection m_modeStateConnection;
    QMetaObject::Connection m_surfaceSizeConnection;
    QMetaObject::Connection m_surfaceCursorConnection;
    QMetaObject::Connection m_surfaceSelectionConnection;
    QMetaObject::Connection m_surfaceVisibleRunsConnection;
    QMetaObject::Connection m_surfacePartialRunsConnection;
    QMetaObject::Connection m_surfaceDestroyedConnection;

    // ── 状态 ────────────────────────────────────────────────────────────────
    QPointer<QTermTerminal> m_terminal;

    QTimer *m_resizeDebounceTimer    = nullptr;
    QTimer *m_selectionAutoScrollTimer = nullptr;
    QTimer *m_clickResetTimer        = nullptr;

    QString m_fontFamily    = QStringLiteral("Menlo");
    int     m_fontPixelSize = 18;
    qreal   m_cellWidth     = 1.0;
    qreal   m_cellHeight    = 1.0;
    qreal   m_viewWidth     = 0.0;
    qreal   m_viewHeight    = 0.0;

    // ── 鼠标 / 选区内部状态 ─────────────────────────────────────────────────
    int   m_clickStreak            = 0;
    int   m_lastClickRow           = -1;
    int   m_lastClickColumn        = -1;
    int   m_selectionAnchorRow     = -1;
    int   m_selectionAnchorColumn  = -1;
    bool  m_suppressSelectionRelease = false;
    qreal m_dragX                  = 0.0;
    qreal m_dragY                  = 0.0;
    int   m_autoScrollDirection    = 0; // +1 = 向上, -1 = 向下
};

} // namespace QTerm
