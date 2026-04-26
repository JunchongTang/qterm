#ifndef QTERM_QTERMQUICKITEM_H
#define QTERM_QTERMQUICKITEM_H

#include <QColor>
#include <QPointer>
#include <QQmlComponent>
#include <QQuickItem>
#include <QString>

#include <QTerm/QTermTerminal.h>
#include <QTerm/QTermTheme.h>

#include <QtQml/qqmlregistration.h>

namespace QTerm {

class QTermViewController;

// High-performance Qt Scene Graph terminal renderer.
// Replaces QTermQuickPaintedItem's QPainter-to-texture path with a native QSG
// node tree: colored background geometry, QSGTextNode-per-row for text, and
// flat-color geometry nodes for selection and cursor.
//
// Dirty-flag system:
//   Full rebuild  – font/size/terminal change
//   Content dirty – visibleLineRunsChanged  → bg fills + text nodes repopulated
//   Selection dirty – selectionChanged      → selection geometry only
//   Cursor dirty  – cursorChanged           → cursor geometry only
//
// The public API is a strict superset of QTermQuickPaintedItem so QML that
// uses the older item can switch by changing just the type name.
class QTermQuickItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
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
    Q_PROPERTY(QTerm::QTermQuickItem::CursorStyle cursorStyle READ cursorStyle WRITE setCursorStyle NOTIFY cursorStyleChanged)
    Q_PROPERTY(QQmlComponent *cursorDelegate READ cursorDelegate WRITE setCursorDelegate NOTIFY cursorDelegateChanged FINAL)
    Q_PROPERTY(qreal scrollPosition READ scrollPosition WRITE setScrollPosition NOTIFY scrollChanged)
    Q_PROPERTY(qreal scrollSize READ scrollSize NOTIFY scrollChanged)

public:
    // Cursor built-in shapes; values deliberately match QTermQuickPaintedItem::CursorStyle.
    enum CursorStyle {
        Block,
        Underline,
        Bar
    };
    Q_ENUM(CursorStyle)

    explicit QTermQuickItem(QQuickItem *parent = nullptr);

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

    qreal scrollPosition() const noexcept;
    void setScrollPosition(qreal position);
    qreal scrollSize() const noexcept;

    Q_INVOKABLE int rowAtPosition(qreal y) const;
    Q_INVOKABLE int columnAtPosition(qreal x) const;

    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void componentComplete() override;

    // Theme API – same as QTermQuickPaintedItem.
    Q_INVOKABLE void loadTheme(const QTerm::QTermTheme &theme);

signals:
    void terminalChanged();
    void fontChanged();
    void metricsChanged();
    void paletteChanged();
    void cursorOpacityChanged();
    void cursorStyleChanged();
    void cursorDelegateChanged();
    void scrollChanged();
    void wheelScrolled(qreal angleDelta);
    void copyRequested();
    void hyperlinkActivated(const QString &url);

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;
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
    void updateMouseAcceptance();
    void recreateCursorDelegateItem();
    void updateCursorDelegateGeometry();
    void scheduleFullDirty();
    void scheduleContentDirty();
    void scheduleSelectionDirty();
    void scheduleCursorDirty();

    QTermViewController *m_controller = nullptr;
    QTermTheme m_theme;

    QColor m_foregroundColor{QStringLiteral("#dce7f3")};
    QColor m_backgroundColor{QStringLiteral("#0a0f15")};
    QColor m_selectionColor{0x46, 0x82, 0xc8, 0x80};
    QColor m_cursorColor{QStringLiteral("#dce7f3")};
    qreal m_cursorOpacity = 0.8;
    CursorStyle m_cursorStyle = Block;

    QQmlComponent *m_cursorDelegate = nullptr;
    QPointer<QQuickItem> m_cursorDelegateItem;

    // Dirty flags read inside updatePaintNode (render thread; GUI thread is
    // blocked at that point so plain bool access is safe).
    bool m_fullDirty = true;
    bool m_contentDirty = true;
    bool m_selectionDirty = true;
    bool m_cursorDirty = true;
    bool m_hasFocus = false;
};

} // namespace QTerm

#endif // QTERM_QTERMQUICKITEM_H
