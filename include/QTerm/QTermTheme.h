#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace QTerm {

class QTermThemeLoader;

/*!
    \class QTermTheme
    \inmodule QTerm
    \brief Fully-resolved terminal colors and optional font overrides.

    QTermTheme is a value type that can be used from both C++ and QML. It is
    consumed directly by the widget layer and can be grouped into packs for
    dark/light variants through QTermThemePack.
*/
class QTermTheme
{
    Q_GADGET
    QML_VALUE_TYPE(termTheme)
    QML_CONSTRUCTIBLE_VALUE

    Q_PROPERTY(QString name        READ name        WRITE setName)
    Q_PROPERTY(bool    darkMode    READ darkMode    WRITE setDarkMode)
    Q_PROPERTY(QColor  foreground  READ foreground  WRITE setForeground)
    Q_PROPERTY(QColor  background  READ background  WRITE setBackground)
    Q_PROPERTY(QColor  selection   READ selection   WRITE setSelection)
    Q_PROPERTY(QColor  cursor      READ cursor      WRITE setCursor)
    Q_PROPERTY(QColor  hyperlinkTint READ hyperlinkTint WRITE setHyperlinkTint)
    // Font overrides — empty / 0 means "inherit from item's own fontFamily / fontPixelSize".
    Q_PROPERTY(QString fontFamily    READ fontFamily    WRITE setFontFamily)
    Q_PROPERTY(int     fontPixelSize READ fontPixelSize WRITE setFontPixelSize)

public:
    QTermTheme(); // defaults to dark()

    // ── Basic colors ──────────────────────────────────────────────────────────
    /*!
        \brief Returns the foreground color used for regular text.
    */
    QColor foreground()    const { return m_foreground; }
    /*!
        \brief Returns the background color used for the terminal viewport.
    */
    QColor background()    const { return m_background; }
    /*!
        \brief Returns the selection highlight color.
    */
    QColor selection()     const { return m_selection; }
    /*!
        \brief Returns the cursor color.
    */
    QColor cursor()        const { return m_cursor; }
    /*!
        \brief Returns the hyperlink tint color.
    */
    QColor hyperlinkTint() const { return m_hyperlinkTint; }

    /*!
        \brief Sets the foreground color used for regular text.
    */
    void setForeground(const QColor &c)    { m_foreground = c; }
    /*!
        \brief Sets the background color used for the terminal viewport.
    */
    void setBackground(const QColor &c)    { m_background = c; }
    /*!
        \brief Sets the selection highlight color.
    */
    void setSelection(const QColor &c)     { m_selection = c; }
    /*!
        \brief Sets the cursor color.
    */
    void setCursor(const QColor &c)        { m_cursor = c; }
    /*!
        \brief Sets the hyperlink tint color.
    */
    void setHyperlinkTint(const QColor &c) { m_hyperlinkTint = c; }

    // ── ANSI 16-color palette (index 0–15) ────────────────────────────────────
    /*!
        \brief Returns a palette color for the requested ANSI index.
        \param index The ANSI color index in the range 0 to 15.
        \return The requested color, or a fallback color if the index is invalid.
    */
    QColor paletteColor(int index) const;
    // Raw pointer to internal QColor[16], for RenderUtils direct access.
    const QColor *palette16() const { return m_palette; }
    /*!
        \brief Sets one ANSI palette entry.
        \param index The ANSI color index in the range 0 to 15.
        \param color The color to assign.
    */
    Q_INVOKABLE void setPaletteColor(int index, const QColor &color);

    // ── Font overrides ────────────────────────────────────────────────────────
    // Empty string = do not override the item's fontFamily.
    /*!
        \brief Returns the optional font family override.
    */
    QString fontFamily()   const { return m_fontFamily; }
    // 0 = do not override the item's fontPixelSize.
    /*!
        \brief Returns the optional font pixel size override.
    */
    int     fontPixelSize() const { return m_fontPixelSize; }

    /*!
        \brief Sets an optional font family override.
    */
    void setFontFamily(const QString &family) { m_fontFamily = family; }
    /*!
        \brief Sets an optional font pixel size override.
    */
    void setFontPixelSize(int size)            { m_fontPixelSize = size; }

    // ── Metadata ─────────────────────────────────────────────────────────────
    /*!
        \brief Returns the theme name.
    */
    QString name()     const { return m_name; }
    /*!
        \brief Returns whether this theme is intended for dark mode.
    */
    bool    darkMode() const { return m_darkMode; }

    /*!
        \brief Sets the theme name.
    */
    void setName(const QString &name) { m_name = name; }
    /*!
        \brief Sets whether the theme is intended for dark mode.
    */
    void setDarkMode(bool dark)       { m_darkMode = dark; }

    // ── Built-in factories — both belong to the "QTerm 2026" pack ───────────
    /*!
        \brief Returns the built-in dark theme.
    */
    static QTermTheme dark();   ///< QTerm 2026 Dark
    /*!
        \brief Returns the built-in light theme.
    */
    static QTermTheme light();  ///< QTerm 2026 Light

private:
    struct RawTag {};
    explicit QTermTheme(RawTag) noexcept {} // leaves members default-init; used by make()

    QString m_name;
    QColor  m_foreground;
    QColor  m_background;
    QColor  m_selection;
    QColor  m_cursor;
    QColor  m_hyperlinkTint{QStringLiteral("#6ab0f5")};
    QColor  m_palette[16];
    bool    m_darkMode = true;
    QString m_fontFamily;
    int     m_fontPixelSize = 0;

    // Used by built-in factory methods; accesses private members directly.
    static QTermTheme make(const char *name, bool darkMode,
                            const char *fg, const char *bg,
                            const char *sel, const char *cur,
                            const char *hyperlink,
                            const char * const p[16]);

    friend class QTermThemeLoader;
};

} // namespace QTerm
