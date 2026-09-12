#ifndef QTERM_QTERMQUICKPAINTEDITEM_H
#define QTERM_QTERMQUICKPAINTEDITEM_H

#include <QColor>
#include <QPointer>
#include <QQmlComponent>
#include <QQuickPaintedItem>
#include <QString>
#include <QVector>

#include <QTerm/QTermTerminal.h>
#include <QTerm/QTermTheme.h>

#include <QtQml/qqmlregistration.h>

namespace QTerm {

class QTermViewController; // src/quick/QTermViewController.h

/*!
    \qmltype QTermQuickPaintedItem
    \inqmlmodule QTerm
    \brief QPainter-based terminal item for Qt Quick.

    This item provides compatibility-oriented rendering and mirrors the public
    API of QTermQuickItem.
*/
class QTermQuickPaintedItem : public QQuickPaintedItem
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
    /*!
        Glyph colour for reverse-video (SGR 7) cells with no explicit background
        of their own. **Separate from \l backgroundColor on purpose**: that one may
        carry alpha for a translucent terminal, and a translucent glyph over a
        solid block of its own foreground renders as a smear (or vanishes at
        alpha 0). Invalid (the default) derives it from \l backgroundColor with
        alpha forced opaque -- the historical behaviour.
    */
    Q_PROPERTY(QColor inverseTextColor READ inverseTextColor WRITE setInverseTextColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor selectionColor READ selectionColor WRITE setSelectionColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor searchHighlightColor READ searchHighlightColor WRITE setSearchHighlightColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor searchCurrentColor READ searchCurrentColor WRITE setSearchCurrentColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor cursorColor READ cursorColor WRITE setCursorColor NOTIFY paletteChanged)
    Q_PROPERTY(qreal cursorOpacity READ cursorOpacity WRITE setCursorOpacity NOTIFY cursorOpacityChanged)
    Q_PROPERTY(QTerm::QTermQuickPaintedItem::CursorStyle cursorStyle READ cursorStyle WRITE setCursorStyle NOTIFY cursorStyleChanged)
    Q_PROPERTY(QQmlComponent *cursorDelegate READ cursorDelegate WRITE setCursorDelegate NOTIFY cursorDelegateChanged FINAL)
    // Normalised scroll properties, matching QML ScrollBar's position / size.
    Q_PROPERTY(qreal scrollPosition READ scrollPosition WRITE setScrollPosition NOTIFY scrollChanged)
    Q_PROPERTY(qreal scrollSize READ scrollSize NOTIFY scrollChanged)
    Q_PROPERTY(QTerm::QTermTheme theme READ theme WRITE setTheme NOTIFY themeChanged)

public:
    /*!
        \enum QTermQuickPaintedItem::CursorStyle
        \brief Built-in cursor rendering styles.
    */
    enum CursorStyle {
        Block,      // filled block (default)
        Underline,  // underline along the bottom of the cell
        Bar         // vertical bar at the left edge (I-beam)
    };
    Q_ENUM(CursorStyle)

    explicit QTermQuickPaintedItem(QQuickItem *parent = nullptr);

    /*! \brief Returns the terminal attached to this item. */
    QTermTerminal *terminal() const noexcept;
    /*! \brief Sets the terminal attached to this item. */
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
    QColor inverseTextColor() const;
    void setInverseTextColor(const QColor &inverseTextColor);
    void setBackgroundColor(const QColor &backgroundColor);

    QColor selectionColor() const;
    void setSelectionColor(const QColor &selectionColor);

    QColor searchHighlightColor() const;
    void setSearchHighlightColor(const QColor &color);

    QColor searchCurrentColor() const;
    void setSearchCurrentColor(const QColor &color);

    QColor cursorColor() const;
    void setCursorColor(const QColor &cursorColor);

    qreal cursorOpacity() const noexcept;
    void setCursorOpacity(qreal cursorOpacity);

    CursorStyle cursorStyle() const noexcept;
    void setCursorStyle(CursorStyle style);

    QQmlComponent *cursorDelegate() const noexcept;
    void setCursorDelegate(QQmlComponent *delegate);

    /*! \brief Returns the normalized scroll position in the range 0.0 to 1.0. */
    qreal scrollPosition() const noexcept;
    /*! \brief Sets the normalized scroll position in the range 0.0 to 1.0. */
    void setScrollPosition(qreal position);
    /*! \brief Returns the normalized visible scroll size in the range 0.0 to 1.0. */
    qreal scrollSize() const noexcept;

    /*! \brief Maps a vertical coordinate to a terminal row index. */
    Q_INVOKABLE int rowAtPosition(qreal y) const;
    /*! \brief Maps a horizontal coordinate to a terminal column index. */
    Q_INVOKABLE int columnAtPosition(qreal x) const;

    void paint(QPainter *painter) override;
    void componentComplete() override;

    // IME support
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

    /*! \brief Returns the currently applied resolved theme. */
    QTermTheme theme() const;
    /*! \brief Applies a resolved theme to the item. */
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
    // Asks the host to put text on the system clipboard; connect from QML or C++.
    void copyRequested(const QString &text);
    // An OSC 8 hyperlink was activated (Cmd+click); the host decides how to open it.
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
    // Keeps setAcceptedMouseButtons / setAcceptHoverEvents in step with the mouse mode.
    void updateMouseAcceptance();

    // Creates or updates the delegate item's position, size and opacity.
    void recreateCursorDelegateItem();
    void updateCursorDelegateGeometry();

    // ── Shared controller: input handling plus size and scroll logic ─────────
    QTermViewController *m_controller = nullptr;

    // ── Theme, including the palette and the hyperlink colour ────────────────
    QTermTheme m_theme;

    // ── Palette: rendering only, of no concern to the controller ─────────────
    QColor m_foregroundColor = QColor(QStringLiteral("#d2f7d0"));
    QColor m_backgroundColor = QColor(QStringLiteral("#0b1016"));
    // Invalid = derive from m_backgroundColor with alpha forced opaque.
    QColor      m_inverseTextColor;
    // Last reported reason for the cursor being drawn or not (see
    // qtermReportCursorDraw); logs only on transitions.
    QString m_cursorDrawReason;
    QColor m_selectionColor  = QColor(QStringLiteral("#214f76"));
    // Same amber pair as QTermQuickItem, so the two renderers agree.
    QColor m_searchHighlightColor{0xff, 0xd5, 0x4f, 0x66};
    QColor m_searchCurrentColor{0xff, 0xb3, 0x00, 0xcc};
    QColor m_cursorColor     = QColor(QStringLiteral("#d7fbe0"));
    qreal  m_cursorOpacity   = 1.0;
    CursorStyle m_cursorStyle = Block;

    // ── Cursor delegate ──────────────────────────────────────────────────────
    QQmlComponent *m_cursorDelegate     = nullptr;
    QQuickItem    *m_cursorDelegateItem = nullptr;

    // ── Incremental dirty rows, as 0-based visible row numbers ───────────────
    // Non-empty only when contentRowsDirty was fired without a full repaint.
    QVector<int> m_dirtyRows;
};

} // namespace QTerm

#endif // QTERM_QTERMQUICKPAINTEDITEM_H