#ifndef QTERM_QTERMSHELLINFO_H
#define QTERM_QTERMSHELLINFO_H

#include <QString>

#include <QtQml/qqmlregistration.h>

namespace QTerm {

/*!
    \qmltype shellInfo
    \inqmlmodule QTerm
    \qmlvaluetype
    \brief Describes a discovered local shell and the command used to launch it.

    The type is exposed to QML as a value type so scripts can read its
    properties directly from the list returned by QTermLocalShellScanner.
*/
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

    /*!
        \brief Returns the human-readable display name of the shell.
        \return The shell name, for example "zsh", "PowerShell 7" or "Command Prompt".
    */
    QString name() const { return m_name; }

    /*!
        \brief Returns the executable path used to launch the shell.
        \return The resolved program path, for example "/bin/zsh" or "C:\\Program Files\\PowerShell\\7\\pwsh.exe".
    */
    QString program() const { return m_program; }

private:
    QString m_name;
    QString m_program;
};

} // namespace QTerm

#endif // QTERM_QTERMSHELLINFO_H
