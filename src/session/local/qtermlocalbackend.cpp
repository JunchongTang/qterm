#include <QTerm/qtermlocalbackend.h>

#include <cerrno>
#include <cstring>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSocketNotifier>
#include <QTimer>

#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(Q_OS_MACOS) || defined(Q_OS_FREEBSD)
#include <util.h>
#else
#include <pty.h>
#endif

namespace {

QByteArrayList buildArgumentVector(const QString &program, const QStringList &arguments)
{
    QByteArrayList values;
    values.reserve(arguments.size() + 1);
    values.push_back(QFileInfo(program).fileName().toLocal8Bit());

    for (const QString &argument : arguments) {
        values.push_back(argument.toLocal8Bit());
    }

    return values;
}

char **buildArgv(QByteArrayList &values)
{
    auto **argv = new char *[static_cast<std::size_t>(values.size()) + 1];
    for (qsizetype index = 0; index < values.size(); ++index) {
        argv[index] = values[index].data();
    }

    argv[values.size()] = nullptr;
    return argv;
}

int exitCodeFromStatus(int status)
{
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }

    return -1;
}

}

QTermLocalBackend::QTermLocalBackend(QObject *parent)
    : QTermSessionBackend(parent)
    , m_childPollTimer(new QTimer(this))
{
    m_childPollTimer->setInterval(100);
    connect(m_childPollTimer, &QTimer::timeout,
            this, &QTermLocalBackend::pollChildExit);
}

QTermLocalBackend::~QTermLocalBackend()
{
    close();
}

QString QTermLocalBackend::program() const
{
    return m_program;
}

void QTermLocalBackend::setProgram(const QString &program)
{
    if (m_program == program) {
        return;
    }

    m_program = program;
    emit programChanged();
}

QStringList QTermLocalBackend::arguments() const
{
    return m_arguments;
}

void QTermLocalBackend::setArguments(const QStringList &arguments)
{
    if (m_arguments == arguments) {
        return;
    }

    m_arguments = arguments;
    emit argumentsChanged();
}

QString QTermLocalBackend::workingDirectory() const
{
    return m_workingDirectory;
}

void QTermLocalBackend::setWorkingDirectory(const QString &workingDirectory)
{
    if (m_workingDirectory == workingDirectory) {
        return;
    }

    m_workingDirectory = workingDirectory;
    emit workingDirectoryChanged();
}

bool QTermLocalBackend::isRunning() const
{
    return m_running;
}

bool QTermLocalBackend::start()
{
    if (m_running) {
        return true;
    }

    const QString executable = resolvedProgram();
    if (executable.isEmpty()) {
        return false;
    }

    struct winsize windowSize = {};
    windowSize.ws_col = 80;
    windowSize.ws_row = 24;

    const pid_t childPid = forkpty(&m_masterFd, nullptr, nullptr, &windowSize);
    if (childPid < 0) {
        m_masterFd = -1;
        return false;
    }

    if (childPid == 0) {
        if (!m_workingDirectory.isEmpty()) {
            ::chdir(QFile::encodeName(m_workingDirectory).constData());
        }

        QByteArray executableBytes = QFile::encodeName(executable);
        QByteArrayList argumentValues = buildArgumentVector(executable, m_arguments);
        std::unique_ptr<char *[]> argv(buildArgv(argumentValues));
        ::execvp(executableBytes.constData(), argv.get());
        _exit(127);
    }

    m_childPid = static_cast<int>(childPid);
    m_exitEmitted = false;

    const int currentFlags = ::fcntl(m_masterFd, F_GETFL, 0);
    if (currentFlags >= 0) {
        ::fcntl(m_masterFd, F_SETFL, currentFlags | O_NONBLOCK);
    }

    m_readNotifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated,
            this, &QTermLocalBackend::handleMasterReadable);

    m_childPollTimer->start();
    setRunning(true);
    return true;
}

qint64 QTermLocalBackend::bytesAvailable() const
{
    return m_readBuffer.size();
}

QByteArray QTermLocalBackend::read(qint64 maxSize)
{
    if (maxSize <= 0 || m_readBuffer.isEmpty()) {
        return QByteArray();
    }

    if (maxSize >= m_readBuffer.size()) {
        return readAll();
    }

    QByteArray data = m_readBuffer.first(maxSize);
    m_readBuffer.remove(0, maxSize);
    return data;
}

QByteArray QTermLocalBackend::readAll()
{
    if (m_readBuffer.isEmpty()) {
        return QByteArray();
    }

    QByteArray data;
    data.swap(m_readBuffer);
    return data;
}

void QTermLocalBackend::write(const QByteArray &data)
{
    if (m_masterFd < 0 || data.isEmpty()) {
        return;
    }

    qint64 writtenBytes = 0;
    while (writtenBytes < data.size()) {
        const ssize_t result = ::write(m_masterFd,
                                       data.constData() + writtenBytes,
                                       static_cast<std::size_t>(data.size() - writtenBytes));
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }

            break;
        }

        writtenBytes += result;
    }
}

void QTermLocalBackend::resize(int columns, int rows)
{
    if (m_masterFd < 0 || columns <= 0 || rows <= 0) {
        return;
    }

    struct winsize windowSize = {};
    windowSize.ws_col = static_cast<unsigned short>(columns);
    windowSize.ws_row = static_cast<unsigned short>(rows);
    ::ioctl(m_masterFd, TIOCSWINSZ, &windowSize);
}

void QTermLocalBackend::close()
{
    if (m_childPid > 0) {
        ::kill(m_childPid, SIGHUP);
    }

    cleanupMasterFd();
    pollChildExit();

    if (m_childPid <= 0) {
        m_childPollTimer->stop();
        setRunning(false);
    }
}

void QTermLocalBackend::handleMasterReadable()
{
    if (m_masterFd < 0) {
        return;
    }

    QByteArray buffer(4096, Qt::Uninitialized);
    bool hasNewData = false;
    while (true) {
        const ssize_t bytesRead = ::read(m_masterFd, buffer.data(), static_cast<std::size_t>(buffer.size()));
        if (bytesRead > 0) {
            m_readBuffer.append(buffer.constData(), bytesRead);
            hasNewData = true;
            continue;
        }

        if (bytesRead == 0) {
            if (hasNewData) {
                emit readyRead();
            }
            cleanupMasterFd();
            pollChildExit();
            return;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (hasNewData) {
                emit readyRead();
            }
            return;
        }

        if (hasNewData) {
            emit readyRead();
        }
        cleanupMasterFd();
        pollChildExit();
        return;
    }
}

void QTermLocalBackend::pollChildExit()
{
    if (m_childPid <= 0) {
        return;
    }

    int status = 0;
    const pid_t result = ::waitpid(m_childPid, &status, WNOHANG);
    if (result == 0) {
        return;
    }

    if (result < 0) {
        if (errno == EINTR) {
            return;
        }

        m_childPid = -1;
        m_childPollTimer->stop();
        setRunning(false);
        return;
    }

    m_childPid = -1;
    cleanupMasterFd();
    m_childPollTimer->stop();
    setRunning(false);
    finalizeExit(exitCodeFromStatus(status));
}

QString QTermLocalBackend::resolvedProgram() const
{
    if (!m_program.isEmpty()) {
        return m_program;
    }

    const QString shell = qEnvironmentVariable("SHELL");
    if (!shell.isEmpty()) {
        return shell;
    }

    return QStringLiteral("/bin/zsh");
}

void QTermLocalBackend::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }

    m_running = running;
    emit runningChanged();
}

void QTermLocalBackend::cleanupMasterFd()
{
    if (m_readNotifier) {
        m_readNotifier->deleteLater();
        m_readNotifier = nullptr;
    }

    if (m_masterFd >= 0) {
        ::close(m_masterFd);
        m_masterFd = -1;
    }
}

void QTermLocalBackend::finalizeExit(int exitCode)
{
    if (m_exitEmitted) {
        return;
    }

    m_exitEmitted = true;
    emit sessionExited(exitCode);
}