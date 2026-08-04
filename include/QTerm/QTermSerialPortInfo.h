#ifndef QTERM_QTERMSERIALPORTINFO_H
#define QTERM_QTERMSERIALPORTINFO_H

#include <QList>
#include <QObject>
#include <QString>

#include <QtQml/qqmlregistration.h>

QT_FORWARD_DECLARE_CLASS(QSerialPortInfo)

namespace QTerm {

/*!
    \qmltype serialPortInfo
    \inqmlmodule QTerm
    \qmlvaluetype
    \brief Describes one discovered serial port.

    This value type wraps platform serial-port metadata and is returned by
    QTermSerialPortScanner::availablePorts().
*/
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

    /*! \brief Returns the device node or port name, such as COM3 or /dev/ttyUSB0. */
    QString portName() const { return m_portName; }

    /*! \brief Returns the human-readable device description. */
    QString description() const { return m_description; }

    /*! \brief Returns the USB manufacturer string when available. */
    QString manufacturer() const { return m_manufacturer; }

    /*! \brief Returns the numeric USB vendor identifier, or 0 when not applicable. */
    quint16 vendorId()  const { return m_vendorId; }
    /*! \brief Returns the numeric USB product identifier, or 0 when not applicable. */
    quint16 productId() const { return m_productId; }

    /*! \brief Returns true if the port appears to be a USB serial device. */
    bool isUsb() const { return m_isUsb; }

    /*! \brief Enumerates all serial ports currently visible to the operating system. */
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
