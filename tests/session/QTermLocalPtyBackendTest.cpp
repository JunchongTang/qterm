#include <QtTest>

#include <QRegularExpression>

#include <QTerm/QTermLocalPtyBackend.h>

namespace QTerm {

class QTermLocalPtyBackendTest : public QObject
{
    Q_OBJECT

private slots:
    void openReadsConfiguredInitialSize();
    void roundTripsInteractiveInput();
    void appliesLatestResizeAfterOpen();
};

void QTermLocalPtyBackendTest::openReadsConfiguredInitialSize()
{
#if !defined(Q_OS_UNIX)
    QSKIP("Local PTY backend requires Unix.");
#else
    QTermLocalPtyBackend backend;
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

void QTermLocalPtyBackendTest::roundTripsInteractiveInput()
{
#if !defined(Q_OS_UNIX)
    QSKIP("Local PTY backend requires Unix.");
#else
    QTermLocalPtyBackend backend;
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

void QTermLocalPtyBackendTest::appliesLatestResizeAfterOpen()
{
#if !defined(Q_OS_UNIX)
    QSKIP("Local PTY backend requires Unix.");
#else
    QTermLocalPtyBackend backend;
    QByteArray output;
    // After open(), two consecutive resize() calls are applied immediately.
    // The second sleep gives the shell time to see the final PTY size.
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

} // namespace QTerm

QTEST_MAIN(QTerm::QTermLocalPtyBackendTest)

#include "QTermLocalPtyBackendTest.moc"