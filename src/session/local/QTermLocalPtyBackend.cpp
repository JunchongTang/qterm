#include <QTerm/QTermLocalPtyBackend.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QSocketNotifier>
#include <QTimer>

#include <cerrno>
#include <cstring>

#if defined(Q_OS_UNIX)
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
#endif

namespace {

constexpr int kExitPollIntervalMs = 50;
constexpr int kResizeDebounceIntervalMs = 60;

QString childExitMessage(int status)
{
    if (WIFSIGNALED(status)) {
        return QStringLiteral("PTY child exited on signal %1.").arg(WTERMSIG(status));
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        return QStringLiteral("PTY child exited with code %1.").arg(WEXITSTATUS(status));
    }

    return QString();
}

} // namespace

namespace QTerm {

QTermLocalPtyBackend::QTermLocalPtyBackend(QObject *parent)
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

QTermLocalPtyBackend::~QTermLocalPtyBackend()
{
    close();
}

QString QTermLocalPtyBackend::program() const
{
    return m_program;
}

QStringList QTermLocalPtyBackend::arguments() const
{
    return m_arguments;
}

QString QTermLocalPtyBackend::workingDirectory() const
{
    return m_workingDirectory;
}

QProcessEnvironment QTermLocalPtyBackend::processEnvironment() const
{
    return m_environment;
}

void QTermLocalPtyBackend::setProgram(const QString &program)
{
    m_program = program;
}

void QTermLocalPtyBackend::setArguments(const QStringList &arguments)
{
    m_arguments = arguments;
}

void QTermLocalPtyBackend::setWorkingDirectory(const QString &workingDirectory)
{
    m_workingDirectory = workingDirectory;
}

void QTermLocalPtyBackend::setProcessEnvironment(const QProcessEnvironment &environment)
{
    m_environment = environment;
}

void QTermLocalPtyBackend::open()
{
    if (state() == Open || state() == Opening) {
        return;
    }

    close();

#if !defined(Q_OS_UNIX)
    emitErrorOccurred(QStringLiteral("QTermLocalPtyBackend requires a Unix PTY platform."));
    return;
#else
    const QString executable = resolvedProgram();
    if (executable.isEmpty()) {
        emitErrorOccurred(QStringLiteral("No PTY program was configured."));
        return;
    }

    const QByteArray executableUtf8 = QFile::encodeName(executable);
    if (::access(executableUtf8.constData(), X_OK) != 0) {
        emitErrorOccurred(QStringLiteral("PTY program is not executable: %1").arg(executable));
        return;
    }

    struct winsize winsizeData = {};
    winsizeData.ws_col = static_cast<unsigned short>(m_columns);
    winsizeData.ws_row = static_cast<unsigned short>(m_rows);

    setState(Opening);

    int masterFd = -1;
    const pid_t childPid = ::forkpty(&masterFd, nullptr, nullptr, &winsizeData);
    if (childPid < 0) {
        emitErrorOccurred(QStringLiteral("forkpty failed: %1").arg(QString::fromLocal8Bit(std::strerror(errno))));
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
        for (const QString &argument : arguments) {
            argvStorage.append(argument.toLocal8Bit());
        }

        QVector<char *> argv;
        argv.reserve(argvStorage.size() + 1);
        for (QByteArray &entry : argvStorage) {
            argv.append(entry.data());
        }
        argv.append(nullptr);

        ::execvp(executableUtf8.constData(), argv.data());
        ::perror("execvp");
        ::_exit(127);
    }

    m_masterFd = masterFd;
    m_childPid = childPid;
    const int flags = ::fcntl(m_masterFd, F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(m_masterFd, F_SETFL, flags | O_NONBLOCK);
    }

    m_readNotifier->setSocket(m_masterFd);
    m_readNotifier->setEnabled(true);
    m_childExitPollTimer->start();
    setState(Open);
#endif
}

void QTermLocalPtyBackend::close()
{
#if !defined(Q_OS_UNIX)
    if (state() != Closed) {
        setState(Closed);
    }
    return;
#else
    if (m_masterFd < 0 && m_childPid < 0) {
        if (state() != Closed) {
            setState(Closed);
        }
        return;
    }

    if (state() != Closed) {
        setState(Closing);
    }

    if (m_childPid > 0) {
        ::kill(static_cast<pid_t>(m_childPid), SIGHUP);
    }

    closeMasterFd();
    pollChildExit();

    if (m_childPid < 0) {
        setState(Closed);
        return;
    }

    stopRuntimeWatchers();
    setState(Closed);
#endif
}

void QTermLocalPtyBackend::writeData(const QByteArray &data)
{
#if !defined(Q_OS_UNIX)
    Q_UNUSED(data);
    return;
#else
    if (m_masterFd < 0 || data.isEmpty()) {
        return;
    }

    const char *cursor = data.constData();
    qsizetype remaining = data.size();
    while (remaining > 0) {
        const ssize_t written = ::write(m_masterFd, cursor, static_cast<size_t>(remaining));
        if (written > 0) {
            cursor += written;
            remaining -= written;
            continue;
        }

        if (written < 0 && errno == EINTR) {
            continue;
        }

        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }

        emitErrorOccurred(QStringLiteral("PTY write failed: %1").arg(QString::fromLocal8Bit(std::strerror(errno))));
        close();
        return;
    }
#endif
}

void QTermLocalPtyBackend::resize(int columns, int rows)
{
    m_columns = qMax(1, columns);
    m_rows = qMax(1, rows);

    // Apply TIOCSWINSZ immediately so the shell always sees the correct size.
    // Debouncing is handled upstream (QTermQuickPaintedItem::geometryChange)
    // which coalesces rapid drag-resize events before calling session.resize(),
    // so SIGWINCH is only delivered once the user stops dragging.
    // Having a second debounce here would mean TIOCSWINSZ fires 60 ms after the
    // window first shows, which is long enough for the user to start a
    // full-screen app like top(1) that queries TIOCGWINSZ at startup and ends
    // up with the stale 80x24 initial PTY size.
    applyPendingResize();
}

void QTermLocalPtyBackend::applyPendingResize()
{
#if defined(Q_OS_UNIX)
    if (m_masterFd < 0) {
        return;
    }

    struct winsize winsizeData = {};
    winsizeData.ws_col = static_cast<unsigned short>(m_columns);
    winsizeData.ws_row = static_cast<unsigned short>(m_rows);
    ::ioctl(m_masterFd, TIOCSWINSZ, &winsizeData);
#endif
}

void QTermLocalPtyBackend::handleReadable()
{
#if defined(Q_OS_UNIX)
    if (m_masterFd < 0) {
        return;
    }

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

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }

        if (errno == EIO) {
            if (!batch.isEmpty())
                emitDataReceived(batch);
            closeMasterFd();
            pollChildExit();
            return;
        }

        if (!batch.isEmpty())
            emitDataReceived(batch);
        emitErrorOccurred(QStringLiteral("PTY read failed: %1").arg(QString::fromLocal8Bit(std::strerror(errno))));
        close();
        return;
    }

    if (!batch.isEmpty())
        emitDataReceived(batch);
#endif
}

void QTermLocalPtyBackend::pollChildExit()
{
#if defined(Q_OS_UNIX)
    if (m_childPid < 0) {
        stopRuntimeWatchers();
        return;
    }

    int status = 0;
    const pid_t waitResult = ::waitpid(static_cast<pid_t>(m_childPid), &status, WNOHANG);
    if (waitResult == 0) {
        return;
    }

    stopRuntimeWatchers();
    m_childPid = -1;

    if (waitResult < 0) {
        setState(Closed);
        return;
    }

    const QString exitMessage = childExitMessage(status);
    if (!exitMessage.isEmpty()) {
        emitErrorOccurred(exitMessage);
        return;
    }

    setState(Closed);
#endif
}

void QTermLocalPtyBackend::closeMasterFd()
{
#if defined(Q_OS_UNIX)
    if (m_masterFd < 0) {
        return;
    }

    m_readNotifier->setEnabled(false);
    m_readNotifier->setSocket(-1);
    ::close(m_masterFd);
    m_masterFd = -1;
#endif
}

void QTermLocalPtyBackend::stopRuntimeWatchers()
{
    m_readNotifier->setEnabled(false);
    m_resizeDebounceTimer->stop();
    m_childExitPollTimer->stop();
}

QString QTermLocalPtyBackend::resolvedProgram() const
{
    if (!m_program.isEmpty()) {
        return m_program;
    }

    const QString shell = qEnvironmentVariable("SHELL");
    return shell.isEmpty() ? QStringLiteral("/bin/sh") : shell;
}

QStringList QTermLocalPtyBackend::resolvedArguments() const
{
    if (!m_arguments.isEmpty()) {
        return m_arguments;
    }

    if (m_program.isEmpty()) {
        return {QStringLiteral("-i")};
    }

    return {};
}

QProcessEnvironment QTermLocalPtyBackend::resolvedEnvironment() const
{
    QProcessEnvironment environment = m_environment;
    if (environment.isEmpty()) {
        environment = QProcessEnvironment::systemEnvironment();
    }

    if (!environment.contains(QStringLiteral("TERM"))) {
        environment.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    }

    return environment;
}

void QTermLocalPtyBackend::applyEnvironmentOverrides() const
{
#if defined(Q_OS_UNIX)
    const QProcessEnvironment environment = resolvedEnvironment();
    const QStringList keys = environment.keys();
    for (const QString &key : keys) {
        const QByteArray keyUtf8 = key.toLocal8Bit();
        const QByteArray valueUtf8 = environment.value(key).toLocal8Bit();
        ::setenv(keyUtf8.constData(), valueUtf8.constData(), 1);
    }
#endif
}

} // namespace QTerm