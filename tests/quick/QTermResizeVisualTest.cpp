// Manual visual verification of the resize-regression fix using a REAL PTY.
//
// This program starts an actual zsh process via QTermLocalShellBackend, displays
// it in a QTermQuickPaintedItem, then automatically cycles between narrow and wide
// widths so zsh receives real SIGWINCH signals and redraws its own prompt.
//
// Watch the terminal window and the console output.
// Expected: prompt lines are preserved after every resize cycle.
// Buggy   : prompt lines collapse or disappear after widening.
//
// Phases per cycle (after initial shell-startup wait):
//   STARTUP  Wait 3 s for zsh to print its first prompt
//   0  Print buffer — wide baseline
//   1  Narrow window  (zsh gets SIGWINCH)
//   2  Wait 2 s, print buffer after zsh redraw at narrow width
//   3  Widen window   (zsh gets SIGWINCH)
//   4  Wait 2 s, print buffer after zsh redraw at wide width  → next cycle

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcessEnvironment>
#include <QQuickWindow>
#include <QTextStream>
#include <QTimer>

#include <QTerm/QTermLocalShellBackend.h>
#include <QTerm/QTermQuickPaintedItem.h>
#include <QTerm/QTermSession.h>
#include <QTerm/QTermTerminal.h>

using namespace Qt::StringLiterals;

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    // ── PTY backend + session + terminal ──────────────────────────────────
    QTerm::QTermLocalShellBackend backend;
    QTerm::QTermSession          session;
    QTerm::QTermTerminal         terminal;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(u"TERM"_s, u"xterm-256color"_s);
    const QString shellProgram = env.value(u"SHELL"_s, u"/bin/zsh"_s);
    const QString shellName    = QFileInfo(shellProgram).fileName();

    // Write a minimal .zshrc so zsh starts with a predictable, long prompt
    // instead of loading the user's theme (e.g. p10k / oh-my-zsh) which
    // rewrites the previous prompt line on every redraw, leaving only 1
    // visible prompt in the buffer.
    //
    // The prompt text is intentionally long (~72 chars) so it wraps at the
    // narrow test width (200 px ≈ 23 cols with Courier New 14 px).
    const QString tempZdotdir = QDir::tempPath() + u"/qterm_visual_test_zdotdir"_s;
    QDir().mkpath(tempZdotdir);
    {
        QFile zshrc(tempZdotdir + u"/.zshrc"_s);
        zshrc.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream ts(&zshrc);
        // Wipe ALL precmd/chpwd/preexec hooks that any global /etc/zshrc may
        // have installed (e.g. p10k transient prompt, command timing hooks).
        // Without this, many zsh setups erase the previous prompt line before
        // printing the new one, leaving only 1 visible prompt in the buffer.
        ts << "precmd_functions=()\n";
        ts << "preexec_functions=()\n";
        ts << "chpwd_functions=()\n";
        // Also disable zsh's built-in transient-prompt widget if present.
        ts << "(( ${+functions[p10k]} )) && p10k finalize 2>/dev/null; true\n";
        ts << "unset ZSH_THEME POWERLEVEL9K_TRANSIENT_PROMPT\n";
        // Simple, long prompt so it wraps at narrow width (~23 cols).
        ts << "PS1='%F{cyan}➜%f  %n@%m /home/dev/workspace/terminal-app/build/examples/qtquick-terminal %# '\n";
        ts << "PS2='> '\n";
        ts << "PROMPT_EOL_MARK=''\n";
    }
    env.insert(u"ZDOTDIR"_s, tempZdotdir);

    backend.setProgram(shellProgram);
    // Use -i only (not -l) to skip /etc/zprofile and /etc/zlogin which may
    // also register precmd hooks or emit cursor-movement sequences.
    backend.setArguments((shellName == u"bash"_s || shellName == u"zsh"_s)
                             ? QStringList{u"-i"_s}
                             : QStringList{u"-i"_s});
    backend.setProcessEnvironment(env);

    session.setBackend(&backend);
    terminal.setSession(&session);

    // ── Window + item ─────────────────────────────────────────────────────
    QQuickWindow window;
    window.setTitle(u"QTerm PTY resize visual test — watch for disappearing lines"_s);
    window.setColor(QColor(0x10, 0x15, 0x1c));

    const int kWideWidth  = 1120;
    const int kWideHeight = 400;
    window.resize(kWideWidth, kWideHeight);

    QTerm::QTermQuickPaintedItem item(window.contentItem());
    item.setFontFamily(u"Courier New"_s);
    item.setFontPixelSize(14);
    item.setForegroundColor(QColor(0xdc, 0xe7, 0xf3));
    item.setBackgroundColor(QColor(0x10, 0x15, 0x1c));
    item.setCursorColor(QColor(0xdc, 0xe7, 0xf3));
    item.setCursorOpacity(0.85);
    item.setTerminal(&terminal);

    // Keep item filling the window content area.
    auto syncItemSize = [&]() {
        item.setWidth(window.width());
        item.setHeight(window.height());
    };
    QObject::connect(&window, &QQuickWindow::widthChanged,  &window, syncItemSize);
    QObject::connect(&window, &QQuickWindow::heightChanged, &window, syncItemSize);
    syncItemSize();

    // ── Start shell ───────────────────────────────────────────────────────
    session.open();
    window.show();

    // ── The state machine ─────────────────────────────────────────────────
    //
    // The bug this reproduces: in a narrow window the prompt wraps across
    // several physical rows; when the window is widened again zsh redraws it
    // on SIGWINCH, and mishandling the wrap chain swallowed the earlier prompt
    // lines.
    //
    // The phases run in this order:
    //
    //   Startup          wait for zsh to start and print its first prompt
    //   Init x 4         send Enter, leaving 5 prompt lines on screen
    //   CaptureBaseline  record the buffer as the reference
    //   Narrow           shrink to kNarrowWidth; zsh redraws on SIGWINCH
    //   WaitNarrow       let the redraw settle, then dump the wrapped buffer
    //   Widen            grow back to kWideWidth; zsh redraws again
    //   WaitWide         compare against the reference; a difference is a failure
    //
    // CaptureBaseline..WaitWide then repeat kTotalCycles times.
    const int kTotalCycles      = 5;
    const int kNarrowWidth      = 50;
    const int kInitEnters       = 4;    // leaves kInitEnters + 1 prompt lines
    const int kShellStartMs     = 1500; // for zsh to start and print a prompt
    const int kEnterIntervalMs  = 80;   // between the Enter keystrokes
    const int kAfterInitMs      = 100;  // after the last Enter, before the snapshot
    const int kBeforeNarrowMs   = 200;  // pause so the run can be watched
    const int kResizeWaitMs     = 300;  // for zsh to handle SIGWINCH and redraw
    const int kAfterNarrowMs    = 50;   // after dumping, before widening again
    const int kNextCycleMs      = 50;   // between cycles
    int       initSent          = 0;

    enum Phase { Startup = -1, Init, CaptureBaseline, Narrow, WaitNarrow, Widen, WaitWide };
    int   cycle = 0;
    Phase phase = Startup;
    int   failCount = 0;
    QString baseline;      // the widened, visible-only content to compare against

    // Only the visible region, which is what the user actually sees; scrollback
    // is deliberately excluded. After a resize zsh redraws with ESC[nA + ESC[J,
    // pushing the older prompts into history and leaving just the current one
    // on screen -- that part is correct behaviour, not the bug under test.
    auto visibleText = [&]() {
        const QStringList lines = terminal.surfaceModel()->visibleLines();
        int lastNonEmpty = -1;
        for (int i = 0; i < lines.size(); ++i) {
            if (!lines.at(i).trimmed().isEmpty())
                lastNonEmpty = i;
        }
        if (lastNonEmpty < 0)
            return QString();
        QString result;
        for (int i = 0; i <= lastNonEmpty; ++i) {
            if (i > 0)
                result += u'↵';
            result += lines.at(i);
        }
        return result;
    };

    // Everything, history included, for checking the old prompts are still there.
    auto totalText = [&]() {
        return terminal.surfaceModel()->plainText().replace(u'\n', u'↵');
    };

    auto printState = [&](const QString &label) {
        qDebug().noquote()
            << label
            << u"\n    [visible in window] "_s + visibleText()
            << u"\n    [total incl. scrollback] "_s + totalText();
    };

    QTimer timer;

    auto advance = [&]() {
        switch (phase) {

        case Startup:
            // Wait for zsh to finish starting and print its first prompt.
            qDebug().noquote() << u"Shell started — waiting %1 ms for first prompt…"_s.arg(kShellStartMs);
            timer.setInterval(kShellStartMs);
            phase = Init;
            break;

        case Init:
            // One Enter per tick, building up prompt lines on screen. With
            // precmd_functions=() zsh no longer erases the previous prompt, so
            // the older lines stay in the buffer.
            if (initSent < kInitEnters) {
                qDebug().noquote()
                    << u"  → sendKey(Enter) %1/%2"_s.arg(initSent + 1).arg(kInitEnters);
                terminal.sendKey(Qt::Key_Return, u"\r"_s);
                ++initSent;
                timer.setInterval(kEnterIntervalMs);
            } else {
                // All sent: dump the buffer to confirm the 5 prompt lines.
                qDebug().noquote() << u"Init done:"_s;
                printState(u"  "_s);
                timer.setInterval(kAfterInitMs);
                phase = CaptureBaseline;
            }
            break;

        case CaptureBaseline:
            // The reference is taken from cycle 2 onwards, i.e. the steady
            // state after one narrow-widen round trip. Right after Init the
            // visible region still holds all 5 prompt lines; the first resize
            // makes zsh redraw with ESC[nA + ESC[J and it collapses to one.
            // Cycle 1 therefore only establishes that steady state.
            if (cycle > 0) {
                baseline = visibleText();
            }
            qDebug().noquote()
                << QString(u"──── Cycle %1 / %2 ────"_s).arg(cycle + 1).arg(kTotalCycles);
            printState(u"Baseline (wide)"_s);
            timer.setInterval(kBeforeNarrowMs);
            phase = Narrow;
            break;

        case Narrow:
            // Shrink in steps, the way dragging a window edge would.
            // geometryChange() notifies the controller, whose debounce timer
            // eventually issues TIOCSWINSZ; zsh then gets SIGWINCH and redraws
            // the prompt with ESC[1G, wrapping it across several rows.
            qDebug().noquote()
                << u"  → Narrowing to %1 px (zsh gets SIGWINCH)"_s.arg(kNarrowWidth);
            for (qreal w = item.width() - 50; w >= kNarrowWidth; w -= 50)
                item.setWidth(w);
            item.setWidth(kNarrowWidth);
            window.resize(kNarrowWidth, kWideHeight);
            timer.setInterval(kResizeWaitMs);
            phase = WaitNarrow;
            break;

        case WaitNarrow:
            // Dump the wrapped buffer so the wrapping can be inspected by eye.
            printState(u"After narrow redraw"_s);
            timer.setInterval(kAfterNarrowMs);
            phase = Widen;
            break;

        case Widen:
            // Grow back in steps. zsh gets SIGWINCH again and redraws, and the
            // row count should return to what it was. The bug showed up here:
            // severPredecessorWrapChain() got it wrong and ate the earlier
            // prompt lines.
            qDebug().noquote()
                << u"  → Widening to %1 px (zsh gets SIGWINCH)"_s.arg(kWideWidth);
            for (qreal w = kNarrowWidth + 50; w <= kWideWidth; w += 50)
                item.setWidth(w);
            item.setWidth(kWideWidth);
            window.resize(kWideWidth, kWideHeight);
            timer.setInterval(kResizeWaitMs);
            phase = WaitWide;
            break;

        case WaitWide: {
            // After widening, zsh redraws with ESC[nA + ESC[J: the older
            // prompts move into scrollback and only the current one is visible.
            // That is correct terminal behaviour.
            //
            // Cycle 1 only establishes the steady state and becomes the
            // reference. From cycle 2 on, the visible content must match it --
            // that is the check that reflow has not corrupted the prompt.
            printState(u"After widen redraw"_s);
            const QString afterVisible = visibleText();

            if (cycle == 0) {
                // First cycle: adopt this steady state as the reference.
                baseline = afterVisible;
                qDebug().noquote() << u"  ↳ Warmup cycle — baseline set to visible prompt"_s;
            } else {
                const bool ok = (afterVisible == baseline);
                qDebug().noquote()
                    << (ok ? u"  ✓ Visible prompt matches baseline"_s
                           : u"  ✗ MISMATCH — visible was: "_s + afterVisible
                                 + u" | baseline was: "_s + baseline);
                if (!ok)
                    ++failCount;
            }

            ++cycle;
            if (cycle >= kTotalCycles) {
                qDebug().noquote() << u"──── All cycles done ────"_s;
                qDebug().noquote()
                    << (failCount == 0
                            ? u"PASS ✓  Visible prompt intact across all resize cycles"_s
                            : u"FAIL ✗  %1 cycle(s) had visible prompt mismatch"_s.arg(failCount));
                timer.stop();
                QCoreApplication::quit();
            } else {
                timer.setInterval(kNextCycleMs);
                phase = CaptureBaseline;
            }
            break;
        }
        }
    };

    QObject::connect(&timer, &QTimer::timeout, advance);

    // Kick off the startup wait, then start ticking.
    advance();    // sets interval and phase = ShowWide
    timer.start();

    return app.exec();
}
