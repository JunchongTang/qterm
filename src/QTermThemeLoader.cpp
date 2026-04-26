#include <QTerm/QTermThemeLoader.h>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace QTerm {

// ── JSON key → palette index mapping ─────────────────────────────────────────

static const struct { const char *key; int index; } kPaletteKeys[] = {
    {"black", 0}, {"red", 1}, {"green", 2}, {"yellow", 3},
    {"blue", 4}, {"magenta", 5}, {"cyan", 6}, {"white", 7},
    {"brightBlack", 8}, {"brightRed", 9}, {"brightGreen", 10}, {"brightYellow", 11},
    {"brightBlue", 12}, {"brightMagenta", 13}, {"brightCyan", 14}, {"brightWhite", 15}
};

// ── Private helpers (QTermThemeLoader methods → have friend access) ───────────

QTermTheme QTermThemeLoader::themeFromJsonObject(const QJsonObject &obj,
                                                  const QString &fallbackName,
                                                  bool *ok, QString *err)
{
    auto fail = [&](const QString &msg) -> QTermTheme {
        if (ok)  *ok  = false;
        if (err) *err = msg;
        return QTermTheme();
    };

    const QJsonObject palette = obj.value(QLatin1String("palette")).toObject();
    if (palette.isEmpty())
        return fail(QStringLiteral("Missing or empty 'palette' object"));

    for (const auto &entry : kPaletteKeys) {
        if (!palette.contains(QLatin1String(entry.key)))
            return fail(QStringLiteral("Missing palette key: ") + QLatin1String(entry.key));
    }

    QTermTheme t;
    t.m_name = obj.value(QLatin1String("name")).toString(fallbackName);
    t.m_darkMode = obj.value(QLatin1String("darkMode")).toBool(true);
    t.m_foreground = QColor(obj.value(QLatin1String("foreground")).toString());
    t.m_background = QColor(obj.value(QLatin1String("background")).toString());
    t.m_selection = QColor(obj.value(QLatin1String("selection")).toString());
    t.m_cursor = QColor(obj.value(QLatin1String("cursor")).toString());

    const QString hyperlink = obj.value(QLatin1String("hyperlinkTint")).toString();
    if (!hyperlink.isEmpty()) t.m_hyperlinkTint = QColor(hyperlink);

    for (const auto &entry : kPaletteKeys)
        t.m_palette[entry.index] = QColor(palette.value(QLatin1String(entry.key)).toString());

    if (!t.m_foreground.isValid())
        return fail(QStringLiteral("Invalid 'foreground' color"));
    if (!t.m_background.isValid())
        return fail(QStringLiteral("Invalid 'background' color"));
    if (!t.m_selection.isValid())
        return fail(QStringLiteral("Invalid 'selection' color"));
    if (!t.m_cursor.isValid())
        return fail(QStringLiteral("Invalid 'cursor' color"));

    if (ok) *ok = true;
    return t;
}

QJsonObject QTermThemeLoader::themeToJsonObject(const QTermTheme &t)
{
    QJsonObject obj;
    obj[QLatin1String("name")] = t.m_name;
    obj[QLatin1String("version")] = 1;
    obj[QLatin1String("darkMode")] = t.m_darkMode;
    obj[QLatin1String("foreground")] = t.m_foreground.name(QColor::HexRgb);
    obj[QLatin1String("background")] = t.m_background.name(QColor::HexRgb);
    obj[QLatin1String("selection")] = t.m_selection.name(QColor::HexRgb);
    obj[QLatin1String("cursor")] = t.m_cursor.name(QColor::HexRgb);
    obj[QLatin1String("hyperlinkTint")] = t.m_hyperlinkTint.name(QColor::HexRgb);

    QJsonObject palette;
    for (const auto &entry : kPaletteKeys)
        palette[QLatin1String(entry.key)] = t.m_palette[entry.index].name(QColor::HexRgb);
    obj[QLatin1String("palette")] = palette;
    return obj;
}

// ── Internal: parse root JSON ─────────────────────────────────────────────────

static QJsonObject parseRootOrError(const QByteArray &json, bool *ok, QString *err)
{
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseErr);
    if (doc.isNull()) {
        if (ok)  *ok  = false;
        if (err) *err = parseErr.errorString();
        return {};
    }
    if (!doc.isObject()) {
        if (ok)  *ok  = false;
        if (err) *err = QStringLiteral("Root JSON value is not an object");
        return {};
    }
    return doc.object();
}

// ── Public API ────────────────────────────────────────────────────────────────

QTermTheme QTermThemeLoader::loadTheme(const QString &path, bool *ok, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (ok)  *ok  = false;
        if (err) *err = f.errorString();
        return {};
    }
    return loadThemeFromJson(f.readAll(), ok, err);
}

QTermTheme QTermThemeLoader::loadThemeFromJson(const QByteArray &json, bool *ok, QString *err)
{
    bool localOk = false;
    const QJsonObject root = parseRootOrError(json, &localOk, err);
    if (!localOk) { if (ok) *ok = false; return {}; }

    const QString name = root.value(QLatin1String("name")).toString();

    const QJsonObject variants = root.value(QLatin1String("variants")).toObject();
    if (!variants.isEmpty()) {
        const QString firstKey = variants.keys().first();
        QJsonObject varObj = variants.value(firstKey).toObject();
        if (!varObj.contains(QLatin1String("name")))
            varObj[QLatin1String("name")] = name + QStringLiteral(" ") + firstKey;
        return themeFromJsonObject(varObj, name, ok, err);
    }

    return themeFromJsonObject(root, name, ok, err);
}

QTermThemePack QTermThemeLoader::loadPack(const QString &path, bool *ok, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (ok)  *ok  = false;
        if (err) *err = f.errorString();
        return {};
    }

    bool localOk = false;
    const QJsonObject root = parseRootOrError(f.readAll(), &localOk, err);
    if (!localOk) { if (ok) *ok = false; return {}; }

    const QString packName = root.value(QLatin1String("name")).toString();
    QTermThemePack pack;
    pack.m_name = packName;

    const QJsonObject variants = root.value(QLatin1String("variants")).toObject();
    if (!variants.isEmpty()) {
        for (const QString &key : variants.keys()) {
            QJsonObject varObj = variants.value(key).toObject();
            if (!varObj.contains(QLatin1String("name")))
                varObj[QLatin1String("name")] = packName + QStringLiteral(" ") + key;
            bool varOk = false;
            QString varErr;
            const QTermTheme t = themeFromJsonObject(varObj, key, &varOk, &varErr);
            if (!varOk) {
                if (ok)  *ok  = false;
                if (err) *err = QStringLiteral("Variant '%1': %2").arg(key, varErr);
                return {};
            }
            pack.m_variantOrder.append(key);
            pack.m_variants.insert(key, t);
        }
    } else {
        bool varOk = false;
        const QTermTheme t = themeFromJsonObject(root, packName, &varOk, err);
        if (!varOk) { if (ok) *ok = false; return {}; }
        pack.m_variantOrder.append(QStringLiteral("default"));
        pack.m_variants.insert(QStringLiteral("default"), t);
    }

    if (ok) *ok = true;
    return pack;
}

bool QTermThemeLoader::saveTheme(const QTermTheme &theme, const QString &path, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = f.errorString();
        return false;
    }
    f.write(QJsonDocument(themeToJsonObject(theme)).toJson());
    return true;
}

bool QTermThemeLoader::savePack(const QTermThemePack &pack, const QString &path, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = f.errorString();
        return false;
    }
    f.write(toJson(pack));
    return true;
}

QByteArray QTermThemeLoader::toJson(const QTermTheme &theme)
{
    return QJsonDocument(themeToJsonObject(theme)).toJson();
}

QByteArray QTermThemeLoader::toJson(const QTermThemePack &pack)
{
    QJsonObject root;
    root[QLatin1String("name")]    = pack.name();
    root[QLatin1String("version")] = 1;

    QJsonObject variants;
    for (const QString &key : pack.variantNames()) {
        QJsonObject varObj = themeToJsonObject(pack.variant(key));
        varObj.remove(QLatin1String("name"));
        varObj.remove(QLatin1String("version"));
        variants[key] = varObj;
    }
    root[QLatin1String("variants")] = variants;
    return QJsonDocument(root).toJson();
}

} // namespace QTerm
