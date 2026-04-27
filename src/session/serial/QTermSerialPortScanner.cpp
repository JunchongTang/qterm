#include <QTerm/QTermSerialPortScanner.h>

namespace QTerm {

QTermSerialPortScanner::QTermSerialPortScanner(QObject *parent)
    : QObject(parent)
{
}

QList<QTermSerialPortInfo> QTermSerialPortScanner::availablePorts() const
{
    return QTermSerialPortInfo::availablePorts();
}

} // namespace QTerm
