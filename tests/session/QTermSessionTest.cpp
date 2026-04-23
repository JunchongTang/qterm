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
    void terminalProjectsVisibleLinesToSurfaceModel();
    void terminalKeepsVisibleLineCountOnResize();
    void terminalProjectsVisibleLineRunsToSurfaceModel();
    void terminalProjectsExtendedColorsToSurfaceModel();
    void terminalProjectsDecoratedRunsToSurfaceModel();
    void terminalControlsSelectionFromTerminalApi();
    void surfaceModelDelegatesSelectionControlToTerminal();
    void terminalSelectsWordByViewportColumns();
    void terminalSelectsWordAcrossSoftWrappedRows();
    void terminalExtractsSelectedTextFromSurfaceModel();
    void terminalExtractsWideCharacterSelectionByColumns();
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

} // namespace QTerm

QTEST_MAIN(QTerm::QTermSessionTest)

#include "QTermSessionTest.moc"