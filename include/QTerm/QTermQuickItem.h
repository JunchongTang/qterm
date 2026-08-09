#ifndef QTERM_QTERMQUICKITEM_H
#define QTERM_QTERMQUICKITEM_H

#include <QColor>
#include <QPointer>
#include <QQmlComponent>
#include <QQuickItem>
#include <QTimer>

#include <memory>
#include <QElapsedTimer>
#include <QString>
#include <QVector>

#include <QTerm/QTermTerminal.h>
#include <QTerm/QTermTheme.h>

#include <QtQml/qqmlregistration.h>

QT_BEGIN_NAMESPACE
class QSGTexture;
QT_END_NAMESPACE

namespace QTerm {

class QTermGlyphAtlas;

class QTermViewController;

/*!
    \qmltype QTermQuickItem
    \inqmlmodule QTerm
    \brief High-performance Scene Graph terminal item for Qt Quick.

    QTermQuickItem renders terminal content directly with QSG nodes and exposes
    the same high-level API as QTermQuickPaintedItem for easier migration.
*/
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
    /*!
        Glyph colour for reverse-video (SGR 7) cells that carry no explicit
        background of their own.

        Reverse video swaps the resolved colours: the block is painted with the
        cell's foreground, the glyph with its background. A cell that never set a
        background falls back to the theme default — which is \l backgroundColor.

        That fallback breaks the moment \l backgroundColor carries alpha, as it
        must for a translucent terminal: the glyph is then drawn semi-transparent
        over a solid block of its own foreground, or vanishes outright at alpha 0.
        Set this to the opaque colour reverse video should swap to.

        Invalid (the default) means "derive from \l backgroundColor with alpha
        forced opaque", which reproduces the historical behaviour exactly.
    */
    Q_PROPERTY(QColor inverseTextColor READ inverseTextColor WRITE setInverseTextColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor selectionColor READ selectionColor WRITE setSelectionColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor searchHighlightColor READ searchHighlightColor WRITE setSearchHighlightColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor searchCurrentColor READ searchCurrentColor WRITE setSearchCurrentColor NOTIFY paletteChanged)
    Q_PROPERTY(QColor cursorColor READ cursorColor WRITE setCursorColor NOTIFY paletteChanged)
    Q_PROPERTY(qreal cursorOpacity READ cursorOpacity WRITE setCursorOpacity NOTIFY cursorOpacityChanged)
    Q_PROPERTY(QTerm::QTermQuickItem::CursorStyle cursorStyle READ cursorStyle WRITE setCursorStyle NOTIFY cursorStyleChanged)
    Q_PROPERTY(QQmlComponent *cursorDelegate READ cursorDelegate WRITE setCursorDelegate NOTIFY cursorDelegateChanged FINAL)
    Q_PROPERTY(qreal scrollPosition READ scrollPosition WRITE setScrollPosition NOTIFY scrollChanged)
    Q_PROPERTY(qreal scrollSize READ scrollSize NOTIFY scrollChanged)

public:
    /*!
        \enum QTermQuickItem::CursorStyle
        \brief Built-in cursor rendering styles.
    */
    enum CursorStyle {
        Block,
        Underline,
        Bar
    };
    Q_ENUM(CursorStyle)

    explicit QTermQuickItem(QQuickItem *parent = nullptr);
    // Defined in the .cpp: m_glyphAtlas holds an incomplete type here, since
    // the atlas is an implementation detail rather than part of the API.
    ~QTermQuickItem() override;

    /*! \brief Returns the terminal attached to this item. */
    QTermTerminal *terminal() const noexcept;
    /*! \brief Sets the terminal attached to this item. */
    void setTerminal(QTermTerminal *terminal);

    /*! \brief Returns the font family used to render text. */
    QString fontFamily() const;
    /*! \brief Sets the font family used to render text. */
    void setFontFamily(const QString &fontFamily);

    int fontPixelSize() const noexcept;
    void setFontPixelSize(int fontPixelSize);

    qreal cellWidth() const noexcept;
    qreal cellHeight() const noexcept;

    QColor foregroundColor() const;
    void setForegroundColor(const QColor &foregroundColor);

    QColor backgroundColor() const;
    void setBackgroundColor(const QColor &backgroundColor);

    QColor inverseTextColor() const;
    void setInverseTextColor(const QColor &inverseTextColor);
    //! Resolved colour reverse video swaps to. Never invalid, never translucent.
    QColor effectiveInverseTextColor() const;

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

    /*! \brief Returns the current normalized scroll position in the range 0.0 to 1.0. */
    qreal scrollPosition() const noexcept;
    /*! \brief Sets the normalized scroll position in the range 0.0 to 1.0. */
    void setScrollPosition(qreal position);
    /*! \brief Returns the normalized visible scroll size in the range 0.0 to 1.0. */
    qreal scrollSize() const noexcept;

    /*! \brief Maps a vertical coordinate to a terminal row index. */
    Q_INVOKABLE int rowAtPosition(qreal y) const;
    /*! \brief Maps a horizontal coordinate to a terminal column index. */
    Q_INVOKABLE int columnAtPosition(qreal x) const;

    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void componentComplete() override;

    /*! \brief Applies a resolved theme to the item. */
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
    // Ctrl (⌘ on macOS) + wheel zoom intent: steps>0 zoom in, <0 zoom out.
    // The host QML connects this to adjust the terminal font size.
    void zoomRequested(int steps);
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
    void scheduleRowsDirty(QVector<int> rows);
    // Requests a frame, but never more often than the display can show one.
    // Output arriving faster than the refresh rate is coalesced into a single
    // repaint instead of producing frames nobody sees.
    void requestFrame();
    int minimumFrameIntervalMs() const;

    QTermViewController *m_controller = nullptr;
    QTermTheme m_theme;

    QColor m_foregroundColor{QStringLiteral("#dce7f3")};
    QColor m_backgroundColor{QStringLiteral("#0a0f15")};
    // Invalid = derive from m_backgroundColor with alpha forced opaque.
    QColor m_inverseTextColor;
    QColor m_selectionColor{0x46, 0x82, 0xc8, 0x80};
    QColor m_searchHighlightColor{0xff, 0xd5, 0x4f, 0x66};  // dim amber, all matches
    QColor m_searchCurrentColor{0xff, 0xb3, 0x00, 0xcc};    // bright amber, current match
    QColor m_cursorColor{QStringLiteral("#dce7f3")};
    qreal m_cursorOpacity = 0.8;
    CursorStyle m_cursorStyle = Block;

    QQmlComponent *m_cursorDelegate = nullptr;
    QPointer<QQuickItem> m_cursorDelegateItem;

    // Dirty flags read inside updatePaintNode (render thread; GUI thread is
    // blocked at that point so plain bool access is safe).
    QElapsedTimer m_lastFrameRequest;
    QTimer m_frameCoalesceTimer;
    bool m_fullDirty = true;
    bool m_contentDirty = true;
    bool m_selectionDirty = true;
    bool m_cursorDirty = true;
    bool m_hasFocus = false;
    // Rows that need incremental rebuild (only used when !m_contentDirty).
    QVector<int> m_dirtyRowSet;

    // ── Glyph atlas text path (opt-in via QTERM_GLYPH_ATLAS) ─────────────────
    // A terminal is a fixed grid, so the shaping QTextLayout performs on every
    // rebuild cannot change the result. This path looks glyphs up instead.
    std::unique_ptr<QTermGlyphAtlas> m_glyphAtlas;
    QSGTexture *m_atlasTexture = nullptr;
    quint32 m_atlasTextureGeneration = 0;
};

} // namespace QTerm

#endif // QTERM_QTERMQUICKITEM_H
