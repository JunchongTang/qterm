#ifndef QTERM_QTERMSERIALBACKEND_H
#define QTERM_QTERMSERIALBACKEND_H

#include <QString>

#include <QtQml/qqmlregistration.h>

#include <QTerm/QTermSessionBackend.h>
#include <QTerm/QTermSerialPortInfo.h>

class QSerialPort;

namespace QTerm {

/*!
    \class QTermSerialBackend
    \inmodule QTerm
    \brief Serial-port session backend based on QSerialPort.

    This backend maps terminal session I/O to a serial device. The resize()
    call is accepted for API consistency and ignored by serial hardware.
*/
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

    /*! \brief Returns the configured serial port name, for example COM3 or /dev/ttyUSB0. */
    QString portName() const;
    /*! \brief Sets the serial port name, for example COM3 or /dev/ttyUSB0. */
    void setPortName(const QString &portName);

    // ── Line parameters ───────────────────────────────────────────────────────
    /*! \brief Returns the configured baud rate. */
    int  baudRate() const noexcept;
    /*! \brief Sets the baud rate supported by the OS driver. */
    void setBaudRate(int baudRate);

    int  dataBits() const noexcept;   // 5/6/7/8
    void setDataBits(int dataBits);

    /*! \brief Returns the configured parity mode as one of N, E, O, M or S. */
    QString parity() const;
    /*! \brief Sets the parity mode as one of N, E, O, M or S. */
    void    setParity(const QString &parity);

    // 1 / 2 (1.5 not exposed — rarely used)
    int  stopBits() const noexcept;
    void setStopBits(int stopBits);

    /*! \brief Returns the configured flow control mode. */
    QString flowControl() const;
    /*! \brief Sets the flow control mode to none, hardware or software. */
    void    setFlowControl(const QString &flowControl);

    // ── QTermSessionBackend interface ─────────────────────────────────────────
    void open() override;
    void close() override;
    void writeData(const QByteArray &data) override;
    void resize(int columns, int rows) override; // no-op for serial

    // Serial is a device connection with no "foreground process" concept —
    // closing merely disconnects the device, there is no process to interrupt.
    // Always Idle, so the in-progress-work confirmation never fires for serial.
    WorkState workState() const override { return WorkIdle; }

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
