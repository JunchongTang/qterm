#include <QGuiApplication>
#include <QClipboard>
#include <QProcessEnvironment>
#include <QQmlApplicationEngine>
#include <QVariant>
#include <qqml.h>

#include <QTerm/QTermLocalPtyBackend.h>
#include <QTerm/QTermSession.h>
#include <QTerm/QTermTerminal.h>

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

    QTerm::QTermTerminal terminal;
    QTerm::QTermSession session;
    QTerm::QTermLocalPtyBackend backend;
    DemoClipboardBridge clipboardBridge;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    environment.insert(QStringLiteral("PS1"), QString::fromUtf8("\x1b]0;QTerm Local Shell\x07qterm$ "));
    environment.insert(QStringLiteral("ENV"), QStringLiteral("/dev/null"));

    backend.setProgram(QStringLiteral("/bin/sh"));
    backend.setArguments({QStringLiteral("-i")});
    backend.setProcessEnvironment(environment);

    session.setBackend(&backend);
    terminal.setSession(&session);
    terminal.setTitle(QStringLiteral("QTerm Local PTY Demo"));
    session.open();

    engine.setInitialProperties({
        {QStringLiteral("terminal"), QVariant::fromValue(&terminal)},
        {QStringLiteral("clipboardBridge"), QVariant::fromValue(&clipboardBridge)}
    });
    engine.loadFromModule("QTermDemo", "Main");

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}

#include "main.moc"