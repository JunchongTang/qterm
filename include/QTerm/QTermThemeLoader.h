#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QString>

#include <QTerm/QTermTheme.h>
#include <QTerm/QTermThemePack.h>

namespace QTerm {

/*!
    \class QTermThemeLoader
    \inmodule QTerm
    \brief Loads and saves terminal theme definitions from files and JSON payloads.
*/
class QTermThemeLoader
{
public:
    // ── Load a single resolved theme ─────────────────────────────────────────
    /*!
        \brief Loads a single theme from a theme file.
        \param path The path to the .qtheme or .json theme file.
        \return The resolved theme.
    */
    static QTermTheme loadTheme(const QString &path,
                                bool    *ok          = nullptr,
                                QString *errorString = nullptr);

    /*!
        \brief Loads a theme from a JSON payload.
        \param json The JSON payload containing the theme definition.
        \return The resolved theme.
    */
    static QTermTheme loadThemeFromJson(const QByteArray &json,
                                        bool    *ok          = nullptr,
                                        QString *errorString = nullptr);

    // ── Load a pack (multi-variant container) ─────────────────────────────────
    /*!
        \brief Loads a theme pack from a file.
        \param path The path to the multi-variant theme file.
        \return The loaded pack.
    */
    static QTermThemePack loadPack(const QString &path,
                                   bool    *ok          = nullptr,
                                   QString *errorString = nullptr);

    // ── Save ─────────────────────────────────────────────────────────────────
    /*!
        \brief Saves a single theme to a file.
    */
    static bool saveTheme(const QTermTheme &theme, const QString &path,
                          QString *errorString = nullptr);

    /*!
        \brief Saves a theme pack to a file.
    */
    static bool savePack(const QTermThemePack &pack, const QString &path,
                         QString *errorString = nullptr);

    // ── Serialization ────────────────────────────────────────────────────────
    /*!
        \brief Serializes a theme to JSON.
    */
    static QByteArray toJson(const QTermTheme   &theme);
    /*!
        \brief Serializes a theme pack to JSON.
    */
    static QByteArray toJson(const QTermThemePack &pack);

private:
    // Internal helpers — declared here so they have friend access to QTermTheme.
    static QTermTheme   themeFromJsonObject(const QJsonObject &obj,
                                            const QString &fallbackName,
                                            bool *ok, QString *err);
    static QJsonObject  themeToJsonObject(const QTermTheme &theme);
};

} // namespace QTerm
