#include <QGuiApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QQmlApplicationEngine>
#include <QVariant>
#include <qqml.h>

#include <QTerm/QTermLocalPtyBackend.h>
#include <QTerm/QTermQuickPaintedItem.h>
#include <QTerm/QTermSession.h>
#include <QTerm/QTermTerminal.h>
#include <QTerm/QTermThemePack.h>

#include <QTerm/QTermSurfaceModel.h>

namespace {

class DemoClipboardBridge final : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    Q_INVOKABLE void copyText(const QString &text)
    {
        if (QGuiApplication::clipboard()) {
            QGuiApplication::clipboard()->setText(text);
        }
    }

    Q_INVOKABLE QString clipboardText() const
    {
        if (QGuiApplication::clipboard()) {
            return QGuiApplication::clipboard()->text();
        }
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
        auto *t = qobject_cast<QTerm::QTermQuickPaintedItem *>(item);
        if (t)
            t->setTheme(QTerm::QTermThemePack::qtermDefault().variant(QStringLiteral("dark")));
    }

    Q_INVOKABLE void applyLightTheme(QObject *item)
    {
        auto *t = qobject_cast<QTerm::QTermQuickPaintedItem *>(item);
        if (t)
            t->setTheme(QTerm::QTermThemePack::qtermDefault().variant(QStringLiteral("light")));
    }
};

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    qmlRegisterUncreatableType<QTerm::QTermTerminal>(
        "QTerm", 1, 0, "QTermTerminal", "QTermTerminal is provided by the application.");
    qmlRegisterUncreatableType<QTerm::QTermSurfaceModel>(
        "QTerm", 1, 0, "QTermSurfaceModel", "QTermSurfaceModel is provided by QTermTerminal.");
    qmlRegisterType<QTerm::QTermQuickPaintedItem>("QTerm", 1, 0, "QTermQuickPaintedItem");

    QTerm::QTermTerminal terminal;
    QTerm::QTermSession session;
    QTerm::QTermLocalPtyBackend backend;
    DemoClipboardBridge clipboardBridge;
    DemoThemeHelper themeHelper;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    const QString shellProgram = environment.value(QStringLiteral("SHELL"), QStringLiteral("/bin/zsh"));
    const QString shellName = QFileInfo(shellProgram).fileName();

    backend.setProgram(shellProgram);
    if (shellName == QStringLiteral("bash") || shellName == QStringLiteral("zsh")) {
        backend.setArguments({QStringLiteral("-il")});
    } else {
        backend.setArguments({QStringLiteral("-i")});
    }
    backend.setProcessEnvironment(environment);

    session.setBackend(&backend);
    terminal.setSession(&session);
    terminal.setTitle(QStringLiteral("QTerm Local PTY Demo"));
    session.open();

    engine.setInitialProperties({
        {QStringLiteral("terminal"), QVariant::fromValue(&terminal)},
        {QStringLiteral("clipboardBridge"), QVariant::fromValue(&clipboardBridge)},
        {QStringLiteral("themeHelper"), QVariant::fromValue(&themeHelper)}
    });
    engine.loadFromModule("QTermDemo", "Main");

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}

#include "main.moc"