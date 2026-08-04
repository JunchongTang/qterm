#ifndef QTERM_QTERMSERIALPORTSCANNER_H
#define QTERM_QTERMSERIALPORTSCANNER_H

#include <QList>
#include <QObject>

#include <QtQml/qqmlregistration.h>

#include <QTerm/QTermSerialPortInfo.h>

namespace QTerm {

/*!
    \qmltype QTermSerialPortScanner
    \inqmlmodule QTerm
    \qmlsingleton
    \brief QML singleton that enumerates serial ports at runtime.
*/
class QTermSerialPortScanner : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit QTermSerialPortScanner(QObject *parent = nullptr);

    /*!
        \brief Returns a snapshot of all serial ports visible to the operating system.
        \return A list of available serial-port descriptors.
    */
    Q_INVOKABLE QList<QTerm::QTermSerialPortInfo> availablePorts() const;
};

} // namespace QTerm

#endif // QTERM_QTERMSERIALPORTSCANNER_H
