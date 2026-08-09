// Widget counterpart of realtime.cpp: same yardstick (the moment the payload's
// last line is on screen), so the two renderers can be compared directly.
// Additionally counts paint events and the time spent inside them, which is
// what separates "the paint code is slow" from "we are painting too often".
#include <QApplication>
#include <QElapsedTimer>
#include <QDebug>
#include <QFileInfo>
#include <QPaintEvent>
#include <QTimer>
#include <QTerm/QTermLocalShellBackend.h>
#include <QTerm/QTermSession.h>
#include <QTerm/QTermSessionBackend.h>
#include <QTerm/QTermSurfaceModel.h>
#include <QTerm/QTermTerminal.h>
#include <QTerm/QTermWidget.h>
#include <unistd.h>

namespace {

class CountingWidget : public QTerm::QTermWidget
{
public:
    using QTermWidget::QTermWidget;

    int paints = 0;
    qint64 paintNanos = 0;
    qint64 paintedPixels = 0;
    // Which content revision the most recent paint drew. Comparing two grab()
    // results would prove nothing -- QWidget::grab() re-renders rather than
    // capturing the composited window, so both images show the current state
    // whether or not a frame was dropped. Snapshotting the buffer here does not
    // work either: querying the surface model from inside paintEvent re-enters
    // it while it is being read for painting, which crashes under load.
    int contentRevision = 0;
    int paintedRevision = 0;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QElapsedTimer timer;
        timer.start();
        QTerm::QTermWidget::paintEvent(event);
        paintNanos += timer.nsecsElapsed();
        paintedPixels += qint64(event->rect().width()) * event->rect().height();
        ++paints;
        paintedRevision = contentRevision;
    }
};

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QString load = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                  : QStringLiteral("/tmp/qterm-bench/color1m.txt");
    const QString marker = argc > 2 ? QString::fromLocal8Bit(argv[2])
                                    : QStringLiteral("segment-199999");

    QTerm::QTermTerminal terminal;
    QTerm::QTermSession session;
    QTerm::QTermLocalShellBackend backend;
    backend.setProgram(QStringLiteral("/bin/sh"));
    backend.setArguments({QStringLiteral("-c"), QStringLiteral("cat %1").arg(load)});

    const int winW = qEnvironmentVariableIntValue("QTERM_W") ?: 1252;
    const int winH = qEnvironmentVariableIntValue("QTERM_H") ?: 760;

    CountingWidget view;
    view.setTerminal(&terminal);
    view.setFontFamily(QStringLiteral("Menlo"));
    view.setFontPixelSize(16);
    view.resize(winW, winH);
    view.show();

    session.setBackend(&backend);
    terminal.setSession(&session);

    // Counts content changes so the paint side can record which revision it
    // last drew; cheap enough to run on every change, unlike reading the model.
    QObject::connect(terminal.surfaceModel(), &QTerm::QTermSurfaceModel::visibleLineRunsChanged,
                     &app, [&view] { ++view.contentRevision; });

    auto *t = new QElapsedTimer;
    auto *childExit = new double(0);
    QObject::connect(&session, &QTerm::QTermSession::stateChanged, &app, [&, t, childExit] {
        if (session.state() == QTerm::QTermSessionBackend::Closed
            || session.state() == QTerm::QTermSessionBackend::Error) {
            *childExit = t->elapsed() / 1000.0;
        }
    });

    QTimer::singleShot(700, &app, [&, t, childExit] {
        view.paints = 0;
        view.paintNanos = 0;
        view.paintedPixels = 0;
        t->start();
        session.open();
        auto *poll = new QTimer(&app);
        poll->setInterval(5);
        QObject::connect(poll, &QTimer::timeout, &app, [&, t, childExit] {
            for (const QString &line : terminal.surfaceModel()->visibleLines()) {
                if (line.contains(marker)) {
                    const double total = t->elapsed() / 1000.0;
                    poll->stop();
                    // The marker only proves the *buffer* holds the last line.
                    // Coalescing can only go wrong by leaving the screen behind
                    // it, so settle past one frame deadline and then check that
                    // the most recent paint reflects the final buffer.
                    QElapsedTimer settle;
                    settle.start();
                    while (settle.elapsed() < 250)
                        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
                    const char *integrity =
                        view.paintedRevision == view.contentRevision
                            ? "last paint drew the final content"
                            : "MISMATCH: screen left behind the buffer";
                    const double inPaint = view.paintNanos / 1e9;
                    qInfo().noquote()
                        << QStringLiteral("  last line on screen %1 s   child exit %2 s")
                               .arg(total, 0, 'f', 3).arg(*childExit, 0, 'f', 3)
                        << QStringLiteral("\n  paints %1   in paint %2 s (%3%)   avg %4 ms   avg area %5 rows")
                               .arg(view.paints)
                               .arg(inPaint, 0, 'f', 3)
                               .arg(100.0 * inPaint / total, 0, 'f', 1)
                               .arg(view.paints ? inPaint * 1000.0 / view.paints : 0.0, 0, 'f', 2)
                               .arg(view.paints ? view.paintedPixels / double(view.paints)
                                                  / qMax(1.0, double(view.width()))
                                                  / qMax(1.0, view.cellHeight()) : 0.0, 0, 'f', 1)
                        << QStringLiteral("\n  grid %1x%2   integrity: %3")
                               .arg(terminal.columns()).arg(terminal.rows())
                               .arg(QString::fromLatin1(integrity));
                    // One machine-readable line per run, so a sweep can be
                    // collected without parsing the prose above.
                    qInfo().noquote()
                        << QStringLiteral("RESULT\ttool=realtime_widget\tpayload=%1"
                                          "\tseconds=%2\tchild_exit=%3\trepaints=%4"
                                          "\tpaint_seconds=%5\tgrid=%6x%7")
                               .arg(QFileInfo(load).fileName())
                               .arg(total, 0, 'f', 3)
                               .arg(*childExit, 0, 'f', 3)
                               .arg(view.paints)
                               .arg(inPaint, 0, 'f', 3)
                               .arg(terminal.columns()).arg(terminal.rows());
                    ::_exit(0);
                }
            }
        });
        poll->start();
    });
    QTimer::singleShot(180000, &app, [] { qInfo() << "  TIMEOUT"; ::_exit(1); });
    return app.exec();
}
