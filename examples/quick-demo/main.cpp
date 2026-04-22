#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QVariant>
#include <qqml.h>

#include <QTerm/QTermTerminal.h>

#include <QTerm/QTermSurfaceModel.h>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    qmlRegisterUncreatableType<QTerm::QTermTerminal>(
        "QTerm", 1, 0, "QTermTerminal", "QTermTerminal is provided by the application.");
    qmlRegisterUncreatableType<QTerm::QTermSurfaceModel>(
        "QTerm", 1, 0, "QTermSurfaceModel", "QTermSurfaceModel is provided by QTermTerminal.");

    QTerm::QTermTerminal terminal;
    terminal.setTitle(QStringLiteral("QTerm Bootstrap"));
    terminal.feedText(QStringLiteral(
        "QTerm bootstrap demo\r\n"
        "\r\n"
        "Design and roadmap are in place.\r\n"
        "Next milestone: headless core buffer types, parser MVP, and resize invariants.\r\n"
    ));

    engine.setInitialProperties({
        {QStringLiteral("terminal"), QVariant::fromValue(&terminal)}
    });
    engine.loadFromModule("QTermDemo", "Main");

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}