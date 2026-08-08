// Unix/macOS implementation of QTermLocalShellBackend using forkpty(3).
// This file is only compiled on non-Windows platforms (see src/CMakeLists.txt).

#include <QTerm/QTermLocalShellBackend.h>

#include <QCoreApplication>
#include <QFile>
#include <QTimer>

#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <mutex>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#if defined(Q_OS_MACOS) || defined(Q_OS_FREEBSD)
#include <util.h>
#else
#include <pty.h>
#endif

#if defined(Q_OS_MACOS)
#include <libproc.h>     // proc_name — foreground process name
#include <sys/param.h>   // MAXCOMLEN
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

// Reading is throttled once this much delivered-but-unparsed data is
// outstanding. A pty normally throttles a chatty program by blocking its
// writes when the buffer fills; draining the master as fast as possible
// removes that brake, so without a limit here an endless producer such as
// yes(1) would queue without bound.
constexpr qsizetype kMaximumPendingBytes = 4 * 1024 * 1024;

constexpr qsizetype kReadChunkSize = 65536;

} // namespace

namespace QTerm {

/*
    Drains a pty master on a dedicated thread.

    A QSocketNotifier is simpler and was what this backend used originally, but
    on macOS it wakes the GUI thread through CFSocket. Sampling a 1M-line stream
    showed that wake-up dominating: the GUI thread spent 258 of 516 samples in
    __CFSocketPerformV0 and another 123 waiting in mach_msg, leaving only about
    half its time for actual parsing. Reading with a blocking poll() here and
    posting the bytes across took the same stream from 0.441 s to 0.116 s.

    A self-pipe rather than closing the fd is used to unblock poll() on stop,
    so there is no window in which a recycled descriptor could be read.
*/
class QTermPtyReader
{
public:
    using DataHandler = std::function<void(const QByteArray &)>;
    using EndHandler = std::function<void(int)>; // errno, or 0 for a clean EOF

    ~QTermPtyReader() { stop(); }

    // Handlers run on the thread owning `context`.
    void start(int masterFd, QObject *context, DataHandler onData, EndHandler onEnd)
    {
        stop();
        if (::pipe(m_wakePipe) != 0) {
            m_wakePipe[0] = m_wakePipe[1] = -1;
            return;
        }
        m_stopping = false;
        m_pending = 0;
        m_thread = std::thread([this, masterFd, context,
                                onData = std::move(onData), onEnd = std::move(onEnd)] {
            run(masterFd, context, onData, onEnd);
        });
    }

    void stop()
    {
        if (!m_thread.joinable())
            return;
        {
            const std::lock_guard<std::mutex> guard(m_mutex);
            m_stopping = true;
        }
        m_space.notify_all();
        if (m_wakePipe[1] >= 0) {
            const char byte = 0;
            while (::write(m_wakePipe[1], &byte, 1) < 0 && errno == EINTR) { }
        }
        m_thread.join();
        closeWakePipe();
    }

    // Called on the consumer thread once a delivered chunk has been parsed.
    void acknowledge(qsizetype bytes)
    {
        {
            const std::lock_guard<std::mutex> guard(m_mutex);
            m_pending -= bytes;
        }
        m_space.notify_all();
    }

private:
    void closeWakePipe()
    {
        for (int &fd : m_wakePipe) {
            if (fd >= 0) {
                ::close(fd);
                fd = -1;
            }
        }
    }

    // Blocks while the consumer is behind. Returns false if asked to stop.
    bool awaitCapacity()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_space.wait(lock, [this] {
            return m_stopping || m_pending < kMaximumPendingBytes;
        });
        return !m_stopping;
    }

    void run(int masterFd, QObject *context,
             const DataHandler &onData, const EndHandler &onEnd)
    {
        QByteArray buffer(kReadChunkSize, Qt::Uninitialized);
        int endReason = 0;

        for (;;) {
            if (!awaitCapacity())
                return; // stopping: the consumer is going away, stay silent

            struct pollfd fds[2] = {};
            fds[0].fd = masterFd;
            fds[0].events = POLLIN;
            fds[1].fd = m_wakePipe[0];
            fds[1].events = POLLIN;

            const int ready = ::poll(fds, 2, -1);
            if (ready < 0) {
                if (errno == EINTR)
                    continue;
                endReason = errno;
                break;
            }
            if (fds[1].revents != 0)
                return; // asked to stop

            bool ended = false;
            for (;;) {
                const ssize_t bytesRead =
                    ::read(masterFd, buffer.data(), static_cast<size_t>(buffer.size()));
                if (bytesRead > 0) {
                    QByteArray chunk(buffer.constData(), static_cast<qsizetype>(bytesRead));
                    {
                        const std::lock_guard<std::mutex> guard(m_mutex);
                        m_pending += chunk.size();
                    }
                    const qsizetype delivered = chunk.size();
                    QMetaObject::invokeMethod(context,
                        [this, onData, chunk = std::move(chunk), delivered] {
                            onData(chunk);
                            acknowledge(delivered);
                        }, Qt::QueuedConnection);
                    continue;
                }
                if (bytesRead == 0) {
                    ended = true; // EOF
                    break;
                }
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break; // drained; back to poll()
                // EIO is how a pty reports that the child closed its side.
                ended = true;
                endReason = (errno == EIO) ? 0 : errno;
                break;
            }
            if (ended)
                break;

            // POLLHUP on its own (no data left) also means the child is gone.
            if ((fds[0].revents & (POLLHUP | POLLERR)) != 0
                && (fds[0].revents & POLLIN) == 0) {
                break;
            }
        }

        // Queued behind every chunk already posted, so the consumer sees all
        // the data before it sees the end.
        QMetaObject::invokeMethod(context, [onEnd, endReason] { onEnd(endReason); },
                                  Qt::QueuedConnection);
    }

    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_space;
    qsizetype m_pending = 0;
    bool m_stopping = false;
    int m_wakePipe[2] = {-1, -1};
};

QTermLocalShellBackend::QTermLocalShellBackend(QObject *parent)
    : QTermSessionBackend(parent)
    , m_reader(std::make_unique<QTermPtyReader>())
    , m_resizeDebounceTimer(new QTimer(this))
    , m_childExitPollTimer(new QTimer(this))
{
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

    m_childReaped = false;
    m_readEnded = false;
    m_childExitMessage.clear();

    m_reader->start(m_masterFd, this,
                    [this](const QByteArray &data) { handleReadData(data); },
                    [this](int error) { handleReadEnded(error); });
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

    // An explicit close is a decision to stop now, so unlike an exiting child
    // this does not wait for buffered output to drain.
    m_readEnded = true;

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

void QTermLocalShellBackend::handleReadData(const QByteArray &data)
{
    if (data.isEmpty())
        return;

    emitDataReceived(data);
}

// Posted by the reader thread after every chunk it read, so all output has
// already been delivered by the time this runs.
void QTermLocalShellBackend::handleReadEnded(int error)
{
    if (m_readEnded)
        return;
    m_readEnded = true;

    if (error != 0) {
        emitErrorOccurred(ConnectionLost,
                          QStringLiteral("PTY read failed: %1")
                              .arg(QString::fromLocal8Bit(std::strerror(error))));
        close();
        return;
    }

    // Clean EOF, or EIO which is how a pty reports the child closing its side.
    closeMasterFd();
    finishIfDrained();
}

void QTermLocalShellBackend::finishIfDrained()
{
    if (!m_childReaped || !m_readEnded)
        return;

    stopRuntimeWatchers();

    if (!m_childExitMessage.isEmpty()) {
        const QString message = m_childExitMessage;
        m_childExitMessage.clear();
        emitErrorOccurred(ConnectionLost, message);
        return;
    }

    setState(Closed);
}

void QTermLocalShellBackend::pollChildExit()
{
    if (m_childPid < 0) {
        m_childReaped = true;
        m_childExitPollTimer->stop();
        finishIfDrained();
        return;
    }

    int status = 0;
    const pid_t waitResult = ::waitpid(static_cast<pid_t>(m_childPid), &status, WNOHANG);
    if (waitResult == 0)
        return;

    // The child is gone, but output it wrote before exiting may still be in the
    // pty buffer. Stop polling and leave the reader running: the session is not
    // finished until that output has been read, or it would be lost.
    m_childExitPollTimer->stop();
    m_childPid = -1;
    m_childReaped = true;
    if (waitResult > 0)
        m_childExitMessage = childExitMessage(status);

    finishIfDrained();
}

void QTermLocalShellBackend::closeMasterFd()
{
    if (m_masterFd < 0)
        return;
    // The reader thread is using this descriptor, so it has to be joined before
    // the descriptor is closed and possibly recycled.
    m_reader->stop();
    ::close(m_masterFd);
    m_masterFd = -1;
}

void QTermLocalShellBackend::stopRuntimeWatchers()
{
    m_reader->stop();
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

QString QTermLocalShellBackend::foregroundProcessName() const
{
    if (m_masterFd < 0 || m_childPid <= 0)
        return {};
    const pid_t fg = ::tcgetpgrp(m_masterFd);
    if (fg <= 0 || fg == static_cast<pid_t>(m_childPid))
        return {};   // idle: foreground is the shell itself, no running program
    // fg is a process-group id == the leader's (i.e. the command's) pid; name it.
#if defined(Q_OS_MACOS)
    char name[2 * MAXCOMLEN + 1] = {0};
    if (::proc_name(fg, name, sizeof(name)) > 0)
        return QString::fromLocal8Bit(name);
    return {};
#else // Linux / other platforms exposing /proc
    QFile comm(QStringLiteral("/proc/%1/comm").arg(fg));
    if (comm.open(QIODevice::ReadOnly))
        return QString::fromLocal8Bit(comm.readAll()).trimmed();
    return {};
#endif
}

} // namespace QTerm
