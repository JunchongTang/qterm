#ifndef QTERM_QTERMSERIALPORTSCANNER_H
#define QTERM_QTERMSERIALPORTSCANNER_H

#include <QList>
#include <QObject>

#include <QtQml/qqmlregistration.h>

#include <QTerm/QTermSerialPortInfo.h>

namespace QTerm {

// QML singleton that enumerates available serial ports at runtime.
//
// QML usage:
//   import QTerm 1.0
//
//   Component.onCompleted: {
//       var ports = SerialPortScanner.availablePorts()
//       for (var i = 0; i < ports.length; i++) {
//           console.log(ports[i].portName, ports[i].description)
//       }
//   }
class QTermSerialPortScanner : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit QTermSerialPortScanner(QObject *parent = nullptr);

    // Returns a snapshot of all serial ports currently visible to the OS.
    // Each call re-enumerates; there is no caching.
    Q_INVOKABLE QList<QTerm::QTermSerialPortInfo> availablePorts() const;
};

} // namespace QTerm

#endif // QTERM_QTERMSERIALPORTSCANNER_H
