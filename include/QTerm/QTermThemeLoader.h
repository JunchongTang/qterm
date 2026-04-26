#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QString>

#include <QTerm/QTermTheme.h>
#include <QTerm/QTermThemePack.h>

namespace QTerm {

class QTermThemeLoader
{
public:
    // ── Load a single resolved theme ─────────────────────────────────────────
    static QTermTheme loadTheme(const QString &path,
                                bool    *ok          = nullptr,
                                QString *errorString = nullptr);

    static QTermTheme loadThemeFromJson(const QByteArray &json,
                                        bool    *ok          = nullptr,
                                        QString *errorString = nullptr);

    // ── Load a pack (multi-variant container) ─────────────────────────────────
    static QTermThemePack loadPack(const QString &path,
                                   bool    *ok          = nullptr,
                                   QString *errorString = nullptr);

    // ── Save ─────────────────────────────────────────────────────────────────
    static bool saveTheme(const QTermTheme &theme, const QString &path,
                          QString *errorString = nullptr);

    static bool savePack(const QTermThemePack &pack, const QString &path,
                         QString *errorString = nullptr);

    // ── Serialization ────────────────────────────────────────────────────────
    static QByteArray toJson(const QTermTheme   &theme);
    static QByteArray toJson(const QTermThemePack &pack);

private:
    // Internal helpers — declared here so they have friend access to QTermTheme.
    static QTermTheme   themeFromJsonObject(const QJsonObject &obj,
                                            const QString &fallbackName,
                                            bool *ok, QString *err);
    static QJsonObject  themeToJsonObject(const QTermTheme &theme);
};

} // namespace QTerm
