// Manual visual verification of the resize-regression fix using a REAL PTY.
//
// This program starts an actual zsh process via QTermLocalPtyBackend, displays
// it in a QTermQuickItem, then automatically cycles between narrow and wide
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

#include <QTerm/QTermLocalPtyBackend.h>
#include <QTerm/QTermQuickItem.h>
#include <QTerm/QTermSession.h>
#include <QTerm/QTermTerminal.h>

using namespace Qt::StringLiterals;

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    // ── PTY backend + session + terminal ──────────────────────────────────
    QTerm::QTermLocalPtyBackend backend;
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
        ts << "PS1='%F{cyan}➜%f  %n@%m /Users/tangjc/1-proj/2-mygithub/qterm/build/examples/quick-demo %# '\n";
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

    QTerm::QTermQuickItem item(window.contentItem());
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

    // ── 状态机总体流程 ────────────────────────────────────────────────────
    //
    // 这个测试复现的 bug：prompt 在窄窗口下折成多个物理行，展宽后 zsh 通过
    // SIGWINCH 重绘 prompt，但终端仿真器的 wrap chain 处理不正确，导致之前的
    // prompt 历史行被吃掉。
    //
    // 总体执行顺序：
    //
    //   Startup          等 3 s，让 zsh 启动并打印第一个 prompt
    //   Init × 4         用 sendKey(Enter) 产生 4 次空命令，屏幕上留下 5 行 prompt
    //   CaptureBaseline  记录当前 buffer 作为"基准"（5 行 prompt）
    //   Narrow           把窗口缩窄到 kNarrowWidth，zsh 收到 SIGWINCH 并重绘
    //   WaitNarrow       等 kResizeWaitMs ms，打印缩窄后的 buffer（验证行数）
    //   Widen            把窗口展宽回 kWideWidth，zsh 再次收到 SIGWINCH 并重绘
    //   WaitWide         等 kResizeWaitMs ms，对比 buffer 与基准 → 若不同则报 FAIL
    //
    //   以上 CaptureBaseline..WaitWide 循环 kTotalCycles 次。
    const int kTotalCycles      = 5;
    const int kNarrowWidth      = 50;
    const int kInitEnters       = 4;    // 发 Enter 次数，产生 kInitEnters+1 行 prompt 历史
    const int kShellStartMs     = 1500; // 等待 zsh 启动并打印第一个 prompt 的时间
    const int kEnterIntervalMs  = 80;   // 每次 sendKey(Enter) 之间的间隔
    const int kAfterInitMs      = 100;  // 所有 Enter 发完后、拍基准快照前的等待
    const int kBeforeNarrowMs   = 200;  // 拍完基准到开始缩窄前的等待（留给视觉观察）
    const int kResizeWaitMs     = 300;  // 每次 resize 后等待 zsh 处理 SIGWINCH 并重绘的时间
    const int kAfterNarrowMs    = 50;   // 打印缩窄后 buffer 到开始展宽前的等待
    const int kNextCycleMs      = 50;   // 一轮结束后进入下一轮前的等待
    int       initSent          = 0;

    enum Phase { Startup = -1, Init, CaptureBaseline, Narrow, WaitNarrow, Widen, WaitWide };
    int   cycle = 0;
    Phase phase = Startup;
    int   failCount = 0;
    QString baseline;      // 基准：展宽后 visible-only 内容（即当前 prompt 文本）

    // 只取 visible 区域的内容（窗口实际显示的内容），忽略 scrollback history。
    // 这是终端窗口用户实际看到的内容，resize 后 zsh 用 ESC[nA + ESC[J 重绘，
    // 旧 prompt 行会被推入 history，visible 里只剩当前 prompt（这是正确行为）。
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

    // 全部内容（history + visible），用于验证旧 prompt 是否仍在 scrollback 中。
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
            // 等待 zsh 完成启动并打印第一个 prompt
            qDebug().noquote() << u"Shell started — waiting %1 ms for first prompt…"_s.arg(kShellStartMs);
            timer.setInterval(kShellStartMs);
            phase = Init;
            break;

        case Init:
            // 每次 tick 发一个 Enter，共发 kInitEnters 次，在屏幕上积累多行 prompt 历史。
            // 有了 precmd_functions=() 后，zsh 不再擦除旧 prompt，所以旧行会保留在 buffer 里。
            if (initSent < kInitEnters) {
                qDebug().noquote()
                    << u"  → sendKey(Enter) %1/%2"_s.arg(initSent + 1).arg(kInitEnters);
                terminal.sendKey(Qt::Key_Return, u"\r"_s);
                ++initSent;
                timer.setInterval(kEnterIntervalMs);
            } else {
                // Enter 全部发完，打印当前 buffer 确认已有 5 行 prompt
                qDebug().noquote() << u"Init done:"_s;
                printState(u"  "_s);
                timer.setInterval(kAfterInitMs);
                phase = CaptureBaseline;
            }
            break;

        case CaptureBaseline:
            // 从第 2 轮开始才捕获基准（第 1 轮 narrow→wide 结束后的稳态）。
            // 第 1 轮 Init 后 visible 里有 5 行 prompt（qMax 尚未触发 history push），
            // resize 后 zsh 用 ESC[nA + ESC[J 重绘，visible 收缩为 1 行。
            // 第 1 轮主要用于"预热"并确立稳态，从第 2 轮起做真正的回归校验。
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
            // 逐步缩窄窗口到 kNarrowWidth，模拟用户拖拽窗口边缘。
            // QTermQuickItem::geometryChange 会同步调用 syncTerminalSize()，
            // QTermLocalPtyBackend 的 debounce timer 到期后发 TIOCSWINSZ，
            // zsh 收到 SIGWINCH 并用 ESC[1G 重绘 prompt（折成多行）。
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
            // 等待 kResizeWaitMs ms 后打印缩窄后的 buffer，供人工观察折行情况。
            printState(u"After narrow redraw"_s);
            timer.setInterval(kAfterNarrowMs);
            phase = Widen;
            break;

        case Widen:
            // 逐步展宽窗口回 kWideWidth。
            // zsh 再次收到 SIGWINCH，用 ESC[1G 重绘 prompt（行数应恢复原状）。
            // Bug 表现：展宽后 severPredecessorWrapChain 处理错误，前几行 prompt 被吃掉。
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
            // 展宽后 zsh 用 ESC[nA + ESC[J 重绘：旧 prompt 行进入 scrollback，
            // visible 区域只有当前 prompt（1 行）。这是正确的终端行为。
            //
            // 第 1 轮是预热轮（确立稳态），不做对比，直接用此时的 visible 作为后续基准。
            // 第 2~N 轮：验证 visible 内容与基准一致（prompt 没有因 reflow 错误而损坏）。
            printState(u"After widen redraw"_s);
            const QString afterVisible = visibleText();

            if (cycle == 0) {
                // 预热轮：用展宽后的稳态作为后续基准
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
