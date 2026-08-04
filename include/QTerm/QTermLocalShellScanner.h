#ifndef QTERM_QTERMLOCALSHELLSCANNER_H
#define QTERM_QTERMLOCALSHELLSCANNER_H

#include <QList>
#include <QObject>

#include <QtQml/qqmlregistration.h>

#include <QTerm/QTermShellInfo.h>

namespace QTerm {

/*!
    \qmltype QTermLocalShellScanner
    \inqmlmodule QTerm
    \qmlsingleton
    \brief Enumerates locally available shell programs at runtime.

    The scanner inspects the current platform and returns a list of shells that
    can be launched by QTermLocalShellBackend.
*/
class QTermLocalShellScanner : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit QTermLocalShellScanner(QObject *parent = nullptr);

    /*!
        \brief Returns a snapshot of shells available on the current system.
        \return A list of discovered shells; the first entry is typically the preferred default.
    */
    Q_INVOKABLE QList<QTerm::QTermShellInfo> availableShells() const;
};

} // namespace QTerm

#endif // QTERM_QTERMLOCALSHELLSCANNER_H
