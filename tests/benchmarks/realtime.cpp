#include <QGuiApplication>
#include <QElapsedTimer>
#include <QDebug>
#include <QQuickWindow>
#include <QFileInfo>
#include <QTimer>
#include <QTerm/QTermSession.h>
#include <QTerm/QTermSessionBackend.h>
#include <QTerm/QTermLocalShellBackend.h>
#include <QTerm/QTermTerminal.h>
#include <QTerm/QTermSurfaceModel.h>
#include <QTerm/QTermQuickItem.h>
#include <unistd.h>

// Measures what the user actually waits for: the moment the last line of the
// payload is on screen. `time cat` only measures how fast the child exited,
// which says nothing once the terminal buffers output on a reader thread.
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const QString load = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                  : QStringLiteral("/tmp/qterm-bench/cjk.txt");
    const QString marker = argc > 2 ? QString::fromLocal8Bit(argv[2])
                                    : QStringLiteral("399999");

    QTerm::QTermTerminal terminal;
    QTerm::QTermSession session;
    QTerm::QTermLocalShellBackend backend;
    backend.setProgram(QStringLiteral("/bin/sh"));
    backend.setArguments({QStringLiteral("-c"), QStringLiteral("cat %1").arg(load)});

    QQuickWindow window;
    const int winW = qEnvironmentVariableIntValue("QTERM_W") ?: 900;
    const int winH = qEnvironmentVariableIntValue("QTERM_H") ?: 600;
    window.resize(winW, winH);
    auto *item = new QTerm::QTermQuickItem(window.contentItem());
    item->setWidth(winW); item->setHeight(winH);
    item->setTerminal(&terminal);
    item->setFontFamily(QStringLiteral("Menlo"));
    item->setFontPixelSize(16);
    window.show();

    int frames = 0;
    QObject::connect(&window, &QQuickWindow::afterRendering, &app, [&] { ++frames; });

    session.setBackend(&backend);
    terminal.setSession(&session);

    auto *t = new QElapsedTimer;
    auto *childExit = new double(0);
    QObject::connect(&session, &QTerm::QTermSession::stateChanged, &app, [&, t, childExit] {
        if (session.state() == QTerm::QTermSessionBackend::Closed
            || session.state() == QTerm::QTermSessionBackend::Error) {
            *childExit = t->elapsed() / 1000.0;
        }
    });

    QTimer::singleShot(700, &app, [&, t, childExit] {
        frames = 0;
        t->start();
        session.open();
        // Poll for the payload's last line to appear on screen.
        auto *poll = new QTimer(&app);
        poll->setInterval(5);
        QObject::connect(poll, &QTimer::timeout, &app, [&, t, childExit] {
            for (const QString &line : terminal.surfaceModel()->visibleLines()) {
                if (line.contains(marker)) {
                    qInfo().noquote()
                        << QStringLiteral("  last line on screen %1 s   child exit %2 s   frames %3   grid %4x%5")
                               .arg(t->elapsed() / 1000.0, 0, 'f', 3)
                               .arg(*childExit, 0, 'f', 3)
                               .arg(frames)
                               .arg(terminal.columns()).arg(terminal.rows());
                    // One machine-readable line per run, so a sweep can be
                    // collected without parsing the prose above.
                    qInfo().noquote()
                        << QStringLiteral("RESULT\ttool=realtime\tpayload=%1\tseconds=%2"
                                          "\tchild_exit=%3\tframes=%4\tgrid=%5x%6")
                               .arg(QFileInfo(load).fileName())
                               .arg(t->elapsed() / 1000.0, 0, 'f', 3)
                               .arg(*childExit, 0, 'f', 3)
                               .arg(frames)
                               .arg(terminal.columns()).arg(terminal.rows());
                    ::_exit(0);
                }
            }
        });
        poll->start();
    });
    QTimer::singleShot(60000, &app, [] { qInfo() << "  TIMEOUT"; ::_exit(1); });
    return app.exec();
}
