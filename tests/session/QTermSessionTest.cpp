#include <QtTest>

#include <QTerm/QTermSession.h>
#include <QTerm/QTermSessionBackend.h>
#include <QTerm/QTermTerminal.h>

using namespace Qt::StringLiterals;

namespace QTerm {

class FakeSessionBackend final : public QTermSessionBackend
{
    Q_OBJECT

public:
    explicit FakeSessionBackend(QObject *parent = nullptr)
        : QTermSessionBackend(parent)
    {
    }

    void open() override
    {
        ++openCount;
        setState(Open);
    }

    void close() override
    {
        ++closeCount;
        setState(Closed);
    }

    void writeData(const QByteArray &data) override
    {
        writes.append(data);
    }

    void resize(int columns, int rows) override
    {
        lastResizeColumns = columns;
        lastResizeRows = rows;
    }

    void pushData(const QByteArray &data)
    {
        emitDataReceived(data);
    }

    int openCount = 0;
    int closeCount = 0;
    int lastResizeColumns = -1;
    int lastResizeRows = -1;
    QList<QByteArray> writes;
};

class QTermSessionTest : public QObject
{
    Q_OBJECT

private slots:
    void forwardsBackendOperations();
    void terminalRoutesInboundAndOutboundData();
    void terminalDecodesSplitUtf8AcrossChunks();
    void terminalUpdatesTitleFromOscSequence();
    void terminalEmitsBellFromSessionData();
    void terminalProjectsCursorStateToSurfaceModel();
    void terminalProjectsCursorVisibilityMode();
    void terminalHidesCursorWhenScrolledFromBottom();
    void terminalProjectsVisibleLinesToSurfaceModel();
    void terminalKeepsVisibleLineCountOnResize();
    void terminalProjectsVisibleLineRunsToSurfaceModel();
    void terminalProjectsExtendedColorsToSurfaceModel();
    void terminalProjectsDecoratedRunsToSurfaceModel();
    void terminalControlsSelectionFromTerminalApi();
    void surfaceModelDelegatesSelectionControlToTerminal();
    void terminalSelectsWordByViewportColumns();
    void terminalSelectsWordAcrossSoftWrappedRows();
    void terminalSelectsLogicalLineAcrossSoftWrappedRows();
    void terminalSelectsShellStyleTokenByViewportColumns();
    void terminalSelectsUrlAndAssignmentTokensByViewportColumns();
    void terminalExcludesTrailingPunctuationFromUrlSelection();
    void terminalExcludesSentenceTrailingDotFromUrlSelection();
    void terminalExcludesTrailingPunctuationFromGenericUrlSchemes();
    void terminalExtractsSelectedTextFromSurfaceModel();
    void terminalExtractsWideCharacterSelectionByColumns();
    void terminalPreservesSelectionAcrossReflowResize();
    void terminalKeepsSelectedTextWhenResizePushesSelectionIntoHistory();
    void surfaceModelProjectsSelectionVisibility();
    void terminalSelectsFromScrolledViewport();
    void terminalPreservesAllPromptLinesAcrossRepeatedWidthOscillation();
    void terminalPreservesAllPromptLinesWhenResizeDebouncedBeforeRedraw();
    void terminalPreservesZshStyledPromptAcrossWidthOscillation();
    void terminalPreservesPromptLinesWhenShellUsesAbsoluteColumnMove();
};

void QTermSessionTest::forwardsBackendOperations()
{
    QTermSession session;
    FakeSessionBackend backend;
    QSignalSpy stateSpy(&session, &QTermSession::stateChanged);
    QSignalSpy dataSpy(&session, &QTermSession::dataReceived);

    session.setBackend(&backend);
    session.open();
    session.writeData("abc");
    session.resize(120, 40);
    backend.pushData("hello");
    session.close();

    QCOMPARE(backend.openCount, 1);
    QCOMPARE(backend.closeCount, 1);
    QCOMPARE(backend.writes, QList<QByteArray>{QByteArray("abc")});
    QCOMPARE(backend.lastResizeColumns, 120);
    QCOMPARE(backend.lastResizeRows, 40);
    QCOMPARE(dataSpy.size(), 1);
    QCOMPARE(dataSpy.at(0).at(0).toByteArray(), QByteArray("hello"));
    QVERIFY(stateSpy.size() >= 2);
    QCOMPARE(session.state(), QTermSessionBackend::Closed);
}

void QTermSessionTest::terminalRoutesInboundAndOutboundData()
{
    QTermTerminal terminal;
    QTermSession session;
    FakeSessionBackend backend;

    session.setBackend(&backend);
    terminal.setSession(&session);

    QCOMPARE(backend.lastResizeColumns, 80);
    QCOMPARE(backend.lastResizeRows, 24);

    backend.pushData("hello\r\nworld");
    QCOMPARE(terminal.surfaceModel()->plainText(), "hello\nworld"_L1);

    terminal.sendKey(Qt::Key_Up);
    QCOMPARE(backend.writes.last(), QByteArray("\x1b[A"));

    terminal.feedText("\x1b[?1h\x1b[?2004h"_L1);
    terminal.sendKey(Qt::Key_Up);
    QCOMPARE(backend.writes.last(), QByteArray("\x1bOA"));

    terminal.sendPaste("hi"_L1);
    QCOMPARE(backend.writes.last(), QByteArray("\x1b[200~hi\x1b[201~"));

    terminal.setTerminalSize(100, 42);
    QCOMPARE(backend.lastResizeColumns, 100);
    QCOMPARE(backend.lastResizeRows, 42);
}

void QTermSessionTest::terminalDecodesSplitUtf8AcrossChunks()
{
    QTermTerminal terminal;
    QTermSession session;
    FakeSessionBackend backend;

    session.setBackend(&backend);
    terminal.setSession(&session);

    backend.pushData(QByteArray::fromHex("e4"));
    QCOMPARE(terminal.surfaceModel()->plainText(), QString());

    backend.pushData(QByteArray::fromHex("b8ad"));
    QCOMPARE(terminal.surfaceModel()->plainText(), QString::fromUtf8("中"));
}

void QTermSessionTest::terminalUpdatesTitleFromOscSequence()
{
    QTermTerminal terminal;
    QTermSession session;
    FakeSessionBackend backend;

    session.setBackend(&backend);
    terminal.setSession(&session);

    backend.pushData(QByteArray("\x1b]2;live-title\x07"));

    QCOMPARE(terminal.title(), "live-title"_L1);
}

void QTermSessionTest::terminalEmitsBellFromSessionData()
{
    QTermTerminal terminal;
    QTermSession session;
    FakeSessionBackend backend;
    QSignalSpy bellSpy(&terminal, &QTermTerminal::bell);

    session.setBackend(&backend);
    terminal.setSession(&session);

    backend.pushData(QByteArray("before\aafter"));

    QCOMPARE(bellSpy.size(), 1);
    QCOMPARE(terminal.surfaceModel()->plainText(), "beforeafter"_L1);
}

void QTermSessionTest::terminalProjectsCursorStateToSurfaceModel()
{
    QTermTerminal terminal;

    terminal.feedText("ab\r\ncd"_L1);

    QCOMPARE(terminal.surfaceModel()->cursorRow(), 1);
    QCOMPARE(terminal.surfaceModel()->cursorColumn(), 2);
    QVERIFY(terminal.surfaceModel()->cursorVisible());
}

void QTermSessionTest::terminalProjectsCursorVisibilityMode()
{
    QTermTerminal terminal;

    terminal.feedText("\x1b[?25l"_L1);
    QVERIFY(!terminal.surfaceModel()->cursorVisible());

    terminal.feedText("\x1b[?25h"_L1);
    QVERIFY(terminal.surfaceModel()->cursorVisible());
}

void QTermSessionTest::terminalHidesCursorWhenScrolledFromBottom()
{
    QTermTerminal terminal;

    terminal.setTerminalSize(5, 2);
    terminal.feedText("alpha\r\nbeta\r\ngamma"_L1);

    QVERIFY(terminal.surfaceModel()->cursorVisible());
    QCOMPARE(terminal.surfaceModel()->cursorRow(), 1);
    QCOMPARE(terminal.surfaceModel()->cursorColumn(), 4);

    terminal.scrollByLines(1);

    QCOMPARE(terminal.scrollOffset(), 1);
    QVERIFY(!terminal.surfaceModel()->cursorVisible());

    terminal.scrollToBottom();

    QCOMPARE(terminal.scrollOffset(), 0);
    QVERIFY(terminal.surfaceModel()->cursorVisible());
    QCOMPARE(terminal.surfaceModel()->cursorRow(), 1);
    QCOMPARE(terminal.surfaceModel()->cursorColumn(), 4);
}

void QTermSessionTest::terminalProjectsVisibleLinesToSurfaceModel()
{
    QTermTerminal terminal;

    terminal.feedText("ab\r\ncd"_L1);

    const QStringList visibleLines = terminal.surfaceModel()->visibleLines();
    QCOMPARE(visibleLines.size(), terminal.rows());
    QCOMPARE(visibleLines.at(0), "ab"_L1);
    QCOMPARE(visibleLines.at(1), "cd"_L1);
}

void QTermSessionTest::terminalKeepsVisibleLineCountOnResize()
{
    QTermTerminal terminal;

    terminal.setTerminalSize(20, 6);

    const QStringList visibleLines = terminal.surfaceModel()->visibleLines();
    QCOMPARE(visibleLines.size(), 6);
    QCOMPARE(visibleLines.at(0), QString());
    QCOMPARE(visibleLines.at(5), QString());
}

void QTermSessionTest::terminalProjectsVisibleLineRunsToSurfaceModel()
{
    QTermTerminal terminal;

    terminal.feedText("\x1b[1;31mA\x1b[4;34mB\x1b[0mC"_L1);

    const QVariantList lineRuns = terminal.surfaceModel()->visibleLineRuns();
    QCOMPARE(lineRuns.size(), terminal.rows());

    const QVariantList firstLineRuns = lineRuns.at(0).toList();
    QCOMPARE(firstLineRuns.size(), 3);

    const QVariantMap firstRun = firstLineRuns.at(0).toMap();
    QCOMPARE(firstRun.value(QStringLiteral("text")).toString(), "A"_L1);
    QVERIFY(firstRun.value(QStringLiteral("bold")).toBool());
    QCOMPARE(firstRun.value(QStringLiteral("foregroundIndex")).toInt(), 1);

    const QVariantMap secondRun = firstLineRuns.at(1).toMap();
    QCOMPARE(secondRun.value(QStringLiteral("text")).toString(), "B"_L1);
    QVERIFY(secondRun.value(QStringLiteral("underline")).toBool());
    QCOMPARE(secondRun.value(QStringLiteral("foregroundIndex")).toInt(), 4);

    const QVariantMap thirdRun = firstLineRuns.at(2).toMap();
    QCOMPARE(thirdRun.value(QStringLiteral("text")).toString(), QStringLiteral("C") + QString(77, u' '));
    QVERIFY(!thirdRun.value(QStringLiteral("bold")).toBool());
    QCOMPARE(thirdRun.value(QStringLiteral("foregroundIndex")).toInt(), -1);
    QCOMPARE(thirdRun.value(QStringLiteral("foregroundRgb")).toInt(), -1);
}

void QTermSessionTest::terminalProjectsExtendedColorsToSurfaceModel()
{
    QTermTerminal terminal;

    terminal.feedText("\x1b[38;5;196mA\x1b[38;2;12;34;56mB\x1b[0mC"_L1);

    const QVariantList lineRuns = terminal.surfaceModel()->visibleLineRuns();
    const QVariantList firstLineRuns = lineRuns.at(0).toList();
    QCOMPARE(firstLineRuns.size(), 3);

    const QVariantMap firstRun = firstLineRuns.at(0).toMap();
    QCOMPARE(firstRun.value(QStringLiteral("foregroundIndex")).toInt(), 196);
    QCOMPARE(firstRun.value(QStringLiteral("foregroundRgb")).toInt(), -1);

    const QVariantMap secondRun = firstLineRuns.at(1).toMap();
    QCOMPARE(secondRun.value(QStringLiteral("foregroundIndex")).toInt(), -1);
    QCOMPARE(secondRun.value(QStringLiteral("foregroundRgb")).toInt(), 0x0c2238);
}

void QTermSessionTest::terminalProjectsDecoratedRunsToSurfaceModel()
{
    QTermTerminal terminal;

    terminal.feedText("\x1b[2;7;9mA\x1b[22;27;29mB"_L1);

    const QVariantList lineRuns = terminal.surfaceModel()->visibleLineRuns();
    const QVariantList firstLineRuns = lineRuns.at(0).toList();
    QCOMPARE(firstLineRuns.size(), 2);

    const QVariantMap firstRun = firstLineRuns.at(0).toMap();
    QVERIFY(firstRun.value(QStringLiteral("dim")).toBool());
    QVERIFY(firstRun.value(QStringLiteral("inverse")).toBool());
    QVERIFY(firstRun.value(QStringLiteral("strikethrough")).toBool());

    const QVariantMap secondRun = firstLineRuns.at(1).toMap();
    QVERIFY(!secondRun.value(QStringLiteral("dim")).toBool());
    QVERIFY(!secondRun.value(QStringLiteral("inverse")).toBool());
    QVERIFY(!secondRun.value(QStringLiteral("strikethrough")).toBool());
}

void QTermSessionTest::terminalControlsSelectionFromTerminalApi()
{
    QTermTerminal terminal;

    terminal.feedText("abc\r\ndef"_L1);
    terminal.setSelectionRange(1, 2, 0, 1);

    QVERIFY(terminal.surfaceModel()->hasSelection());
    QCOMPARE(terminal.surfaceModel()->selectionStartRow(), 0);
    QCOMPARE(terminal.surfaceModel()->selectionStartColumn(), 1);
    QCOMPARE(terminal.surfaceModel()->selectionEndRow(), 1);
    QCOMPARE(terminal.surfaceModel()->selectionEndColumn(), 2);
    QCOMPARE(terminal.surfaceModel()->selectedText(), "bc\nde"_L1);

    terminal.clearSelection();
    QVERIFY(!terminal.surfaceModel()->hasSelection());
    QCOMPARE(terminal.surfaceModel()->selectedText(), QString());
}

void QTermSessionTest::surfaceModelDelegatesSelectionControlToTerminal()
{
    QTermTerminal terminal;

    terminal.feedText("alpha beta"_L1);
    terminal.surfaceModel()->setSelectionRange(0, 0, 0, 5);

    QVERIFY(terminal.surfaceModel()->hasSelection());
    QCOMPARE(terminal.surfaceModel()->selectedText(), "alpha"_L1);

    terminal.surfaceModel()->clearSelection();
    QVERIFY(!terminal.surfaceModel()->hasSelection());
}

void QTermSessionTest::terminalSelectsWordByViewportColumns()
{
    QTermTerminal terminal;

    terminal.feedText(QString::fromUtf8(u8"中a b!"));
    terminal.selectWordAt(0, 1);
    QCOMPARE(terminal.surfaceModel()->selectedText(), QString::fromUtf8(u8"中a"));

    terminal.selectWordAt(0, 4);
    QCOMPARE(terminal.surfaceModel()->selectedText(), "b"_L1);

    terminal.selectWordAt(0, 5);
    QCOMPARE(terminal.surfaceModel()->selectedText(), "!"_L1);
}

void QTermSessionTest::terminalSelectsWordAcrossSoftWrappedRows()
{
    QTermTerminal terminal;

    terminal.setTerminalSize(3, 4);
    terminal.feedText("alpha"_L1);
    terminal.selectWordAt(1, 1);

    QCOMPARE(terminal.surfaceModel()->selectionStartRow(), 0);
    QCOMPARE(terminal.surfaceModel()->selectionStartColumn(), 0);
    QCOMPARE(terminal.surfaceModel()->selectionEndRow(), 1);
    QCOMPARE(terminal.surfaceModel()->selectionEndColumn(), 2);
    QCOMPARE(terminal.surfaceModel()->selectedText(), "alpha"_L1);
}

void QTermSessionTest::terminalSelectsLogicalLineAcrossSoftWrappedRows()
{
    QTermTerminal terminal;

    terminal.setTerminalSize(3, 4);
    terminal.feedText("alpha\r\n  beta"_L1);
    terminal.selectLogicalLineAt(2);

    QCOMPARE(terminal.surfaceModel()->selectionStartRow(), 2);
    QCOMPARE(terminal.surfaceModel()->selectionStartColumn(), 0);
    QCOMPARE(terminal.surfaceModel()->selectionEndRow(), 3);
    QCOMPARE(terminal.surfaceModel()->selectionEndColumn(), 3);
    QCOMPARE(terminal.surfaceModel()->selectedText(), "  beta"_L1);

    terminal.selectLogicalLineAt(1);
    QCOMPARE(terminal.surfaceModel()->selectionStartRow(), 0);
    QCOMPARE(terminal.surfaceModel()->selectionStartColumn(), 0);
    QCOMPARE(terminal.surfaceModel()->selectionEndRow(), 1);
    QCOMPARE(terminal.surfaceModel()->selectionEndColumn(), 2);
    QCOMPARE(terminal.surfaceModel()->selectedText(), "alpha"_L1);
}

void QTermSessionTest::terminalSelectsShellStyleTokenByViewportColumns()
{
    QTermTerminal terminal;

    terminal.feedText("/tmp/foo-bar.txt foo::bar"_L1);

    terminal.selectWordAt(0, 9);
    QCOMPARE(terminal.surfaceModel()->selectedText(), "/tmp/foo-bar.txt"_L1);

    terminal.selectWordAt(0, 22);
    QCOMPARE(terminal.surfaceModel()->selectedText(), "foo::bar"_L1);
}

void QTermSessionTest::terminalSelectsUrlAndAssignmentTokensByViewportColumns()
{
    QTermTerminal terminal;

    terminal.feedText("https://example.com/a?b=1#frag FOO=bar"_L1);

    terminal.selectWordAt(0, 12);
    QCOMPARE(terminal.surfaceModel()->selectedText(), "https://example.com/a?b=1#frag"_L1);

    terminal.selectWordAt(0, 36);
    QCOMPARE(terminal.surfaceModel()->selectedText(), "FOO=bar"_L1);
}

void QTermSessionTest::terminalExcludesTrailingPunctuationFromUrlSelection()
{
    QTermTerminal terminal;

    terminal.feedText("(https://example.com/a?b=1#frag),"_L1);

    terminal.selectWordAt(0, 12);
    QCOMPARE(terminal.surfaceModel()->selectedText(), "https://example.com/a?b=1#frag"_L1);
}

void QTermSessionTest::terminalExcludesSentenceTrailingDotFromUrlSelection()
{
    QTermTerminal terminal;

    terminal.feedText("See https://example.com/path."_L1);

    terminal.selectWordAt(0, 12);
    QCOMPARE(terminal.surfaceModel()->selectedText(), "https://example.com/path"_L1);
}

void QTermSessionTest::terminalExcludesTrailingPunctuationFromGenericUrlSchemes()
{
    QTermTerminal terminal;

    terminal.feedText("mailto:user@example.com, file:///tmp/foo.txt; www.example.com/path."_L1);

    terminal.selectWordAt(0, 10);
    QCOMPARE(terminal.surfaceModel()->selectedText(), "mailto:user@example.com"_L1);

    terminal.selectWordAt(0, 33);
    QCOMPARE(terminal.surfaceModel()->selectedText(), "file:///tmp/foo.txt"_L1);

    terminal.selectWordAt(0, 56);
    QCOMPARE(terminal.surfaceModel()->selectedText(), "www.example.com/path"_L1);
}

void QTermSessionTest::terminalExtractsSelectedTextFromSurfaceModel()
{
    QTermTerminal terminal;

    terminal.feedText("abc\r\ndef"_L1);
    terminal.setSelectionRange(1, 2, 0, 1);

    QVERIFY(terminal.surfaceModel()->hasSelection());
    QCOMPARE(terminal.surfaceModel()->selectionStartRow(), 0);
    QCOMPARE(terminal.surfaceModel()->selectionStartColumn(), 1);
    QCOMPARE(terminal.surfaceModel()->selectionEndRow(), 1);
    QCOMPARE(terminal.surfaceModel()->selectionEndColumn(), 2);
    QCOMPARE(terminal.surfaceModel()->selectedText(), "bc\nde"_L1);

    terminal.clearSelection();
    QVERIFY(!terminal.surfaceModel()->hasSelection());
    QCOMPARE(terminal.surfaceModel()->selectedText(), QString());
}

void QTermSessionTest::terminalExtractsWideCharacterSelectionByColumns()
{
    QTermTerminal terminal;
    const QString text = QString::fromUtf8(u8"中a");

    terminal.feedText(text);
    terminal.setSelectionRange(0, 0, 0, 2);
    QCOMPARE(terminal.surfaceModel()->selectedText(), QString::fromUtf8(u8"中"));

    terminal.setSelectionRange(0, 0, 0, 3);
    QCOMPARE(terminal.surfaceModel()->selectedText(), text);
}

void QTermSessionTest::terminalPreservesSelectionAcrossReflowResize()
{
    QTermTerminal terminal;

    terminal.setTerminalSize(10, 4);
    terminal.feedText("alpha beta"_L1);
    terminal.selectWordAt(0, 7);

    QCOMPARE(terminal.surfaceModel()->selectedText(), "beta"_L1);
    QVERIFY(terminal.surfaceModel()->selectionVisible());

    terminal.setTerminalSize(5, 4);

    QVERIFY(terminal.surfaceModel()->hasSelection());
    QVERIFY(terminal.surfaceModel()->selectionVisible());
    QCOMPARE(terminal.surfaceModel()->selectedText(), "beta"_L1);
    QCOMPARE(terminal.surfaceModel()->selectionStartRow(), 1);
    QCOMPARE(terminal.surfaceModel()->selectionStartColumn(), 1);
    QCOMPARE(terminal.surfaceModel()->selectionEndRow(), 1);
    QCOMPARE(terminal.surfaceModel()->selectionEndColumn(), 5);

    terminal.setTerminalSize(10, 4);

    QCOMPARE(terminal.surfaceModel()->selectedText(), "beta"_L1);
    QVERIFY(terminal.surfaceModel()->selectionVisible());
    QCOMPARE(terminal.surfaceModel()->selectionStartRow(), 0);
    QCOMPARE(terminal.surfaceModel()->selectionStartColumn(), 6);
    QCOMPARE(terminal.surfaceModel()->selectionEndRow(), 0);
    QCOMPARE(terminal.surfaceModel()->selectionEndColumn(), 10);
}

void QTermSessionTest::terminalKeepsSelectedTextWhenResizePushesSelectionIntoHistory()
{
    QTermTerminal terminal;

    terminal.setTerminalSize(10, 2);
    terminal.feedText("alpha beta\r\ngamma"_L1);
    terminal.selectWordAt(0, 1);

    QCOMPARE(terminal.surfaceModel()->selectedText(), "alpha"_L1);
    QCOMPARE(terminal.surfaceModel()->selectionStartRow(), 0);
    QCOMPARE(terminal.surfaceModel()->selectionEndRow(), 0);

    terminal.setTerminalSize(5, 2);

    QVERIFY(terminal.surfaceModel()->hasSelection());
    QVERIFY(!terminal.surfaceModel()->selectionVisible());
    QCOMPARE(terminal.surfaceModel()->selectedText(), "alpha"_L1);
    QCOMPARE(terminal.surfaceModel()->selectionStartRow(), -1);
    QCOMPARE(terminal.surfaceModel()->selectionStartColumn(), 0);
    QCOMPARE(terminal.surfaceModel()->selectionEndRow(), -1);
    QCOMPARE(terminal.surfaceModel()->selectionEndColumn(), 0);

    terminal.setTerminalSize(10, 2);

    // With the distance-from-bottom policy, "alpha beta" returns to the
    // visible area when widening back: the cursor ("gamma") stays at row 1
    // (bottom) and "alpha beta" takes row 0. The selection becomes visible.
    QVERIFY(terminal.surfaceModel()->hasSelection());
    QVERIFY(terminal.surfaceModel()->selectionVisible());
    QCOMPARE(terminal.surfaceModel()->selectedText(), "alpha"_L1);
    QCOMPARE(terminal.surfaceModel()->selectionStartRow(), 0);
    QCOMPARE(terminal.surfaceModel()->selectionEndRow(), 0);
}

void QTermSessionTest::surfaceModelProjectsSelectionVisibility()
{
    QTermSurfaceModel surfaceModel;

    QVERIFY(!surfaceModel.hasSelection());
    QVERIFY(!surfaceModel.selectionVisible());

    surfaceModel.setSelectionSnapshot(true, 0, 1, 0, 4, "bcd"_L1);
    QVERIFY(surfaceModel.hasSelection());
    QVERIFY(surfaceModel.selectionVisible());

    surfaceModel.setSelectionSnapshot(true, -1, 0, -1, 0, "alpha"_L1);
    QVERIFY(surfaceModel.hasSelection());
    QVERIFY(!surfaceModel.selectionVisible());
    QCOMPARE(surfaceModel.selectedText(), "alpha"_L1);

    surfaceModel.clearSelection();
    QVERIFY(!surfaceModel.hasSelection());
    QVERIFY(!surfaceModel.selectionVisible());
}

void QTermSessionTest::terminalSelectsFromScrolledViewport()
{
    QTermTerminal terminal;

    terminal.setTerminalSize(5, 2);
    terminal.feedText("alpha\r\nbeta\r\ngamma"_L1);

    QCOMPARE(terminal.surfaceModel()->visibleLines().at(0), "beta"_L1);
    QCOMPARE(terminal.surfaceModel()->visibleLines().at(1), "gamma"_L1);

    terminal.scrollByLines(1);
    QCOMPARE(terminal.scrollOffset(), 1);
    QCOMPARE(terminal.surfaceModel()->visibleLines().at(0), "alpha"_L1);
    QCOMPARE(terminal.surfaceModel()->visibleLines().at(1), "beta"_L1);

    terminal.setSelectionRange(0, 0, 0, 5);
    QVERIFY(terminal.surfaceModel()->hasSelection());
    QVERIFY(terminal.surfaceModel()->selectionVisible());
    QCOMPARE(terminal.surfaceModel()->selectedText(), "alpha"_L1);
    QCOMPARE(terminal.surfaceModel()->selectionStartRow(), 0);
    QCOMPARE(terminal.surfaceModel()->selectionEndRow(), 0);

    terminal.scrollToBottom();
    QCOMPARE(terminal.scrollOffset(), 0);
    QVERIFY(terminal.surfaceModel()->hasSelection());
    QVERIFY(!terminal.surfaceModel()->selectionVisible());
    QCOMPARE(terminal.surfaceModel()->selectedText(), "alpha"_L1);

    terminal.scrollByLines(1);
    QVERIFY(terminal.surfaceModel()->selectionVisible());
    QCOMPARE(terminal.surfaceModel()->selectionStartRow(), 0);
    QCOMPARE(terminal.surfaceModel()->selectionEndRow(), 0);
}

void QTermSessionTest::terminalPreservesAllPromptLinesAcrossRepeatedWidthOscillation()
{
    // Reproduces the real-world bug reported by the user:
    //
    //   "I press Enter 5 times, then drag the window width from wide (~1120px)
    //    to narrow (~50px) and back, repeated 3-5 times. Eventually some
    //    prompt lines disappear — 5 lines become 2."
    //
    // This test drives QTermTerminal (the same path as QTermQuickPaintedItem) and
    // simulates the full interaction:
    //   1. Feed 5 prompt lines as if printed by a real shell session.
    //   2. Repeatedly oscillate the terminal width (wide→narrow→wide).
    //   3. After each width change, feed the shell's SIGWINCH redraw output
    //      (\r + ESC[K + prompt reprint) the same way the PTY backend delivers
    //      it via dataReceived → feedText.
    //   4. Assert that all 5 prompt lines are still visible / in the buffer
    //      after every resize+redraw cycle.
    //
    // The prompt text is deliberately long (fills most of a wide terminal) so
    // that it wraps when the terminal is narrowed, triggering the reflow path
    // that previously caused data loss.

    QTermTerminal terminal;

    // Wide terminal — matches a typical desktop window width.
    terminal.setTerminalSize(80, 24);

    // A prompt typical of zsh with a path component, without ANSI codes to
    // keep assertions simple.  Length = 51 chars, fits in 80 cols (no wrap),
    // but wraps into 2 rows when width is ≤ 50 cols.
    const QString prompt = u"➜  tangjc@MBP /Users/tangjc/1-proj/2-mygithub/qterm"_s;

    // Feed 4 completed prompt lines (Enter pressed 4 times) + the 5th active prompt.
    QString transcript;
    for (int i = 0; i < 4; ++i) {
        transcript += prompt + u"\r\n"_s;
    }
    transcript += prompt;  // 5th prompt — cursor sits at its end.
    terminal.feedText(transcript);

    // Helper: collect the full buffer text (history + visible) from the model.
    // plainText() on QTermSurfaceModel reflects debugPlainText() of the core,
    // which includes both history and visible lines.
    auto bufferText = [&]() {
        return terminal.surfaceModel()->plainText();
    };

    const QString expected = QStringList(5, prompt).join(u'\n');
    QCOMPARE(bufferText(), expected);

    // Simulate the user dragging the window width 5 times, each time:
    //   (a) Narrow the terminal  → reflow wraps the long prompt.
    //   (b) Shell receives SIGWINCH and sends \r + ESC[K + prompt.
    //   (c) Widen the terminal   → reflow should restore the original layout.
    //   (d) Shell receives SIGWINCH again and redraws at wide width.
    for (int cycle = 0; cycle < 5; ++cycle) {
        // (a) Narrow — prompt (51 chars) wraps into 2 physical rows.
        terminal.setTerminalSize(40, 24);
        QCOMPARE(bufferText(), expected);

        // (b) Shell redraws at narrow width: CR moves to physical row start,
        //     ESC[K erases to end of line, then the prompt is reprinted.
        //     This is exactly what zsh sends after SIGWINCH.
        terminal.feedText(u"\r\x1b[K"_s + prompt);
        QCOMPARE(bufferText(), expected);

        // (c) Widen back.
        terminal.setTerminalSize(80, 24);
        QCOMPARE(bufferText(), expected);

        // (d) Shell redraws at wide width.
        terminal.feedText(u"\r\x1b[K"_s + prompt);
        QCOMPARE(bufferText(), expected);
    }
}

void QTermSessionTest::terminalPreservesAllPromptLinesWhenResizeDebouncedBeforeRedraw()
{
    // Reproduces the real-world bug more precisely:
    //
    // When QTermQuickPaintedItem calls setTerminalSize synchronously on every geometry
    // change (no debounce), the buffer is reflowed many times during a single
    // drag gesture. The PTY backend debounces TIOCSWINSZ, so the shell only
    // gets ONE SIGWINCH at the end. The shell then redraws at the *final* width,
    // but the buffer has been reflowed through many intermediate widths first.
    //
    // This test simulates: rapid narrowing through multiple intermediate sizes
    // (as the user drags the window edge pixel by pixel), followed by a single
    // shell redraw at the final narrow width, then widening back.
    // The bug manifests as prompt lines disappearing after the widen.

    QTermTerminal terminal;
    terminal.setTerminalSize(80, 24);

    // Prompt length = 51 chars — wraps at widths <= 50.
    const QString prompt = u"➜  tangjc@MBP /Users/tangjc/1-proj/2-mygithub/qterm"_s;

    QString transcript;
    for (int i = 0; i < 4; ++i) {
        transcript += prompt + u"\r\n"_s;
    }
    transcript += prompt;
    terminal.feedText(transcript);

    const QString expected = QStringList(5, prompt).join(u'\n');
    QCOMPARE(terminal.surfaceModel()->plainText(), expected);

    auto bufferText = [&]() { return terminal.surfaceModel()->plainText(); };

    // --- Simulate 3 full drag cycles ---
    // Each cycle: many intermediate resize steps (pixel-by-pixel drag),
    // ONE shell SIGWINCH redraw at the final size, then drag back.
    for (int cycle = 0; cycle < 3; ++cycle) {
        // Pixel-by-pixel narrowing: each step is a setTerminalSize call
        // as QTermQuickPaintedItem fires on every geometryChange.
        // We simulate this with a few representative intermediate widths
        // (the real drag would have tens of steps, one per pixel column).
        for (int w = 75; w >= 35; w -= 5) {
            terminal.setTerminalSize(w, 24);
            // No QCOMPARE here — the intermediate states may have wrapping,
            // but the content must still be correct.
        }
        // Final narrow width for this cycle.
        terminal.setTerminalSize(30, 24);

        // Shell receives SIGWINCH and redraws at width 30.
        // This is the debounced TIOCSWINSZ — the shell didn't see the
        // intermediate sizes at all.
        terminal.feedText(u"\r\x1b[K"_s + prompt);
        QCOMPARE(bufferText(), expected);

        // Now widen back (pixel-by-pixel again).
        for (int w = 35; w <= 75; w += 5) {
            terminal.setTerminalSize(w, 24);
        }
        terminal.setTerminalSize(80, 24);
        QCOMPARE(bufferText(), expected);

        // Shell redraws at wide width.
        terminal.feedText(u"\r\x1b[K"_s + prompt);
        QCOMPARE(bufferText(), expected);
    }
}

void QTermSessionTest::terminalPreservesZshStyledPromptAcrossWidthOscillation()
{
    // Reproduces the reported bug using the actual prompt format from the
    // bug report, including ANSI color codes exactly as a real zsh would emit.
    //
    // Real zsh prompt (the one in BUGS.md):
    //   "➜  tangjc@MBP /Users/tangjc/1-proj/2-mygithub/qterm/build/examples/quick-demo"
    //
    // After SIGWINCH, zsh sends a redraw sequence like:
    //   \r + ESC[K + (prompt with color codes)
    //
    // The plain-text length of the prompt is 80 chars (fills an 80-col terminal
    // exactly), so at any width < 80 it wraps.

    QTermTerminal terminal;
    terminal.setTerminalSize(120, 24);

    // Actual zsh prompt from the bug report with typical ANSI color codes.
    // Color codes: \x1b[1;32m (bold green), \x1b[1;34m (bold blue), \x1b[0m (reset).
    // Plain-text visible length = 80 chars.
    const QString coloredPrompt =
        u"\x1b[1;32m➜ \x1b[0m \x1b[1;34mtangjc@MBP\x1b[0m "
        u"/Users/tangjc/1-proj/2-mygithub/qterm/build/examples/quick-demo"_s;
    // Plain text equivalent (what appears on screen, ANSI codes stripped for assertion).
    const QString plainPrompt =
        u"➜  tangjc@MBP /Users/tangjc/1-proj/2-mygithub/qterm/build/examples/quick-demo"_s;

    // Simulate 5 Enter presses: 4 completed lines + 1 active prompt.
    for (int i = 0; i < 4; ++i) {
        terminal.feedText(coloredPrompt + u"\r\n"_s);
    }
    terminal.feedText(coloredPrompt);

    const QString expected = QStringList(5, plainPrompt).join(u'\n');
    QCOMPARE(terminal.surfaceModel()->plainText(), expected);

    auto bufferText = [&]() { return terminal.surfaceModel()->plainText(); };

    // Repeat the user's drag gesture 5 times (matching "3-5 times" in bug report).
    for (int cycle = 0; cycle < 5; ++cycle) {
        // Simulate pixel-by-pixel drag from 120 → ~50 cols.
        // The buffer reflows at every step (QTermQuickPaintedItem is synchronous now).
        for (int w = 110; w >= 50; w -= 10) {
            terminal.setTerminalSize(w, 24);
        }

        // Shell receives SIGWINCH at the final narrow width and redraws.
        terminal.feedText(u"\r\x1b[K"_s + coloredPrompt);
        QCOMPARE(bufferText(), expected);

        // Drag back to wide.
        for (int w = 60; w <= 110; w += 10) {
            terminal.setTerminalSize(w, 24);
        }
        terminal.setTerminalSize(120, 24);
        QCOMPARE(bufferText(), expected);

        // Shell redraws at wide width.
        terminal.feedText(u"\r\x1b[K"_s + coloredPrompt);
        QCOMPARE(bufferText(), expected);
    }
}

void QTermSessionTest::terminalPreservesPromptLinesWhenShellUsesAbsoluteColumnMove()
{
    // Some shells (and zsh in certain configurations) use CSI G (CHA, move to
    // absolute column) instead of CR (\r) when redrawing the prompt after SIGWINCH.
    // ESC[1G is equivalent to \r (move to column 1) but goes through a different
    // code path. Our breakPredecessorWrapOnWrite mechanism is only triggered by
    // carriageReturn(), so if the shell sends ESC[1G instead, the predecessor wrap
    // chain is NOT severed, leaving orphaned wrap fragments that cause line loss.
    //
    // This test documents that ESC[1G + ESC[K + prompt must work identically to
    // \r + ESC[K + prompt.

    QTermTerminal terminal;
    terminal.setTerminalSize(80, 24);

    const QString prompt = u"➜  tangjc@MBP /Users/tangjc/1-proj/2-mygithub/qterm"_s;  // 51 chars

    QString transcript;
    for (int i = 0; i < 4; ++i) {
        transcript += prompt + u"\r\n"_s;
    }
    transcript += prompt;
    terminal.feedText(transcript);

    const QString expected = QStringList(5, prompt).join(u'\n');
    QCOMPARE(terminal.surfaceModel()->plainText(), expected);

    auto bufferText = [&]() { return terminal.surfaceModel()->plainText(); };

    // Shell uses ESC[1G (absolute column 1) instead of CR.
    // ESC[1G = move cursor to column 1 of current row.
    // This is CSI 1 G (CHA), which is NOT implemented → ignored.
    // Without CHA support, breakPredecessorWrapOnWrite is never set,
    // so the predecessor wrap chain is never severed.
    terminal.setTerminalSize(40, 24);
    QCOMPARE(bufferText(), expected);

    // Shell redraws using ESC[1G (absolute column move) instead of \r.
    terminal.feedText(u"\x1b[1G\x1b[K"_s + prompt);

    // This QCOMPARE is expected to FAIL with the current implementation
    // because CSI G is not handled — the cursor does not move to column 1
    // and breakPredecessorWrapOnWrite is not set.
    QCOMPARE(bufferText(), expected);

    terminal.setTerminalSize(80, 24);
    QCOMPARE(bufferText(), expected);
}

} // namespace QTerm

QTEST_MAIN(QTerm::QTermSessionTest)

#include "QTermSessionTest.moc"