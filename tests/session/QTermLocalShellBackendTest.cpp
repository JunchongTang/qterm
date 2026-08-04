#include <QtTest>

#include <QRegularExpression>

#include <QTerm/QTermLocalShellBackend.h>

namespace QTerm {

class QTermLocalShellBackendTest : public QObject
{
    Q_OBJECT

private slots:
    void openReadsConfiguredInitialSize();
    void roundTripsInteractiveInput();
    void appliesLatestResizeAfterOpen();
    void opensAndConnectsOnWindows();
};

// ── Unix tests ────────────────────────────────────────────────────────────────

void QTermLocalShellBackendTest::openReadsConfiguredInitialSize()
{
#if !defined(Q_OS_UNIX)
    QSKIP("Unix PTY size test skipped on non-Unix.");
#else
    QTermLocalShellBackend backend;
    QByteArray output;

    backend.setProgram(QStringLiteral("/bin/sh"));
    backend.setArguments({QStringLiteral("-c"), QStringLiteral("stty size")});
    backend.resize(132, 43);

    connect(&backend, &QTermSessionBackend::dataReceived, this, [&output](const QByteArray &data) {
        output.append(data);
    });

    backend.open();

    QTRY_VERIFY_WITH_TIMEOUT(output.contains("43 132"), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(backend.state(), QTermSessionBackend::Closed, 3000);
#endif
}

void QTermLocalShellBackendTest::roundTripsInteractiveInput()
{
#if !defined(Q_OS_UNIX)
    QSKIP("Unix PTY interactive test skipped on non-Unix.");
#else
    QTermLocalShellBackend backend;
    QByteArray output;
    const QString script = QStringLiteral("printf 'ready\\n'; IFS= read line; printf 'reply:%s\\n' \"$line\"");

    backend.setProgram(QStringLiteral("/bin/sh"));
    backend.setArguments({QStringLiteral("-c"), script});

    connect(&backend, &QTermSessionBackend::dataReceived, this, [&output](const QByteArray &data) {
        output.append(data);
    });

    backend.open();

    QTRY_VERIFY_WITH_TIMEOUT(output.contains("ready"), 3000);

    backend.writeData(QByteArray("ping\n"));

    QTRY_VERIFY_WITH_TIMEOUT(output.contains("reply:ping"), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(backend.state(), QTermSessionBackend::Closed, 3000);
#endif
}

void QTermLocalShellBackendTest::appliesLatestResizeAfterOpen()
{
#if !defined(Q_OS_UNIX)
    QSKIP("Unix PTY resize test skipped on non-Unix.");
#else
    QTermLocalShellBackend backend;
    QByteArray output;
    const QString script = QStringLiteral("sleep 0.3; stty size");

    backend.setProgram(QStringLiteral("/bin/sh"));
    backend.setArguments({QStringLiteral("-c"), script});

    connect(&backend, &QTermSessionBackend::dataReceived, this, [&output](const QByteArray &data) {
        output.append(data);
    });

    backend.open();
    backend.resize(90, 31);
    backend.resize(100, 35);

    QTRY_VERIFY_WITH_TIMEOUT(output.contains("35 100"), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(backend.state(), QTermSessionBackend::Closed, 3000);
#endif
}

// ── Windows test ──────────────────────────────────────────────────────────────

void QTermLocalShellBackendTest::opensAndConnectsOnWindows()
{
#if !defined(Q_OS_WIN)
    QSKIP("Windows ConPTY test skipped on non-Windows.");
#else
    QTermLocalShellBackend backend;
    QByteArray output;

    // Run a one-shot command via cmd.exe and verify we receive output.
    backend.setProgram(QStringLiteral("cmd.exe"));
    backend.setArguments({QStringLiteral("/c"), QStringLiteral("echo qterm-ok")});

    connect(&backend, &QTermSessionBackend::dataReceived, this, [&output](const QByteArray &data) {
        output.append(data);
    });

    backend.open();
    QCOMPARE(backend.state(), QTermSessionBackend::Open);

    QTRY_VERIFY_WITH_TIMEOUT(output.contains("qterm-ok"), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(backend.state(), QTermSessionBackend::Closed, 5000);
#endif
}

} // namespace QTerm

QTEST_MAIN(QTerm::QTermLocalShellBackendTest)

#include "QTermLocalShellBackendTest.moc"
