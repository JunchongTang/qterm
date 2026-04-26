#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

#include <QTerm/QTermTheme.h>

namespace QTerm {

class QTermThemeLoader;

/**
 * A named collection of QTermTheme variants loaded from a multi-variant
 * .qtheme file (one with a top-level "variants" object).
 *
 * The Widget layer never holds a QTermThemePack — it only consumes the
 * already-resolved QTermTheme returned by variant() or resolveForSystem().
 * The application layer is responsible for choosing a variant and calling
 * setTheme() on the widget.
 */
class QTermThemePack
{
public:
    QTermThemePack() = default;

    /// Pack name (from the top-level "name" field in the .qtheme file).
    QString name() const { return m_name; }

    /// All variant names in file-insertion order.
    QStringList variantNames() const { return m_variantOrder; }

    /**
     * Returns the named variant. Falls back to the first variant if the name
     * is not found. Returns a default-constructed QTermTheme() if the pack
     * is empty.
     */
    QTermTheme variant(const QString &name) const;

    /**
     * Picks a variant based on the system color scheme preference:
     *   - Qt::ColorScheme::Dark  → variant("dark")
     *   - Qt::ColorScheme::Light → variant("light")
     * Falls back to the first variant if the expected name does not exist.
     */
    QTermTheme resolveForSystem() const;

    bool hasVariant(const QString &name) const { return m_variants.contains(name); }
    int  variantCount() const { return m_variantOrder.size(); }

    // Built-in pack factory.
    static QTermThemePack qtermDefault(); ///< "QTerm 2026" with "dark" and "light" variants

private:
    QString                   m_name;
    QStringList               m_variantOrder; ///< insertion order preserved
    QMap<QString, QTermTheme> m_variants;

    friend class QTermThemeLoader;
};

} // namespace QTerm
