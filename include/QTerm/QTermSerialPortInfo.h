#ifndef QTERM_QTERMSERIALPORTINFO_H
#define QTERM_QTERMSERIALPORTINFO_H

#include <QList>
#include <QObject>
#include <QString>

#include <QtQml/qqmlregistration.h>

QT_FORWARD_DECLARE_CLASS(QSerialPortInfo)

namespace QTerm {

// Describes one discovered serial port device.
// Thin wrapper around QSerialPortInfo providing the same cross-platform
// enumeration without exposing QSerialPortInfo in the public QTerm API.
//
// Registered as a QML value type ("serialPortInfo") so QML code can read
// properties directly from items returned by QTermSerialPortScanner::availablePorts().
class QTermSerialPortInfo
{
    Q_GADGET
    QML_VALUE_TYPE(serialPortInfo)

    Q_PROPERTY(QString portName     READ portName     CONSTANT)
    Q_PROPERTY(QString description  READ description  CONSTANT)
    Q_PROPERTY(QString manufacturer READ manufacturer CONSTANT)
    Q_PROPERTY(quint16 vendorId     READ vendorId     CONSTANT)
    Q_PROPERTY(quint16 productId    READ productId    CONSTANT)
    Q_PROPERTY(bool    isUsb        READ isUsb        CONSTANT)

public:
    QTermSerialPortInfo() = default;
    explicit QTermSerialPortInfo(const QSerialPortInfo &info);

    // Device node / port name, e.g. "/dev/tty.usbserial-0001" or "COM3".
    QString portName() const { return m_portName; }

    // Human-readable description, e.g. "USB-to-Serial Adapter".
    QString description() const { return m_description; }

    // USB manufacturer string, if applicable.
    QString manufacturer() const { return m_manufacturer; }

    // Numeric USB vendor/product ID (0 when not USB).
    quint16 vendorId()  const { return m_vendorId; }
    quint16 productId() const { return m_productId; }

    // True when the port appears to be a USB serial adapter.
    bool isUsb() const { return m_isUsb; }

    // Enumerate all available serial ports on this system.
    static QList<QTermSerialPortInfo> availablePorts();

private:
    QString m_portName;
    QString m_description;
    QString m_manufacturer;
    quint16 m_vendorId  = 0;
    quint16 m_productId = 0;
    bool    m_isUsb     = false;
};

} // namespace QTerm

#endif // QTERM_QTERMSERIALPORTINFO_H
