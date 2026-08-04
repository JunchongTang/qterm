#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

#include <QTerm/QTermTheme.h>

namespace QTerm {

class QTermThemeLoader;

/*!
    \class QTermThemePack
    \inmodule QTerm
    \brief A named collection of theme variants used for dark/light switching.

    The widget layer consumes a resolved QTermTheme directly, while the
    application layer can use QTermThemePack to choose an appropriate variant.
*/
class QTermThemePack
{
public:
    QTermThemePack() = default;

    /*!
        \brief Returns the pack name from the source theme file.
    */
    QString name() const { return m_name; }

    /*!
        \brief Returns the variant names in insertion order.
    */
    QStringList variantNames() const { return m_variantOrder; }

    /*!
        \brief Returns the named variant.
        \param name The variant name to look up.
        \return The matching variant, or the first variant if no exact match exists.
    */
    QTermTheme variant(const QString &name) const;

    /*!
        \brief Resolves the most suitable variant for the current system color scheme.
        \return The themed variant matching the current system preference.
    */
    QTermTheme resolveForSystem() const;

    /*!
        \brief Returns whether a variant with the given name exists.
    */
    bool hasVariant(const QString &name) const { return m_variants.contains(name); }

    /*!
        \brief Returns the number of variants in the pack.
    */
    int  variantCount() const { return m_variantOrder.size(); }

    /*!
        \brief Returns the built-in QTerm default pack.
    */
    static QTermThemePack qtermDefault(); ///< "QTerm 2026" with "dark" and "light" variants

private:
    QString                   m_name;
    QStringList               m_variantOrder; ///< insertion order preserved
    QMap<QString, QTermTheme> m_variants;

    friend class QTermThemeLoader;
};

} // namespace QTerm
