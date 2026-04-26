#include <QTerm/QTermThemePack.h>

#include <QGuiApplication>
#include <QStyleHints>

namespace QTerm {

QTermTheme QTermThemePack::variant(const QString &name) const
{
    if (m_variants.contains(name))
        return m_variants.value(name);
    if (!m_variantOrder.isEmpty())
        return m_variants.value(m_variantOrder.first());
    return QTermTheme();
}

QTermTheme QTermThemePack::resolveForSystem() const
{
    const auto scheme = qGuiApp->styleHints()->colorScheme();
    const QString preferredName =
        (scheme == Qt::ColorScheme::Light) ? QStringLiteral("light")
                                           : QStringLiteral("dark");
    return variant(preferredName);
}

QTermThemePack QTermThemePack::qtermDefault()
{
    QTermThemePack pack;
    pack.m_name = QStringLiteral("QTerm 2026");
    pack.m_variantOrder = {QStringLiteral("dark"), QStringLiteral("light")};
    pack.m_variants.insert(QStringLiteral("dark"), QTermTheme::dark());
    pack.m_variants.insert(QStringLiteral("light"), QTermTheme::light());
    return pack;
}

} // namespace QTerm
