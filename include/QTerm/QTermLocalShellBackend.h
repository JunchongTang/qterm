#ifndef QTERM_QTERMLOCALSHELLBACKEND_H
#define QTERM_QTERMLOCALSHELLBACKEND_H

#include <QProcessEnvironment>
#include <QVariantMap>
#include <QString>
#include <QStringList>

#include <QtQml/qqmlregistration.h>

#include <QTerm/QTermSessionBackend.h>

class QSocketNotifier;
class QTimer;

namespace QTerm {

/*!
    \class QTermLocalShellBackend
    \inmodule QTerm
    \brief Launches a local shell process and forwards its I/O through a terminal session.

    The backend uses platform-specific PTY or ConPTY support depending on the
    current operating system.
*/
class QTermLocalShellBackend : public QTermSessionBackend
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString program READ program WRITE setProgram NOTIFY programChanged)
    Q_PROPERTY(QStringList arguments READ arguments WRITE setArguments NOTIFY argumentsChanged)
    Q_PROPERTY(QString workingDirectory READ workingDirectory WRITE setWorkingDirectory NOTIFY workingDirectoryChanged)
    /*!
        \brief Extra environment variables, merged on top of the inherited environment.

        \c processEnvironment replaces the whole environment, which QML cannot build
        (there is no \c QProcessEnvironment value type). This property takes a plain
        map and merges it over whatever the child would otherwise inherit, so callers
        can add a couple of variables without having to reproduce the rest.
    */
    Q_PROPERTY(QVariantMap extraEnvironment READ extraEnvironment WRITE setExtraEnvironment NOTIFY extraEnvironmentChanged)

public:
    explicit QTermLocalShellBackend(QObject *parent = nullptr);
    ~QTermLocalShellBackend() override;

    /*!
        \brief Returns the configured shell program path.
        \return The executable path used when launching the shell.
    */
    QString program() const;

    /*!
        \brief Returns the argument list passed to the shell process.
    */
    QStringList arguments() const;

    /*!
        \brief Returns the working directory used for launching the shell.
    */
    QString workingDirectory() const;

    /*!
        \brief Returns the environment variables applied to the child process.
    */
    QProcessEnvironment processEnvironment() const;

    /*!
        \brief Sets the shell program to launch.
        \param program The executable path or command name.
    */
    void setProgram(const QString &program);

    /*!
        \brief Sets the arguments passed to the shell process.
        \param arguments The argument list.
    */
    void setArguments(const QStringList &arguments);

    /*!
        \brief Sets the working directory used by the shell process.
        \param workingDirectory The directory to start in.

        **An empty string means "do not change directory"**: the shell inherits this
        process's working directory, like any other forked child (the same as
        \c QProcess with no working directory set).

        The library deliberately does not pick one for you. What a *user* expects a
        new terminal to open in -- their home directory, a project folder, wherever
        the last one was -- is the host's policy, not the terminal's: this class
        only knows how to start a process, and "the host's own cwd" is the one
        answer that needs no guessing. A host that wants "home unless the user chose
        something else" resolves that itself before setting this.

        **A path that cannot be entered kills the session.** The child \c chdir()s
        before exec and, on failure, prints the reason on the pty and exits with
        status 127. A host that takes this path from a text field should check it
        first and report it itself -- by the time the child exits, the session the
        user was looking at is already gone.
    */
    void setWorkingDirectory(const QString &workingDirectory);

    /*!
        \brief Sets the process environment for the shell.
        \param environment The environment variables to apply.
    */
    void setProcessEnvironment(const QProcessEnvironment &environment);

    QVariantMap extraEnvironment() const;
    /*!
        \brief Sets extra variables merged over the inherited environment.
        \param environment Key/value pairs; values are converted with \c toString().
    */
    void setExtraEnvironment(const QVariantMap &environment);

    /*!
        \brief Starts the local shell process.
    */
    void open() override;

    /*!
        \brief Stops the local shell process and closes the PTY or ConPTY connection.
    */
    void close() override;

    /*!
        \brief Sends input data to the child shell process.
        \param data The bytes to write to the shell's stdin.
    */
    void writeData(const QByteArray &data) override;

    /*!
        \brief Resizes the pseudo-terminal.
        \param columns The new number of columns.
        \param rows The new number of rows.
    */
    void resize(int columns, int rows) override;

    // Precise local-shell foreground detection (computed on demand, no timer):
    //   - Unix:    compares tcgetpgrp(masterFd) against the child shell's pgid
    //              (== childPid).
    //   - Windows: probes for child processes of the shell via Toolhelp
    //              (coarse: any child => Busy).
    WorkState workState() const override;
    QString foregroundProcessName() const override;

    /*!
        \brief Exit code of the shell, or -1 while it is still running.

        Set when the child is reaped. Kept as a property rather than only in the
        \c errorOccurred message because a host that wants to show it (a tooltip,
        a status line) should not have to parse prose — and a clean exit produces
        no error at all, yet 0 is still information worth showing.

        A shell killed by a signal reports 128 + signal number, the convention
        shells themselves use for \c $?.
    */
    Q_INVOKABLE int exitCode() const { return m_exitCode; }

signals:
    void programChanged();
    void argumentsChanged();
    void workingDirectoryChanged();
    void extraEnvironmentChanged();

private:
    // resolvedProgram() is common: returns configured program, or the platform
    // default shell ($SHELL on Unix, %ComSpec% on Windows).
    QString resolvedProgram() const;

    // ── Common data ───────────────────────────────────────────────────────────
    QString m_program;
    QStringList m_arguments;
    QString m_workingDirectory;
    QProcessEnvironment m_environment;
    QVariantMap m_extraEnvironment;
    int m_columns = 80;
    int m_rows = 24;

    // ── Platform-specific members ─────────────────────────────────────────────
#if defined(Q_OS_WIN)
    // Windows ConPTY: a dedicated read thread performs blocking ReadFile on the
    // output pipe; handles stored as void* to keep <windows.h> out of this header.
    class ReadThread;

    void doClose();
    void onReadThreadDataReceived(const QByteArray &data);
    void onReadThreadPipeEnded();
    void pollProcessExit();
    QString buildCommandLine() const;

    ReadThread *m_readThread = nullptr;
    QTimer *m_processExitTimer = nullptr;
    void *m_hPC = nullptr;       // HPCON
    void *m_hProcess = nullptr;  // HANDLE — child process
    void *m_hThread = nullptr;   // HANDLE — child main thread
    void *m_hPipeIn = nullptr;   // HANDLE — write end of stdin pipe
    void *m_hPipeOut = nullptr;  // HANDLE — read end of stdout pipe
#else
    // Unix PTY: QSocketNotifier provides async reads from the master fd without
    // a separate thread.
    void applyPendingResize();
    void handleReadable();
    void pollChildExit();
    void closeMasterFd();
    void stopRuntimeWatchers();
    QStringList resolvedArguments() const;
    QProcessEnvironment resolvedEnvironment() const;
    void applyEnvironmentOverrides() const;

    int m_masterFd = -1;
    qint64 m_childPid = -1;
    int m_exitCode = -1;
    QSocketNotifier *m_readNotifier = nullptr;
    QTimer *m_resizeDebounceTimer = nullptr;
    QTimer *m_childExitPollTimer = nullptr;
#endif
};

} // namespace QTerm

#endif // QTERM_QTERMLOCALSHELLBACKEND_H
