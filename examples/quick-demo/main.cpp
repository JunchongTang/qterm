#include <QGuiApplication>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QObject>
#include <qqml.h>

#include <QTerm/qtermlocalbackend.h>
#include <QTerm/qtermplaintextsurfaceadapter.h>
#include <QTerm/qtermquickitem.h>
#include <QTerm/qtermsession.h>
#include <QTerm/qtermsurfacemodel.h>

#include <memory>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);

    qmlRegisterType<QTermQuickItem>("QTerm", 1, 0, "TerminalView");
    qmlRegisterUncreatableType<QTermSurfaceModel>("QTerm", 1, 0, "TerminalSurfaceModel",
                                                   QStringLiteral("Create models from C++ only"));

    QTermSurfaceModel surfaceModel;
    surfaceModel.setTerminalSize(QSize(80, 24));

    QTermSession terminalSession;
    auto localBackend = std::make_unique<QTermLocalBackend>();
    localBackend->setProgram(qEnvironmentVariable("SHELL", QStringLiteral("/bin/zsh")));
    localBackend->setWorkingDirectory(QDir::currentPath());
    terminalSession.setBackend(std::move(localBackend));

    QTermPlainTextSurfaceAdapter surfaceAdapter;
    surfaceAdapter.setSession(&terminalSession);
    surfaceAdapter.setSurfaceModel(&surfaceModel);

    terminalSession.start();

    QObject::connect(&application, &QCoreApplication::aboutToQuit,
                     &terminalSession, &QTermSession::close);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("terminalSurfaceModel"), &surfaceModel);
    engine.rootContext()->setContextProperty(QStringLiteral("terminalSession"), &terminalSession);
    engine.loadFromModule("QTerm.Demo", "Main");

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return application.exec();
}