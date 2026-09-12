// Unix/macOS implementation of QTermLocalShellBackend using forkpty(3).
// This file is only compiled on non-Windows platforms (see src/CMakeLists.txt).

#include <QTerm/QTermLocalShellBackend.h>

#include <QCoreApplication>
#include <QFile>
#include <QSocketNotifier>
#include <QVarLengthArray>
#include <QTimer>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(Q_OS_MACOS) || defined(Q_OS_FREEBSD)
#include <util.h>
#else
#include <pty.h>
#endif

#if defined(Q_OS_MACOS)
#include <libproc.h>     // proc_name / proc_listchildpids — foreground process name
#include <sys/param.h>   // MAXCOMLEN
#else
#include <QDir>          // /proc scan — see childOf()
#endif

namespace {

constexpr int kExitPollIntervalMs = 50;
constexpr int kResizeDebounceIntervalMs = 60;

QString childExitMessage(int status)
{
    if (WIFSIGNALED(status))
        return QStringLiteral("PTY child exited on signal %1.").arg(WTERMSIG(status));
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
        return QStringLiteral("PTY child exited with code %1.").arg(WEXITSTATUS(status));
    return QString();
}

} // namespace

namespace QTerm {

QTermLocalShellBackend::QTermLocalShellBackend(QObject *parent)
    : QTermSessionBackend(parent)
    , m_readNotifier(new QSocketNotifier(QSocketNotifier::Read, this))
    , m_resizeDebounceTimer(new QTimer(this))
    , m_childExitPollTimer(new QTimer(this))
{
    m_readNotifier->setEnabled(false);
    connect(m_readNotifier, &QSocketNotifier::activated, this, [this]() {
        handleReadable();
    });

    m_resizeDebounceTimer->setSingleShot(true);
    m_resizeDebounceTimer->setInterval(kResizeDebounceIntervalMs);
    connect(m_resizeDebounceTimer, &QTimer::timeout, this, [this]() {
        applyPendingResize();
    });

    m_childExitPollTimer->setInterval(kExitPollIntervalMs);
    connect(m_childExitPollTimer, &QTimer::timeout, this, [this]() {
        pollChildExit();
    });
}

QTermLocalShellBackend::~QTermLocalShellBackend()
{
    close();
}

QString QTermLocalShellBackend::program() const { return m_program; }
QStringList QTermLocalShellBackend::arguments() const { return m_arguments; }
QString QTermLocalShellBackend::workingDirectory() const { return m_workingDirectory; }
QProcessEnvironment QTermLocalShellBackend::processEnvironment() const { return m_environment; }

void QTermLocalShellBackend::setProgram(const QString &program)
{
    m_program = program;
    emit programChanged();
}

void QTermLocalShellBackend::setArguments(const QStringList &arguments)
{
    m_arguments = arguments;
    emit argumentsChanged();
}

void QTermLocalShellBackend::setWorkingDirectory(const QString &workingDirectory)
{
    m_workingDirectory = workingDirectory;
    emit workingDirectoryChanged();
}

void QTermLocalShellBackend::setProcessEnvironment(const QProcessEnvironment &environment)
{
    m_environment = environment;
}

void QTermLocalShellBackend::open()
{
    if (state() == Open || state() == Opening)
        return;

    close();
    // A reopen must not keep reporting the previous run's exit code.
    m_exitCode = -1;

    const QString executable = resolvedProgram();
    if (executable.isEmpty()) {
        emitErrorOccurred(SpawnFailed, QStringLiteral("No PTY program was configured."));
        return;
    }

    const QByteArray executableUtf8 = QFile::encodeName(executable);
    if (::access(executableUtf8.constData(), X_OK) != 0) {
        emitErrorOccurred(SpawnFailed, QStringLiteral("PTY program is not executable: %1").arg(executable));
        return;
    }

    struct winsize winsizeData = {};
    winsizeData.ws_col = static_cast<unsigned short>(m_columns);
    winsizeData.ws_row = static_cast<unsigned short>(m_rows);

    setState(Opening);

    int masterFd = -1;
    const pid_t childPid = ::forkpty(&masterFd, nullptr, nullptr, &winsizeData);
    if (childPid < 0) {
        emitErrorOccurred(SpawnFailed, QStringLiteral("forkpty failed: %1").arg(QString::fromLocal8Bit(std::strerror(errno))));
        return;
    }

    if (childPid == 0) {
        if (!m_workingDirectory.isEmpty()) {
            const QByteArray workingDirectoryUtf8 = QFile::encodeName(m_workingDirectory);
            if (::chdir(workingDirectoryUtf8.constData()) != 0) {
                ::perror("chdir");
                ::_exit(127);
            }
        }

        applyEnvironmentOverrides();

        const QStringList arguments = resolvedArguments();
        QVector<QByteArray> argvStorage;
        argvStorage.reserve(arguments.size() + 1);
        argvStorage.append(executableUtf8);
        for (const QString &argument : arguments)
            argvStorage.append(argument.toLocal8Bit());

        QVector<char *> argv;
        argv.reserve(argvStorage.size() + 1);
        for (QByteArray &entry : argvStorage)
            argv.append(entry.data());
        argv.append(nullptr);

        ::execvp(executableUtf8.constData(), argv.data());
        ::perror("execvp");
        ::_exit(127);
    }

    m_masterFd = masterFd;
    m_childPid = childPid;
    const int flags = ::fcntl(m_masterFd, F_GETFL, 0);
    if (flags >= 0)
        ::fcntl(m_masterFd, F_SETFL, flags | O_NONBLOCK);

    m_readNotifier->setSocket(m_masterFd);
    m_readNotifier->setEnabled(true);
    m_childExitPollTimer->start();
    setState(Open);
}

void QTermLocalShellBackend::close()
{
    if (m_masterFd < 0 && m_childPid < 0) {
        if (state() != Closed)
            setState(Closed);
        return;
    }

    if (state() != Closed)
        setState(Closing);

    if (m_childPid > 0)
        ::kill(static_cast<pid_t>(m_childPid), SIGHUP);

    closeMasterFd();
    pollChildExit();

    if (m_childPid < 0) {
        setState(Closed);
        return;
    }

    stopRuntimeWatchers();
    setState(Closed);
}

void QTermLocalShellBackend::writeData(const QByteArray &data)
{
    if (m_masterFd < 0 || data.isEmpty())
        return;

    const char *cursor = data.constData();
    qsizetype remaining = data.size();
    while (remaining > 0) {
        const ssize_t written = ::write(m_masterFd, cursor, static_cast<size_t>(remaining));
        if (written > 0) {
            cursor += written;
            remaining -= written;
            continue;
        }

        if (written < 0 && errno == EINTR)
            continue;

        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;

        emitErrorOccurred(ConnectionLost, QStringLiteral("PTY write failed: %1").arg(QString::fromLocal8Bit(std::strerror(errno))));
        close();
        return;
    }
}

void QTermLocalShellBackend::resize(int columns, int rows)
{
    m_columns = qMax(1, columns);
    m_rows = qMax(1, rows);
    applyPendingResize();
}

// ── Private helpers ───────────────────────────────────────────────────────────

void QTermLocalShellBackend::applyPendingResize()
{
    if (m_masterFd < 0)
        return;
    struct winsize winsizeData = {};
    winsizeData.ws_col = static_cast<unsigned short>(m_columns);
    winsizeData.ws_row = static_cast<unsigned short>(m_rows);
    ::ioctl(m_masterFd, TIOCSWINSZ, &winsizeData);
}

void QTermLocalShellBackend::handleReadable()
{
    if (m_masterFd < 0)
        return;

    QByteArray chunk(65536, Qt::Uninitialized);
    QByteArray batch;

    for (;;) {
        const ssize_t bytesRead = ::read(m_masterFd, chunk.data(), static_cast<size_t>(chunk.size()));
        if (bytesRead > 0) {
            batch.append(chunk.constData(), static_cast<qsizetype>(bytesRead));
            continue;
        }

        if (bytesRead == 0) {
            if (!batch.isEmpty())
                emitDataReceived(batch);
            closeMasterFd();
            pollChildExit();
            return;
        }

        if (errno == EINTR)
            continue;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;

        if (errno == EIO) {
            if (!batch.isEmpty())
                emitDataReceived(batch);
            closeMasterFd();
            pollChildExit();
            return;
        }

        if (!batch.isEmpty())
            emitDataReceived(batch);
        emitErrorOccurred(ConnectionLost, QStringLiteral("PTY read failed: %1").arg(QString::fromLocal8Bit(std::strerror(errno))));
        close();
        return;
    }

    if (!batch.isEmpty())
        emitDataReceived(batch);
}

void QTermLocalShellBackend::pollChildExit()
{
    if (m_childPid < 0) {
        stopRuntimeWatchers();
        return;
    }

    int status = 0;
    const pid_t waitResult = ::waitpid(static_cast<pid_t>(m_childPid), &status, WNOHANG);
    if (waitResult == 0)
        return;

    stopRuntimeWatchers();
    m_childPid = -1;

    if (waitResult > 0) {
        // 128 + signal is the convention shells use for $? when a child is
        // killed, so a host can print one number either way.
        m_exitCode = WIFEXITED(status)   ? WEXITSTATUS(status)
                   : WIFSIGNALED(status) ? 128 + WTERMSIG(status)
                                         : -1;
    }

    if (waitResult < 0) {
        setState(Closed);
        return;
    }

    const QString exitMessage = childExitMessage(status);
    if (!exitMessage.isEmpty()) {
        emitErrorOccurred(ConnectionLost, exitMessage);
        return;
    }

    setState(Closed);
}

void QTermLocalShellBackend::closeMasterFd()
{
    if (m_masterFd < 0)
        return;
    m_readNotifier->setEnabled(false);
    m_readNotifier->setSocket(-1);
    ::close(m_masterFd);
    m_masterFd = -1;
}

void QTermLocalShellBackend::stopRuntimeWatchers()
{
    m_readNotifier->setEnabled(false);
    m_resizeDebounceTimer->stop();
    m_childExitPollTimer->stop();
}

QString QTermLocalShellBackend::resolvedProgram() const
{
    if (!m_program.isEmpty())
        return m_program;
    const QString shell = qEnvironmentVariable("SHELL");
    return shell.isEmpty() ? QStringLiteral("/bin/sh") : shell;
}

QStringList QTermLocalShellBackend::resolvedArguments() const
{
    if (!m_arguments.isEmpty())
        return m_arguments;
    if (m_program.isEmpty())
        return {QStringLiteral("-i")};
    return {};
}

QProcessEnvironment QTermLocalShellBackend::resolvedEnvironment() const
{
    QProcessEnvironment environment = m_environment;
    if (environment.isEmpty())
        environment = QProcessEnvironment::systemEnvironment();
    if (!environment.contains(QStringLiteral("TERM")))
        environment.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    return environment;
}

void QTermLocalShellBackend::applyEnvironmentOverrides() const
{
    const QProcessEnvironment environment = resolvedEnvironment();
    const QStringList keys = environment.keys();
    for (const QString &key : keys) {
        const QByteArray keyUtf8 = key.toLocal8Bit();
        const QByteArray valueUtf8 = environment.value(key).toLocal8Bit();
        ::setenv(keyUtf8.constData(), valueUtf8.constData(), 1);
    }
}

// ── Foreground process detection (Unix) ─────────────────────────────────────
// A PTY's foreground process group is kernel-maintained: tcgetpgrp returns the
// group id that currently owns the keyboard / Ctrl-C. The child shell from
// forkpty becomes a session/group leader via setsid, so its pgid == m_childPid.
// When idle (at a prompt) the foreground group is the shell itself (== childPid);
// while running a command the shell sets that command's group as foreground
// (!= childPid). So a single tcgetpgrp call decides it.

QTermSessionBackend::WorkState QTermLocalShellBackend::workState() const
{
    if (m_masterFd < 0 || m_childPid <= 0)
        return WorkUnknown;
    const pid_t fg = ::tcgetpgrp(m_masterFd);
    if (fg < 0)
        return WorkUnknown;
    return (fg == static_cast<pid_t>(m_childPid)) ? WorkIdle : WorkBusy;
}

namespace {

// Name of a pid, or an empty string when it cannot be read.
QString processName(pid_t pid)
{
    if (pid <= 0)
        return {};
#if defined(Q_OS_MACOS)
    char name[2 * MAXCOMLEN + 1] = {0};
    if (::proc_name(pid, name, sizeof(name)) > 0)
        return QString::fromLocal8Bit(name);
    return {};
#else // Linux and other platforms exposing /proc
    QFile comm(QStringLiteral("/proc/%1/comm").arg(pid));
    if (comm.open(QIODevice::ReadOnly))
        return QString::fromLocal8Bit(comm.readAll()).trimmed();
    return {};
#endif
}

// First child of `pid`, or 0. Used to look past wrappers — see below.
pid_t childOf(pid_t pid)
{
    if (pid <= 0)
        return 0;
#if defined(Q_OS_MACOS)
    // Ask for the size first: a wrapper normally has exactly one child, but a
    // fixed-size buffer would silently truncate on the odd case that it does not.
    const int bytes = ::proc_listchildpids(pid, nullptr, 0);
    if (bytes <= 0)
        return 0;
    QVarLengthArray<pid_t, 8> kids(bytes / static_cast<int>(sizeof(pid_t)));
    const int got = ::proc_listchildpids(pid, kids.data(), bytes);
    if (got <= 0)
        return 0;
    return kids.at(0);
#else
    // No cheap "list my children" call; scan /proc for a matching PPid.
    const QStringList entries =
        QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        bool isPid = false;
        const int candidate = entry.toInt(&isPid);
        if (!isPid || candidate <= 0)
            continue;
        QFile stat(QStringLiteral("/proc/%1/stat").arg(candidate));
        if (!stat.open(QIODevice::ReadOnly))
            continue;
        // "<pid> (<comm>) <state> <ppid> ..." — comm may contain spaces and
        // parentheses, so split after the last ')'.
        const QByteArray line = stat.readAll();
        const int close = line.lastIndexOf(')');
        if (close < 0)
            continue;
        const QList<QByteArray> rest =
            line.mid(close + 1).simplified().split(' ');
        if (rest.size() < 2)
            continue;
        if (rest.at(1).toInt() == pid)
            return candidate;
    }
    return 0;
#endif
}

// Wrappers that run the interesting command as a child. Without looking past
// them a screenful of tabs all read "sudo".
bool isWrapper(const QString &name)
{
    static const QStringList wrappers = {
        QStringLiteral("sudo"),   QStringLiteral("doas"),  QStringLiteral("env"),
        QStringLiteral("nice"),   QStringLiteral("nohup"), QStringLiteral("stdbuf"),
        QStringLiteral("time"),   QStringLiteral("timeout"),
    };
    return wrappers.contains(name);
}

} // namespace

QString QTermLocalShellBackend::foregroundProcessName() const
{
    if (m_masterFd < 0 || m_childPid <= 0)
        return {};
    const pid_t fg = ::tcgetpgrp(m_masterFd);
    if (fg <= 0 || fg == static_cast<pid_t>(m_childPid))
        return {};   // idle: foreground is the shell itself, no running program

    // fg is a process-group id == the leader's (i.e. the command's) pid.
    //
    // Walk past wrappers: `sudo make` puts sudo in the foreground and make below
    // it, so naming the group leader would label every such tab "sudo". The loop
    // is bounded because `sudo -E env FOO=1 make` nests more than once, and a
    // bound (rather than a single step) keeps a pathological chain from spinning.
    pid_t pid = fg;
    QString name = processName(pid);
    for (int depth = 0; depth < 4 && isWrapper(name); ++depth) {
        const pid_t kid = childOf(pid);
        if (kid <= 0)
            break;                  // wrapper with no child yet — keep its name
        const QString kidName = processName(kid);
        if (kidName.isEmpty())
            break;
        pid = kid;
        name = kidName;
    }
    return name;
}

} // namespace QTerm
