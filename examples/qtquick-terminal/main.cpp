#include <QGuiApplication>
#include <QClipboard>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStyleHints>

#include <QTerm/QTermQuickPaintedItem.h>
#include <QTerm/QTermQuickItem.h>
#include <QTerm/QTermTheme.h>

namespace {

class DemoClipboardBridge final : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    Q_INVOKABLE void copyText(const QString &text)
    {
        if (QGuiApplication::clipboard())
            QGuiApplication::clipboard()->setText(text);
    }

    Q_INVOKABLE QString clipboardText() const
    {
        if (QGuiApplication::clipboard())
            return QGuiApplication::clipboard()->text();
        return {};
    }
};

class DemoThemeHelper final : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // ANSI 16 colors tuned to a Windows-console-like palette.
    static QTerm::QTermTheme windowsLikeDarkTheme()
    {
        QTerm::QTermTheme theme = QTerm::QTermTheme::dark();
        theme.setName(QStringLiteral("Qt Quick Terminal Dark"));
        theme.setForeground(QColor(QStringLiteral("#F2F2F2")));
        // Matches Theme.background in the QML demo so the terminal blends
        // into the surrounding chrome.
        theme.setBackground(QColor(QStringLiteral("#18181B")));
        theme.setSelection(QColor(QStringLiteral("#2A66D9")));
        theme.setCursor(QColor(QStringLiteral("#FFFFFF")));
        theme.setHyperlinkTint(QColor(QStringLiteral("#4AA8FF")));

        static const char *palette[16] = {
            "#0C0C0C", "#C50F1F", "#13A10E", "#C19C00",
            "#0037DA", "#881798", "#3A96DD", "#CCCCCC",
            "#767676", "#E74856", "#16C60C", "#F9F1A5",
            "#3B78FF", "#B4009E", "#61D6D6", "#F2F2F2"
        };
        for (int i = 0; i < 16; ++i)
            theme.setPaletteColor(i, QColor(QString::fromLatin1(palette[i])));
        return theme;
    }

    static QTerm::QTermTheme windowsLikeLightTheme()
    {
        QTerm::QTermTheme theme = QTerm::QTermTheme::light();
        theme.setName(QStringLiteral("Qt Quick Terminal Light"));
        theme.setForeground(QColor(QStringLiteral("#1A1A1A")));
        theme.setBackground(QColor(QStringLiteral("#FFFFFF")));
        theme.setSelection(QColor(QStringLiteral("#B3D4FC")));
        theme.setCursor(QColor(QStringLiteral("#1A1A1A")));
        theme.setHyperlinkTint(QColor(QStringLiteral("#1A66C2")));

        static const char *palette[16] = {
            "#1A1A1A", "#C50F1F", "#0E7A0B", "#8A6D00",
            "#0037DA", "#7A1585", "#0E7490", "#4D4D4D",
            "#767676", "#B02532", "#118A0E", "#9C7B00",
            "#2860C4", "#8E2196", "#0F7C90", "#1A1A1A"
        };
        for (int i = 0; i < 16; ++i)
            theme.setPaletteColor(i, QColor(QString::fromLatin1(palette[i])));
        return theme;
    }

    Q_INVOKABLE void applyTheme(QObject *item, bool dark)
    {
        const auto theme = dark ? windowsLikeDarkTheme() : windowsLikeLightTheme();
        if (auto *t = qobject_cast<QTerm::QTermQuickPaintedItem *>(item))
            t->setTheme(theme);
        else if (auto *t = qobject_cast<QTerm::QTermQuickItem *>(item))
            t->loadTheme(theme);
    }

    // Requests the platform color scheme, which also repaints the native
    // window decoration (title bar) on macOS and Windows. QStyleHints only
    // gained a colorScheme setter in Qt 6.8; on older Qt the title bar stays
    // at whatever the desktop reports.
    Q_INVOKABLE void setColorScheme(bool dark)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        QGuiApplication::styleHints()->setColorScheme(dark ? Qt::ColorScheme::Dark
                                                           : Qt::ColorScheme::Light);
#else
        Q_UNUSED(dark);
#endif
    }
};

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("qtquick-terminal"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;

    DemoClipboardBridge clipboardBridge;
    DemoThemeHelper themeHelper;

    engine.rootContext()->setContextProperty(QStringLiteral("clipboardBridge"), &clipboardBridge);
    engine.rootContext()->setContextProperty(QStringLiteral("themeHelper"), &themeHelper);

    engine.loadFromModule("QtQuickTerminal", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}

#include "main.moc"

