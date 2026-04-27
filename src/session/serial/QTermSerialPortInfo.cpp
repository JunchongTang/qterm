#include <QTerm/QTermSerialPortInfo.h>

#include <QSerialPortInfo>

namespace QTerm {

QTermSerialPortInfo::QTermSerialPortInfo(const QSerialPortInfo &info)
    : m_portName(info.portName())
    , m_description(info.description())
    , m_manufacturer(info.manufacturer())
    , m_vendorId(info.hasVendorIdentifier() ? info.vendorIdentifier() : 0)
    , m_productId(info.hasProductIdentifier() ? info.productIdentifier() : 0)
    , m_isUsb(info.hasVendorIdentifier())
{
}

QList<QTermSerialPortInfo> QTermSerialPortInfo::availablePorts()
{
    const QList<QSerialPortInfo> qtPorts = QSerialPortInfo::availablePorts();
    QList<QTermSerialPortInfo> result;
    result.reserve(qtPorts.size());
    for (const QSerialPortInfo &p : qtPorts)
        result.append(QTermSerialPortInfo(p));
    return result;
}

} // namespace QTerm
