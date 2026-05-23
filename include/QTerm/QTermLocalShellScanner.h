#ifndef QTERM_QTERMLOCALSHELLSCANNER_H
#define QTERM_QTERMLOCALSHELLSCANNER_H

#include <QList>
#include <QObject>

#include <QtQml/qqmlregistration.h>

#include <QTerm/QTermShellInfo.h>

namespace QTerm {

// QML singleton that enumerates available local shells at runtime.
//
// On Unix:    reads /etc/shells, filters to executable entries, prepends $SHELL.
// On Windows: searches PATH for pwsh.exe (PowerShell 7), powershell.exe, cmd.exe.
//
// QML usage:
//   import QTerm 1.0
//
//   Component.onCompleted: {
//       var shells = QTermLocalShellScanner.availableShells()
//       for (var i = 0; i < shells.length; i++)
//           console.log(shells[i].name, shells[i].program)
//   }
class QTermLocalShellScanner : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit QTermLocalShellScanner(QObject *parent = nullptr);

    // Returns a snapshot of available shells on this system.
    // The first entry is the preferred default (system shell or best available).
    // Each call re-enumerates; there is no caching.
    Q_INVOKABLE QList<QTerm::QTermShellInfo> availableShells() const;
};

} // namespace QTerm

#endif // QTERM_QTERMLOCALSHELLSCANNER_H
