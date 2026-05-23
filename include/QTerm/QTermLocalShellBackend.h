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

// Cross-platform local shell backend.
// On Unix/macOS: launches a process via forkpty (Unix PTY).
// On Windows:    launches a process via CreatePseudoConsole (ConPTY).
// Windows ConPTY requires Windows 10 1809 (build 17763) or later.
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

    QString program() const;
    QStringList arguments() const;
    QString workingDirectory() const;
    QProcessEnvironment processEnvironment() const;

    void setProgram(const QString &program);
    void setArguments(const QStringList &arguments);
    void setWorkingDirectory(const QString &workingDirectory);
    void setProcessEnvironment(const QProcessEnvironment &environment);

    void open() override;
    void close() override;
    void writeData(const QByteArray &data) override;
    void resize(int columns, int rows) override;

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
