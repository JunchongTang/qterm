#ifndef QTERM_QTERMQUICKPAINTEDITEM_H
#define QTERM_QTERMQUICKPAINTEDITEM_H

#include <QColor>
#include <QPointer>
#include <QQmlComponent>
#include <QQuickPaintedItem>
#include <QString>

#include <QTerm/QTermTerminal.h>
#include <QTerm/QTermTheme.h>

namespace QTerm {

class QTermViewController; // src/quick/QTermViewController.h

class QTermQuickPaintedItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QTerm::QTermTerminal *terminal READ terminal WRITE setTerminal NOTIFY terminalChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontChanged)
    Q_PROPERTY(int fontPixelSize READ fontPixelSize WRITE setFontPixelSize NOTIFY fontChanged)
    Q_PROPERTY(qreal cellWidth READ cellWidth NOTIFY metricsChanged)
    Q_PROPERTY(qreal cellHeight READ cellHeight NOTIFY metricsChanged)
    Q_PROPERTY(QColor foregroundColor READ foregroundColor WRITE setForegroundColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor selectionColor READ selectionColor WRITE setSelectionColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor cursorColor READ cursorColor WRITE setCursorColor NOTIFY paletteChanged)
    Q_PROPERTY(qreal cursorOpacity READ cursorOpacity WRITE setCursorOpacity NOTIFY cursorOpacityChanged)
    Q_PROPERTY(QTerm::QTermQuickPaintedItem::CursorStyle cursorStyle READ cursorStyle WRITE setCursorStyle NOTIFY cursorStyleChanged)
    Q_PROPERTY(QQmlComponent *cursorDelegate READ cursorDelegate WRITE setCursorDelegate NOTIFY cursorDelegateChanged FINAL)
    // 标准化滚动属性，直接对接 QML ScrollBar 的 position / size
    Q_PROPERTY(qreal scrollPosition READ scrollPosition WRITE setScrollPosition NOTIFY scrollChanged)
    Q_PROPERTY(qreal scrollSize READ scrollSize NOTIFY scrollChanged)
    Q_PROPERTY(QTerm::QTermTheme theme READ theme WRITE setTheme NOTIFY themeChanged)

public:
    // 光标内建形状枚举。设置后使用对应的默认渲染；设置 cursorDelegate 可完全自定义。
    enum CursorStyle {
        Block,      // 填充块（默认）
        Underline,  // 单元格底部下划线
        Bar         // 左侧竖线（I-beam）
    };
    Q_ENUM(CursorStyle)

    explicit QTermQuickPaintedItem(QQuickItem *parent = nullptr);

    QTermTerminal *terminal() const noexcept;
    void setTerminal(QTermTerminal *terminal);

    QString fontFamily() const;
    void setFontFamily(const QString &fontFamily);

    int fontPixelSize() const noexcept;
    void setFontPixelSize(int fontPixelSize);

    qreal cellWidth() const noexcept;
    qreal cellHeight() const noexcept;

    QColor foregroundColor() const;
    void setForegroundColor(const QColor &foregroundColor);

    QColor backgroundColor() const;
    void setBackgroundColor(const QColor &backgroundColor);

    QColor selectionColor() const;
    void setSelectionColor(const QColor &selectionColor);

    QColor cursorColor() const;
    void setCursorColor(const QColor &cursorColor);

    qreal cursorOpacity() const noexcept;
    void setCursorOpacity(qreal cursorOpacity);

    CursorStyle cursorStyle() const noexcept;
    void setCursorStyle(CursorStyle style);

    QQmlComponent *cursorDelegate() const noexcept;
    void setCursorDelegate(QQmlComponent *delegate);

    // ScrollBar 对接：position/size 均为标准化 [0..1]，position=0 对应顶部
    qreal scrollPosition() const noexcept;
    void setScrollPosition(qreal position);
    qreal scrollSize() const noexcept;

    Q_INVOKABLE int rowAtPosition(qreal y) const;
    Q_INVOKABLE int columnAtPosition(qreal x) const;

    void paint(QPainter *painter) override;
    void componentComplete() override;

    // IME support
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

    QTermTheme theme() const;
    void setTheme(const QTermTheme &theme);

signals:
    void terminalChanged();
    void fontChanged();
    void metricsChanged();
    void paletteChanged();
    void cursorOpacityChanged();
    void cursorStyleChanged();
    void cursorDelegateChanged();
    void scrollChanged();
    void wheelScrolled(int scrollOffset);
    // 请求外部将 text 写入系统剪贴板（QML/C++ 均可连接）
    void copyRequested(const QString &text);
    // OSC 8 超链接被激活（Cmd+单击），外部决定如何打开 URL
    void hyperlinkActivated(const QString &url);
    void themeChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    // 鼠标模式变化时同步 setAcceptedMouseButtons / setAcceptHoverEvents
    void updateMouseAcceptance();

    // 创建 / 更新 delegate item 的位置、尺寸、透明度
    void recreateCursorDelegateItem();
    void updateCursorDelegateGeometry();

    // ── 共享控制器（输入处理 + 尺寸/滚动逻辑） ───────────────────────────────
    QTermViewController *m_controller = nullptr;

    // ── 主题（包含调色板和超链接色） ────────────────────────────────────────
    QTermTheme m_theme;

    // ── 调色板（渲染参数，controller 无需感知） ──────────────────────────────
    QColor m_foregroundColor = QColor(QStringLiteral("#d2f7d0"));
    QColor m_backgroundColor = QColor(QStringLiteral("#0b1016"));
    QColor m_selectionColor  = QColor(QStringLiteral("#214f76"));
    QColor m_cursorColor     = QColor(QStringLiteral("#d7fbe0"));
    qreal  m_cursorOpacity   = 1.0;
    CursorStyle m_cursorStyle = Block;

    // ── 光标 delegate ─────────────────────────────────────────────────────────
    QQmlComponent *m_cursorDelegate     = nullptr;
    QQuickItem    *m_cursorDelegateItem = nullptr;
};

} // namespace QTerm

#endif // QTERM_QTERMQUICKPAINTEDITEM_H