#include <QTerm/QTermTheme.h>

namespace QTerm {

// ── Private static factory helper ────────────────────────────────────────────

QTermTheme QTermTheme::make(const char *name, bool darkMode,
                             const char *fg, const char *bg,
                             const char *sel, const char *cur,
                             const char *hyperlink,
                             const char * const p[16])
{
    QTermTheme t{RawTag{}};
    t.m_name = QLatin1String(name);
    t.m_darkMode = darkMode;
    t.m_foreground = QColor(QLatin1String(fg));
    t.m_background = QColor(QLatin1String(bg));
    t.m_selection = QColor(QLatin1String(sel));
    t.m_cursor = QColor(QLatin1String(cur));
    t.m_hyperlinkTint = QColor(QLatin1String(hyperlink));
    for (int i = 0; i < 16; ++i)
        t.m_palette[i] = QColor(QLatin1String(p[i]));
    return t;
}

// ── Constructor ───────────────────────────────────────────────────────────────

QTermTheme::QTermTheme() : QTermTheme(dark()) {}

// ── paletteColor ─────────────────────────────────────────────────────────────

QColor QTermTheme::paletteColor(int index) const
{
    if (index < 0 || index > 15)
        return QColor();
    return m_palette[index];
}

// ── Built-in factories ────────────────────────────────────────────────────────

QTermTheme QTermTheme::dark()
{
    static const char *p[16] = {
        "#10151c", "#ff5f56", "#27c93f", "#ffbd2e",
        "#4f8cff", "#c678dd", "#56b6c2", "#dce7f3",
        "#5b6574", "#ff8f88", "#58d26a", "#ffd866",
        "#7aa2ff", "#d7a6ff", "#7dd3d8", "#f5fbff"
    };
    return make("QTerm 2026 Dark", true,
                "#d2f7d0", "#0b1016", "#214f76", "#d7fbe0", "#6ab0f5", p);
}

QTermTheme QTermTheme::light()
{
    static const char *p[16] = {
        "#282c34", "#e06c75", "#98c379", "#e5c07b",
        "#61afef", "#c678dd", "#56b6c2", "#abb2bf",
        "#5c6370", "#e06c75", "#98c379", "#e5c07b",
        "#61afef", "#c678dd", "#56b6c2", "#ffffff"
    };
    return make("QTerm 2026 Light", false,
                "#383a42", "#fafafa", "#c8d3e1", "#526fff", "#2d6fb5", p);
}

} // namespace QTerm
