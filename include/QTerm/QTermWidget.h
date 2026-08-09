#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QVector>
#include <QWidget>

#include <QTerm/QTermTerminal.h>
#include <QTerm/QTermTheme.h>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace QTerm {

class QTermViewController;

/*!
    \class QTermWidget
    \inmodule QTerm
    \brief Widget-based terminal view for embedding a QTermTerminal in Qt Widgets applications.
*/
class QTermWidget : public QWidget
{
    Q_OBJECT
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
    Q_PROPERTY(QTerm::QTermWidget::CursorStyle cursorStyle READ cursorStyle WRITE setCursorStyle NOTIFY cursorStyleChanged)
    Q_PROPERTY(qreal scrollPosition READ scrollPosition WRITE setScrollPosition NOTIFY scrollChanged)
    Q_PROPERTY(qreal scrollSize READ scrollSize NOTIFY scrollChanged)
    Q_PROPERTY(QTerm::QTermTheme theme READ theme WRITE setTheme NOTIFY themeChanged)

public:
    enum CursorStyle {
        Block     = 0,
        Underline = 1,
        Bar       = 2
    };
    Q_ENUM(CursorStyle)

    explicit QTermWidget(QWidget *parent = nullptr);

    /*!
        \brief Returns the terminal instance attached to the widget.
        \return The current terminal, or nullptr if none is set.
    */
    QTermTerminal *terminal() const noexcept;

    /*!
        \brief Attaches a terminal to the widget.
        \param terminal The terminal object that provides the buffer and input/output state.
    */
    void setTerminal(QTermTerminal *terminal);

    /*!
        \brief Returns the font family used for rendering terminal text.
    */
    QString fontFamily() const;

    /*!
        \brief Sets the font family used for rendering terminal text.
        \param family The font family name.
    */
    void setFontFamily(const QString &family);

    /*!
        \brief Returns the pixel size of the terminal font.
    */
    int fontPixelSize() const noexcept;

    /*!
        \brief Sets the pixel size of the terminal font.
        \param size The new font size in pixels.
    */
    void setFontPixelSize(int size);

    qreal cellWidth() const noexcept;
    qreal cellHeight() const noexcept;

    QColor foregroundColor() const;
    void setForegroundColor(const QColor &color);

    QColor backgroundColor() const;
    QColor inverseTextColor() const;
    void setInverseTextColor(const QColor &inverseTextColor);
    void setBackgroundColor(const QColor &color);

    QColor selectionColor() const;
    void setSelectionColor(const QColor &color);

    QColor searchHighlightColor() const;
    void setSearchHighlightColor(const QColor &color);

    QColor searchCurrentColor() const;
    void setSearchCurrentColor(const QColor &color);

    QColor cursorColor() const;
    void setCursorColor(const QColor &color);

    qreal cursorOpacity() const noexcept;
    void setCursorOpacity(qreal opacity);

    CursorStyle cursorStyle() const noexcept;
    void setCursorStyle(CursorStyle style);

    qreal scrollPosition() const noexcept;
    void setScrollPosition(qreal position);
    qreal scrollSize() const noexcept;

    /*!
        \brief Converts a vertical widget position into a terminal row index.
        \param y The y coordinate in widget coordinates.
        \return The corresponding row number, or -1 if the position is outside the viewport.
    */
    Q_INVOKABLE int rowAtPosition(qreal y) const;

    /*!
        \brief Converts a horizontal widget position into a terminal column index.
        \param x The x coordinate in widget coordinates.
        \return The corresponding column number, or -1 if the position is outside the viewport.
    */
    Q_INVOKABLE int columnAtPosition(qreal x) const;

    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    QSize sizeHint() const override;

    /*!
        \brief Returns the currently applied terminal theme.
        \return The resolved theme used for rendering.
    */
    QTermTheme theme() const;

    /*!
        \brief Applies a new theme to the widget.
        \param theme The theme to use for colors and optional font overrides.
    */
    void setTheme(const QTermTheme &theme);

signals:
    void terminalChanged();
    void fontChanged();
    void metricsChanged();
    void paletteChanged();
    void cursorOpacityChanged();
    void cursorStyleChanged();
    void scrollChanged();
    void wheelScrolled(int scrollOffset);
    // Ctrl (⌘ on macOS) + wheel zoom intent: steps>0 zoom in, <0 zoom out.
    // The host connects this to adjust the terminal font size.
    void zoomRequested(int steps);
    void copyRequested(const QString &text);
    void hyperlinkActivated(const QString &url);
    void themeChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void updateMouseAcceptance();

    QTermViewController *m_controller  = nullptr;

    QTermTheme  m_theme;   // current theme; individual color members follow it
    QColor      m_foregroundColor = QColor(QStringLiteral("#d2f7d0"));
    QColor      m_backgroundColor = QColor(QStringLiteral("#0b1016"));
    // Invalid = derive from m_backgroundColor with alpha forced opaque.
    QColor      m_inverseTextColor;
    QColor      m_selectionColor  = QColor(QStringLiteral("#214f76"));
    // Same amber pair as QTermQuickItem, so the two renderers agree.
    QColor      m_searchHighlightColor{0xff, 0xd5, 0x4f, 0x66};  // dim, all matches
    QColor      m_searchCurrentColor{0xff, 0xb3, 0x00, 0xcc};    // bright, current match
    QColor      m_cursorColor     = QColor(QStringLiteral("#d7fbe0"));
    qreal       m_cursorOpacity   = 1.0;
    CursorStyle m_cursorStyle     = Block;

    // Incremental dirty-row set; non-empty only between contentRowsDirty and paint.
    QVector<int> m_dirtyRows;

    /*
        Repaint coalescing.

        QQuickItem::update() only sets a dirty flag and the scene graph decides
        when to render, so a Qt Quick view repaints at most once per vsync no
        matter how often the terminal changes. QWidget::update() posts an
        UpdateRequest that the event loop delivers on its very next pass, so a
        widget repaints roughly once per PTY read -- on a 16 MB payload that
        measured 16680 full-screen repaints against the ~70 the scene graph
        needed, and 93% of the wall time went into paintEvent.

        Updates are therefore held back to one per frame interval. The first
        change after an idle period still paints immediately, so typing latency
        is unaffected; only a burst is throttled, and the trailing timer
        guarantees the final state is drawn.
    */
    void scheduleUpdate(const QRect &rect = QRect());
    void flushPendingUpdate();
    int frameIntervalMs() const;

    QTimer *m_repaintTimer = nullptr;
    QElapsedTimer m_sinceLastPaint;
    QRect m_pendingRect;
    bool m_pendingFull = false;
};

} // namespace QTerm
