#include <QtTest>

#include <QTerm/QTermSerialBackend.h>
#include <QTerm/QTermSessionBackend.h>

namespace QTerm {

class QTermSerialBackendTest final : public QObject
{
    Q_OBJECT

private slots:
    // Opening a port that cannot be opened must report the failure exactly once.
    //
    // QSerialPort signals errorOccurred *and* returns false from open(), so the naive
    // shape (handle the signal, then also emit from the open() failure branch) reports
    // every failed open twice — visibly so for a consumer that prints the message.
    // The duplicate also carried the wrong kind: a permission error arrived as
    // PermissionDenied and then again as DeviceNotFound.
    void failedOpenReportsOnce()
    {
        QTermSerialBackend backend;
        backend.setPortName(QStringLiteral("qterm-no-such-port"));

        QSignalSpy errors(&backend, &QTermSessionBackend::errorOccurred);
        backend.open();

        QCOMPARE(errors.count(), 1);
        QCOMPARE(backend.state(), QTermSessionBackend::Error);
    }

    void emptyPortNameReportsOnce()
    {
        QTermSerialBackend backend;

        QSignalSpy errors(&backend, &QTermSessionBackend::errorOccurred);
        backend.open();

        QCOMPARE(errors.count(), 1);
        QCOMPARE(errors.first().at(0).toInt(), int(QTermSessionBackend::DeviceNotFound));
        QCOMPARE(backend.state(), QTermSessionBackend::Error);
    }
};

} // namespace QTerm

QTEST_MAIN(QTerm::QTermSerialBackendTest)
#include "QTermSerialBackendTest.moc"
