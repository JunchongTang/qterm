#pragma once

#include <QColor>
#include <QString>

namespace QTerm {

class QTermThemeLoader;

/**
 * A fully-resolved set of terminal colors.
 * No variant concept here — this is a single theme that the Widget can consume
 * directly. Use QTermThemePack to manage multiple variants (dark/light/etc.)
 * and call resolveForSystem() or variant() to get a QTermTheme.
 */
class QTermTheme
{
public:
    QTermTheme(); // defaults to dark()

    // ── Basic colors ──────────────────────────────────────────────────────────
    QColor foreground()    const { return m_foreground; }
    QColor background()    const { return m_background; }
    QColor selection()     const { return m_selection; }
    QColor cursor()        const { return m_cursor; }
    QColor hyperlinkTint() const { return m_hyperlinkTint; }

    // ── ANSI 16-color palette (index 0–15) ────────────────────────────────────
    QColor paletteColor(int index) const;
    // Raw pointer to internal QColor[16], for RenderUtils direct access.
    const QColor *palette16() const { return m_palette; }

    // ── Metadata ─────────────────────────────────────────────────────────────
    QString name()     const { return m_name; }
    bool    darkMode() const { return m_darkMode; }

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

    // Used by built-in factory methods; accesses private members directly.
    static QTermTheme make(const char *name, bool darkMode,
                            const char *fg, const char *bg,
                            const char *sel, const char *cur,
                            const char *hyperlink,
                            const char * const p[16]);

    friend class QTermThemeLoader;
};

} // namespace QTerm
