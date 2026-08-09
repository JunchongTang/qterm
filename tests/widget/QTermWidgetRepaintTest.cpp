#include <QtTest>

#include <QElapsedTimer>
#include <QPaintEvent>

#include <QTerm/QTermSurfaceModel.h>
#include <QTerm/QTermTerminal.h>
#include <QTerm/QTermWidget.h>

/*
    Guards the repaint scheduling in QTermWidget.

    These assert invariants rather than durations. The regression they exist for
    turned a 16 MB payload into 58 seconds by repainting 16680 times where ~44
    were needed; that is a structural difference of two orders of magnitude, so
    a counter catches it with margin to spare and without depending on how fast
    the machine is. A wall-clock threshold would have had to be tuned per
    machine and per build type, and would still have measured the wrong thing.

    Output is pushed with feedText() rather than through a PTY: the point is to
    reproduce the event-loop pattern that caused the regression -- a content
    change, then an event loop pass, repeated -- which is exactly what a socket
    notifier delivering PTY reads produces.
*/
class QTermWidgetRepaintTest : public QObject
{
    Q_OBJECT

private slots:
    void firstChangeRepaintsImmediately();
    void burstCoalescesIntoFewRepaints();
    void lastRepaintReflectsFinalBuffer();
};

namespace {

class CountingWidget : public QTerm::QTermWidget
{
public:
    int paints = 0;
    // Which content revision the most recent paint drew.
    //
    // Comparing two grab() results would prove nothing: QWidget::grab()
    // re-renders the widget rather than capturing the composited window, so
    // both images show the current state whether or not a frame was dropped.
    // Snapshotting the buffer here does not work either -- querying the surface
    // model from inside paintEvent re-enters it while it is being read for
    // painting, which crashes under sustained output.
    int contentRevision = 0;
    int paintedRevision = 0;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QTerm::QTermWidget::paintEvent(event);
        ++paints;
        paintedRevision = contentRevision;
    }
};

// Runs the event loop for `ms`, so timer-driven repaints get a chance to fire.
void spin(int ms)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
}

void feedBurst(QTerm::QTermTerminal &terminal, int chunks)
{
    for (int i = 0; i < chunks; ++i) {
        terminal.feedText(QStringLiteral("burst line %1\r\n").arg(i));
        // One event loop pass per chunk: without coalescing this is where the
        // widget used to service a full repaint.
        QCoreApplication::processEvents();
    }
}

} // namespace

void QTermWidgetRepaintTest::firstChangeRepaintsImmediately()
{
    QTerm::QTermTerminal terminal;
    CountingWidget widget;
    widget.setTerminal(&terminal);
    widget.resize(800, 400);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    spin(60);              // settle, and let the frame deadline lapse
    widget.paints = 0;

    terminal.feedText(QStringLiteral("hello\r\n"));
    QCoreApplication::processEvents();

    // Coalescing must not delay an isolated change, or every keystroke would
    // wait for the next frame deadline.
    QCOMPARE(widget.paints, 1);
}

void QTermWidgetRepaintTest::burstCoalescesIntoFewRepaints()
{
    QTerm::QTermTerminal terminal;
    CountingWidget widget;
    widget.setTerminal(&terminal);
    widget.resize(800, 400);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    spin(60);
    widget.paints = 0;

    constexpr int chunks = 500;
    feedBurst(terminal, chunks);
    spin(120);

    // A burst this tight spans a handful of frame intervals at most. The bound
    // is deliberately far above that and far below `chunks`: the regression
    // produced one repaint per chunk.
    QVERIFY2(widget.paints < chunks / 10,
             qPrintable(QStringLiteral("%1 repaints for %2 chunks -- repaints are "
                                       "tracking input rather than frames")
                        .arg(widget.paints).arg(chunks)));
}

void QTermWidgetRepaintTest::lastRepaintReflectsFinalBuffer()
{
    QTerm::QTermTerminal terminal;
    CountingWidget widget;
    widget.setTerminal(&terminal);
    widget.resize(800, 400);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    spin(60);

    QObject::connect(terminal.surfaceModel(),
                     &QTerm::QTermSurfaceModel::visibleLineRunsChanged,
                     &widget, [&widget] { ++widget.contentRevision; });

    feedBurst(terminal, 200);
    // Deliberately ends mid-window: the last change lands while a frame
    // deadline is pending, which is when a trailing flush would be missed.
    terminal.feedText(QStringLiteral("FINAL-LINE-MARKER\r\n"));
    spin(200);

    QVERIFY(widget.contentRevision > 0);
    QCOMPARE(widget.paintedRevision, widget.contentRevision);
}

QTEST_MAIN(QTermWidgetRepaintTest)
#include "QTermWidgetRepaintTest.moc"
