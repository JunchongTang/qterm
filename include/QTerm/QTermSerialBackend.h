#ifndef QTERM_QTERMSERIALBACKEND_H
#define QTERM_QTERMSERIALBACKEND_H

#include <QString>

#include <QtQml/qqmlregistration.h>

#include <QTerm/QTermSessionBackend.h>
#include <QTerm/QTermSerialPortInfo.h>

class QSerialPort;

namespace QTerm {

// Serial-port session backend backed by Qt's QSerialPort.
//
// Usage:
//   QTermSerialBackend backend;
//   backend.setPortName("/dev/tty.usbserial-0001");
//   backend.setBaudRate(115200);
//   backend.open();
//   // wire up to QTermSession the same way as QTermLocalPtyBackend
//
// resize() is accepted but ignored — serial devices have no terminal geometry.
class QTermSerialBackend : public QTermSessionBackend
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString portName READ portName WRITE setPortName NOTIFY portNameChanged)
    Q_PROPERTY(int baudRate READ baudRate WRITE setBaudRate NOTIFY baudRateChanged)
    Q_PROPERTY(int dataBits READ dataBits WRITE setDataBits NOTIFY dataBitsChanged)
    Q_PROPERTY(QString parity READ parity WRITE setParity NOTIFY parityChanged)
    Q_PROPERTY(int stopBits READ stopBits WRITE setStopBits NOTIFY stopBitsChanged)
    Q_PROPERTY(QString flowControl READ flowControl WRITE setFlowControl NOTIFY flowControlChanged)

public:
    explicit QTermSerialBackend(QObject *parent = nullptr);
    ~QTermSerialBackend() override;

    // ── Port selection ────────────────────────────────────────────────────────
    QString portName() const;
    void setPortName(const QString &portName);

    // ── Line parameters ───────────────────────────────────────────────────────
    // Use any integer baud rate supported by the OS/driver.
    int  baudRate() const noexcept;
    void setBaudRate(int baudRate);

    int  dataBits() const noexcept;   // 5/6/7/8
    void setDataBits(int dataBits);

    // "N" / "E" / "O" / "M" / "S"  (None/Even/Odd/Mark/Space)
    QString parity() const;
    void    setParity(const QString &parity);

    // 1 / 2 (1.5 not exposed — rarely used)
    int  stopBits() const noexcept;
    void setStopBits(int stopBits);

    // "none" / "hardware" / "software"
    QString flowControl() const;
    void    setFlowControl(const QString &flowControl);

    // ── QTermSessionBackend interface ─────────────────────────────────────────
    void open() override;
    void close() override;
    void writeData(const QByteArray &data) override;
    void resize(int columns, int rows) override; // no-op for serial

signals:
    void portNameChanged();
    void baudRateChanged();
    void dataBitsChanged();
    void parityChanged();
    void stopBitsChanged();
    void flowControlChanged();

private:
    void applySettings();

    QSerialPort *m_serial = nullptr;
    QString m_portName;
    int m_baudRate = 9600;
    int m_dataBits = 8;
    QString m_parity = QStringLiteral("N");
    int m_stopBits = 1;
    QString m_flowControl = QStringLiteral("none");
};

} // namespace QTerm

#endif // QTERM_QTERMSERIALBACKEND_H
