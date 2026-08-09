#ifndef QTERM_QTERMLOCALSHELLBACKEND_H
#define QTERM_QTERMLOCALSHELLBACKEND_H

#include <QProcessEnvironment>
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
    */
    void setWorkingDirectory(const QString &workingDirectory);

    /*!
        \brief Sets the process environment for the shell.
        \param environment The environment variables to apply.
    */
    void setProcessEnvironment(const QProcessEnvironment &environment);

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

signals:
    void programChanged();
    void argumentsChanged();
    void workingDirectoryChanged();

private:
    // resolvedProgram() is common: returns configured program, or the platform
    // default shell ($SHELL on Unix, %ComSpec% on Windows).
    QString resolvedProgram() const;

    // ── Common data ───────────────────────────────────────────────────────────
    QString m_program;
    QStringList m_arguments;
    QString m_workingDirectory;
    QProcessEnvironment m_environment;
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
    QSocketNotifier *m_readNotifier = nullptr;
    QTimer *m_resizeDebounceTimer = nullptr;
    QTimer *m_childExitPollTimer = nullptr;
#endif
};

} // namespace QTerm

#endif // QTERM_QTERMLOCALSHELLBACKEND_H
