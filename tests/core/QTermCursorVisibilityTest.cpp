#include <QtTest>

#include <QRandomGenerator>

#include <QTerm/QTermSurfaceModel.h>
#include <QTerm/QTermTerminal.h>

using namespace Qt::StringLiterals;

/*
    Guards the cursor staying visible.

    Written for a field report against a third-party host: the cursor vanishes
    after a day or two of use and never comes back, while typing still works.
    That combination is specific -- it means the terminal is healthy and only
    the cursor's visibility gate is stuck -- and it is not reproducible on
    demand, so these tests pin the invariant rather than the symptom.

    The user-facing guarantee is narrow enough to assert directly: when the
    viewport is at the bottom and no program has hidden the cursor, the cursor
    must be visible. Everything else here is a way of reaching that state
    through paths a long-running session takes -- scrollback eviction, resizes,
    alternate-screen round trips, resets.

    Deliberately not asserted: the expression syncSurfaceCursor() uses. Copying
    it here would only restate the implementation and would pass whatever it
    did.
*/
class QTermCursorVisibilityTest : public QObject
{
    Q_OBJECT

private slots:
    void visibleOnAFreshTerminal();
    void hideAndShowRoundTrips();
    void showWithNoOtherOutputReachesTheSurface();
    void survivesScrollbackEviction();
    void survivesResizeChurn();
    void hiddenCursorReportsHidden();
    void scrolledAwayCursorIsNotDrawn();
    void returningToBottomRestoresTheCursor();
    void alternateScreenRoundTripKeepsCursor();
    void resetRecoversAHiddenCursor();
    void randomisedStateTransitions_data();
    void randomisedStateTransitions();
};

namespace {

// The guarantee the bug report is about: parked at the bottom of the buffer
// with nothing hiding the cursor, it has to be drawable.
void verifyCursorUsable(QTerm::QTermTerminal &terminal, const char *context)
{
    QTerm::QTermSurfaceModel *surface = terminal.surfaceModel();
    QVERIFY2(surface->cursorVisible(),
             qPrintable(u"cursor is not visible at the bottom of the buffer (%1)"_s
                            .arg(QString::fromLatin1(context))));
    QVERIFY2(surface->cursorRow() >= 0 && surface->cursorRow() < terminal.rows(),
             qPrintable(u"cursor row %1 is outside the %2-row viewport (%3)"_s
                            .arg(surface->cursorRow())
                            .arg(terminal.rows())
                            .arg(QString::fromLatin1(context))));
    QVERIFY2(surface->cursorColumn() >= 0 && surface->cursorColumn() <= terminal.columns(),
             qPrintable(u"cursor column %1 is outside the %2-column viewport (%3)"_s
                            .arg(surface->cursorColumn())
                            .arg(terminal.columns())
                            .arg(QString::fromLatin1(context))));
}

} // namespace

void QTermCursorVisibilityTest::visibleOnAFreshTerminal()
{
    QTerm::QTermTerminal terminal;
    terminal.setTerminalSize(80, 24);
    verifyCursorUsable(terminal, "fresh terminal");
}

void QTermCursorVisibilityTest::hideAndShowRoundTrips()
{
    QTerm::QTermTerminal terminal;
    terminal.setTerminalSize(80, 24);

    terminal.feedText(u"\x1b[?25l"_s);
    QVERIFY(!terminal.surfaceModel()->cursorVisible());
    terminal.feedText(u"\x1b[?25h"_s);
    verifyCursorUsable(terminal, "after a hide/show round trip");
}

void QTermCursorVisibilityTest::showWithNoOtherOutputReachesTheSurface()
{
    QTerm::QTermTerminal terminal;
    terminal.setTerminalSize(80, 24);
    terminal.feedText(u"prompt$ "_s);
    terminal.feedText(u"\x1b[?25l"_s);
    QVERIFY(!terminal.surfaceModel()->cursorVisible());

    // DECTCEM lives in the mode state, not in the buffer or the cursor
    // position. A show that arrives on its own -- no text, no movement -- must
    // still reach the surface model, or the cursor stays gone until the next
    // unrelated output happens to refresh it.
    terminal.feedText(u"\x1b[?25h"_s);
    verifyCursorUsable(terminal, "show with no accompanying output");
}

void QTermCursorVisibilityTest::survivesScrollbackEviction()
{
    QTerm::QTermTerminal terminal;
    terminal.setTerminalSize(80, 24);
    terminal.setMaximumScrollbackLines(100);

    // Long sessions spend most of their life past this point, with every new
    // line evicting an old one and shifting every projection row.
    for (int line = 0; line < 3000; ++line)
        terminal.feedText(u"line %1\r\n"_s.arg(line));

    verifyCursorUsable(terminal, "after 3000 lines through a 100-line scrollback");
}

void QTermCursorVisibilityTest::survivesResizeChurn()
{
    QTerm::QTermTerminal terminal;
    terminal.setTerminalSize(80, 24);
    terminal.setMaximumScrollbackLines(200);

    for (int line = 0; line < 1500; ++line) {
        terminal.feedText(u"resize line %1\r\n"_s.arg(line));
        if (line % 7 == 0)
            terminal.setTerminalSize(30 + (line % 90), 8 + (line % 24));
    }
    verifyCursorUsable(terminal, "after interleaved output and resizes");
}

void QTermCursorVisibilityTest::hiddenCursorReportsHidden()
{
    QTerm::QTermTerminal terminal;
    terminal.setTerminalSize(80, 24);
    terminal.feedText(u"\x1b[?25l"_s);
    QVERIFY2(!terminal.surfaceModel()->cursorVisible(),
             "a program asked for the cursor to be hidden and it is still reported visible");
}

void QTermCursorVisibilityTest::scrolledAwayCursorIsNotDrawn()
{
    QTerm::QTermTerminal terminal;
    terminal.setTerminalSize(80, 10);
    for (int line = 0; line < 200; ++line)
        terminal.feedText(u"line %1\r\n"_s.arg(line));

    terminal.scrollByLines(100);
    QVERIFY2(!terminal.surfaceModel()->cursorVisible(),
             "the cursor is off screen but still reported as visible");
}

void QTermCursorVisibilityTest::returningToBottomRestoresTheCursor()
{
    QTerm::QTermTerminal terminal;
    terminal.setTerminalSize(80, 10);
    for (int line = 0; line < 200; ++line)
        terminal.feedText(u"line %1\r\n"_s.arg(line));

    terminal.scrollByLines(100);
    terminal.scrollToBottom();
    verifyCursorUsable(terminal, "after scrolling away and back");
}

void QTermCursorVisibilityTest::alternateScreenRoundTripKeepsCursor()
{
    QTerm::QTermTerminal terminal;
    terminal.setTerminalSize(80, 24);
    terminal.feedText(u"shell prompt\r\n"_s);

    // What a full-screen program does on the way in and on the way out.
    terminal.feedText(u"\x1b[?1049h\x1b[?25l"_s);
    QVERIFY(!terminal.surfaceModel()->cursorVisible());
    terminal.feedText(u"\x1b[?25h\x1b[?1049l"_s);
    verifyCursorUsable(terminal, "after an alternate-screen round trip");
}

void QTermCursorVisibilityTest::resetRecoversAHiddenCursor()
{
    QTerm::QTermTerminal terminal;
    terminal.setTerminalSize(80, 24);

    // A program killed mid-run leaves the cursor hidden; `reset` is how a user
    // gets out of it, so that path must actually restore the cursor.
    terminal.feedText(u"\x1b[?25l"_s);
    QVERIFY(!terminal.surfaceModel()->cursorVisible());
    terminal.clear();
    verifyCursorUsable(terminal, "after clear()");
}

void QTermCursorVisibilityTest::randomisedStateTransitions_data()
{
    QTest::addColumn<quint32>("seed");
    // Fixed seeds rather than a random one: a failure has to be reproducible,
    // and a test that only fails on some runs teaches nobody anything.
    for (quint32 seed : {1u, 7u, 42u, 1337u, 90210u})
        QTest::newRow(qPrintable(u"seed %1"_s.arg(seed))) << seed;
}

/*
    A bug that takes a day or two to show up is rarely a single wrong line; it
    is usually an ordering the linear tests above never produce. This walks the
    same state space in a random order and checks the invariant after every
    step, which is the only kind of test with a real chance of finding it.
*/
void QTermCursorVisibilityTest::randomisedStateTransitions()
{
    QFETCH(quint32, seed);
    QRandomGenerator random(seed);

    QTerm::QTermTerminal terminal;
    terminal.setTerminalSize(80, 24);
    terminal.setMaximumScrollbackLines(150);

    bool hiddenByProgram = false;
    QStringList history;

    for (int step = 0; step < 4000; ++step) {
        const int operation = random.bounded(10);
        switch (operation) {
        case 0:
            terminal.feedText(u"\x1b[?25l"_s);
            hiddenByProgram = true;
            history << u"hide"_s;
            break;
        case 1:
            terminal.feedText(u"\x1b[?25h"_s);
            hiddenByProgram = false;
            history << u"show"_s;
            break;
        case 2:
        case 3:
            terminal.feedText(u"step %1\r\n"_s.arg(step));
            history << u"newline"_s;
            break;
        case 4:
            terminal.feedText(u"text"_s);
            history << u"text"_s;
            break;
        case 5:
            terminal.setTerminalSize(20 + random.bounded(100), 5 + random.bounded(40));
            history << u"resize"_s;
            break;
        case 6:
            terminal.scrollByLines(1 + random.bounded(50));
            history << u"scroll up"_s;
            break;
        case 7:
            terminal.scrollToBottom();
            history << u"scroll to bottom"_s;
            break;
        case 8:
            terminal.feedText(random.bounded(2) ? u"\x1b[?1049h"_s : u"\x1b[?1049l"_s);
            history << u"alt screen toggle"_s;
            break;
        case 9:
            terminal.feedText(u"\x1b[%1 q"_s.arg(random.bounded(7)));
            history << u"cursor shape"_s;
            break;
        }

        // Recomputed every step rather than tracked: output pins the viewport
        // back to the bottom and a resize can move it too, so inferring it from
        // the operation alone goes wrong exactly where it matters.
        const bool scrolledAway = terminal.scrollOffset() != 0;

        QTerm::QTermSurfaceModel *surface = terminal.surfaceModel();

        // Structural bound: whatever the state, the cursor must address a cell
        // inside the viewport. A row outside it is how the cursor silently
        // stops being drawn.
        QVERIFY2(surface->cursorRow() >= 0 && surface->cursorRow() < terminal.rows(),
                 qPrintable(u"seed %1 step %2 (%3): cursor row %4 outside %5 rows. Trail: %6"_s
                                .arg(seed).arg(step).arg(history.last())
                                .arg(surface->cursorRow()).arg(terminal.rows())
                                .arg(history.mid(qMax(0, history.size() - 12)).join(u", "_s))));

        // The reported bug: nothing is hiding it and it is not scrolled away,
        // so it must be visible.
        if (!hiddenByProgram && !scrolledAway) {
            QVERIFY2(surface->cursorVisible(),
                     qPrintable(u"seed %1 step %2 (%3): cursor invisible with nothing hiding it. "
                                u"Trail: %4"_s
                                    .arg(seed).arg(step).arg(history.last())
                                    .arg(history.mid(qMax(0, history.size() - 12)).join(u", "_s))));
        }
    }
}

QTEST_MAIN(QTermCursorVisibilityTest)
#include "QTermCursorVisibilityTest.moc"
