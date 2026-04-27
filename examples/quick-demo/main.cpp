#include <QGuiApplication>
#include <QClipboard>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include <QTerm/QTermQuickPaintedItem.h>
#include <QTerm/QTermQuickItem.h>
#include <QTerm/QTermThemePack.h>

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

    Q_INVOKABLE void applyDarkTheme(QObject *item)
    {
        const auto theme = QTerm::QTermThemePack::qtermDefault().variant(QStringLiteral("dark"));
        if (auto *t = qobject_cast<QTerm::QTermQuickPaintedItem *>(item))
            t->setTheme(theme);
        else if (auto *t = qobject_cast<QTerm::QTermQuickItem *>(item))
            t->loadTheme(theme);
    }

    Q_INVOKABLE void applyLightTheme(QObject *item)
    {
        const auto theme = QTerm::QTermThemePack::qtermDefault().variant(QStringLiteral("light"));
        if (auto *t = qobject_cast<QTerm::QTermQuickPaintedItem *>(item))
            t->setTheme(theme);
        else if (auto *t = qobject_cast<QTerm::QTermQuickItem *>(item))
            t->loadTheme(theme);
    }
};

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;

    DemoClipboardBridge clipboardBridge;
    DemoThemeHelper themeHelper;

    engine.rootContext()->setContextProperty(QStringLiteral("clipboardBridge"), &clipboardBridge);
    engine.rootContext()->setContextProperty(QStringLiteral("themeHelper"), &themeHelper);

    engine.loadFromModule("QTermDemo", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}

#include "main.moc"

