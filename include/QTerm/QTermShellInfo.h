#ifndef QTERM_QTERMSHELLINFO_H
#define QTERM_QTERMSHELLINFO_H

#include <QString>

#include <QtQml/qqmlregistration.h>

namespace QTerm {

// Describes one discovered local shell.
//
// Registered as a QML value type ("shellInfo") so QML code can read
// properties directly from items returned by QTermLocalShellScanner::availableShells().
//
// On Unix:    program is the full path (e.g. "/bin/zsh"), name is the basename.
// On Windows: program is the resolved full path (e.g. "C:\Windows\System32\cmd.exe"),
//             name is a human-readable label ("PowerShell 7", "Command Prompt", …).
class QTermShellInfo
{
    Q_GADGET
    QML_VALUE_TYPE(shellInfo)

    Q_PROPERTY(QString name    READ name    CONSTANT)
    Q_PROPERTY(QString program READ program CONSTANT)

public:
    QTermShellInfo() = default;
    QTermShellInfo(const QString &name, const QString &program)
        : m_name(name), m_program(program) {}

    // Human-readable display name, e.g. "zsh", "PowerShell 7", "Command Prompt".
    QString name() const { return m_name; }

    // Executable path passed to QTermLocalShellBackend::setProgram(),
    // e.g. "/bin/zsh" or "C:\Program Files\PowerShell\7\pwsh.exe".
    QString program() const { return m_program; }

private:
    QString m_name;
    QString m_program;
};

} // namespace QTerm

#endif // QTERM_QTERMSHELLINFO_H
