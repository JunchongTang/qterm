#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace QTerm {

class QTermThemeLoader;

/**
 * A fully-resolved set of terminal colors and optional font settings.
 *
 * QTermTheme is a Q_GADGET value type, usable from both C++ and QML.
 * Register it with qmlRegisterValueType<QTermTheme>("QTerm", 1, 0, "TermTheme")
 * in your application's main() to expose it to QML.
 *
 * Font fields (fontFamily / fontPixelSize) are optional: an empty fontFamily or
 * fontPixelSize == 0 means "do not override the item's own font setting".
 *
 * No variant concept here — this is a single theme that the Widget can consume
 * directly. Use QTermThemePack to manage multiple variants (dark/light/etc.)
 * and call resolveForSystem() or variant() to get a QTermTheme.
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
    QColor foreground()    const { return m_foreground; }
    QColor background()    const { return m_background; }
    QColor selection()     const { return m_selection; }
    QColor cursor()        const { return m_cursor; }
    QColor hyperlinkTint() const { return m_hyperlinkTint; }

    void setForeground(const QColor &c)    { m_foreground = c; }
    void setBackground(const QColor &c)    { m_background = c; }
    void setSelection(const QColor &c)     { m_selection = c; }
    void setCursor(const QColor &c)        { m_cursor = c; }
    void setHyperlinkTint(const QColor &c) { m_hyperlinkTint = c; }

    // ── ANSI 16-color palette (index 0–15) ────────────────────────────────────
    QColor paletteColor(int index) const;
    // Raw pointer to internal QColor[16], for RenderUtils direct access.
    const QColor *palette16() const { return m_palette; }
    // Set a single palette entry; index must be in [0, 15].
    Q_INVOKABLE void setPaletteColor(int index, const QColor &color);

    // ── Font overrides ────────────────────────────────────────────────────────
    // Empty string = do not override the item's fontFamily.
    QString fontFamily()   const { return m_fontFamily; }
    // 0 = do not override the item's fontPixelSize.
    int     fontPixelSize() const { return m_fontPixelSize; }

    void setFontFamily(const QString &family) { m_fontFamily = family; }
    void setFontPixelSize(int size)            { m_fontPixelSize = size; }

    // ── Metadata ─────────────────────────────────────────────────────────────
    QString name()     const { return m_name; }
    bool    darkMode() const { return m_darkMode; }

    void setName(const QString &name) { m_name = name; }
    void setDarkMode(bool dark)       { m_darkMode = dark; }

    // ── Built-in factories — both belong to the "QTerm 2026" pack ───────────
    static QTermTheme dark();   ///< QTerm 2026 Dark
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
