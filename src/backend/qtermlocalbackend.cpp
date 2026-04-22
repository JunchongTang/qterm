#include "qtermlocalbackend.h"

#include <QSocketNotifier>
#include <QDebug>

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

QTermLocalBackend::QTermLocalBackend(QObject *parent)
    : QTermSessionBackend(parent)
{
}

QTermLocalBackend::~QTermLocalBackend()
{
    cleanup();
}

void QTermLocalBackend::start()
{
    // --- Open PTY master ---
    m_masterFd = posix_openpt(O_RDWR | O_NOCTTY);
    if (m_masterFd < 0) {
        qWarning("QTermLocalBackend: posix_openpt failed: %s", strerror(errno));
        return;
    }

    if (grantpt(m_masterFd) < 0 || unlockpt(m_masterFd) < 0) {
        qWarning("QTermLocalBackend: grantpt/unlockpt failed: %s", strerror(errno));
        cleanup();
        return;
    }

    // ptsname returns a pointer to static storage; copy it before fork
    char slaveName[256];
    const char *ptsn = ptsname(m_masterFd);
    if (!ptsn) {
        qWarning("QTermLocalBackend: ptsname failed: %s", strerror(errno));
        cleanup();
        return;
    }
    qstrncpy(slaveName, ptsn, sizeof(slaveName));

    // --- Fork ---
    m_childPid = fork();
    if (m_childPid < 0) {
        qWarning("QTermLocalBackend: fork failed: %s", strerror(errno));
        cleanup();
        return;
    }

    if (m_childPid == 0) {
        // ---- Child process ----
        setsid(); // Start a new session

        int slaveFd = open(slaveName, O_RDWR);
        if (slaveFd < 0)
            _exit(1);

#ifdef TIOCSCTTY
        // Make the PTY slave the controlling terminal
        ioctl(slaveFd, TIOCSCTTY, 0);
#endif

        // Redirect stdin/stdout/stderr to PTY slave
        dup2(slaveFd, STDIN_FILENO);
        dup2(slaveFd, STDOUT_FILENO);
        dup2(slaveFd, STDERR_FILENO);

        if (slaveFd > STDERR_FILENO)
            close(slaveFd);

        // The child must not keep the master fd open
        close(m_masterFd);

        // Set sensible terminal variables
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);

        // Launch the user's shell
        const char *shell = getenv("SHELL");
        if (!shell || shell[0] == '\0')
            shell = "/bin/sh";

        execl(shell, shell, static_cast<char *>(nullptr));
        _exit(127); // exec failed
    }

    // ---- Parent process ----

    // Set master fd non-blocking so reads don't stall the event loop
    int flags = fcntl(m_masterFd, F_GETFL, 0);
    fcntl(m_masterFd, F_SETFL, flags | O_NONBLOCK);

    m_readNotifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated,
            this, &QTermLocalBackend::onReadReady);
}

void QTermLocalBackend::sendData(const QByteArray &data)
{
    if (m_masterFd < 0)
        return;

    const char *buf = data.constData();
    qsizetype remaining = data.size();

    while (remaining > 0) {
        ssize_t written = write(m_masterFd, buf, static_cast<size_t>(remaining));
        if (written < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        buf += written;
        remaining -= written;
    }
}

void QTermLocalBackend::resize(int rows, int cols)
{
    if (m_masterFd < 0)
        return;

    struct winsize ws{};
    ws.ws_row    = static_cast<unsigned short>(rows);
    ws.ws_col    = static_cast<unsigned short>(cols);
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    ioctl(m_masterFd, TIOCSWINSZ, &ws);
}

void QTermLocalBackend::onReadReady()
{
    char buf[4096];
    for (;;) {
        ssize_t n = read(m_masterFd, buf, sizeof(buf));
        if (n > 0) {
            Q_EMIT dataReceived(QByteArray(buf, static_cast<int>(n)));
        } else if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break; // No more data right now
            if (errno == EIO) {
                // PTY slave closed – the shell has exited
                cleanup();
                Q_EMIT finished(0);
                break;
            }
            qWarning("QTermLocalBackend: read error: %s", strerror(errno));
            break;
        } else {
            // EOF
            cleanup();
            Q_EMIT finished(0);
            break;
        }
    }
}

void QTermLocalBackend::cleanup()
{
    if (m_readNotifier) {
        m_readNotifier->setEnabled(false);
        delete m_readNotifier;
        m_readNotifier = nullptr;
    }

    if (m_masterFd >= 0) {
        close(m_masterFd);
        m_masterFd = -1;
    }

    if (m_childPid > 0) {
        // Reap the child without blocking
        waitpid(m_childPid, nullptr, WNOHANG);
        m_childPid = -1;
    }
}
