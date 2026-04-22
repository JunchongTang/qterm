#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "quick/qtermquickitem.h"
#include "session/qtermsession.h"

int main(int argc, char *argv[]) {
  QGuiApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("QTermDemo"));
  app.setApplicationVersion(QStringLiteral("0.1"));

  // Register QML types
  qmlRegisterType<QTermQuickItem>("QTerm", 1, 0, "TerminalItem");

  QQmlApplicationEngine engine;

  // Create the session and expose it to QML
  QTermSession session;
  engine.rootContext()->setContextProperty(QStringLiteral("termSession"),
                                           &session);

  const QUrl url(QStringLiteral("qrc:/qt/qml/QuickDemo/qml/Main.qml"));
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreated, &app,
      [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
          QCoreApplication::exit(-1);
      },
      Qt::QueuedConnection);

  engine.load(url);

  // Start the terminal session after the UI is up
  session.start();

  return app.exec();
}
