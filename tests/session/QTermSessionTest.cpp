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

} // namespace QTerm

QTEST_MAIN(QTerm::QTermSessionTest)

#include "QTermSessionTest.moc"