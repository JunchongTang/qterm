#ifndef QTERMLOCALBACKEND_H
#define QTERMLOCALBACKEND_H

#include "qtermsessionbackend.h"

class QSocketNotifier;

/**
 * @brief Local PTY backend using POSIX pseudo-terminal.
 *
 * Opens a PTY master/slave pair, forks a child process running the
 * user's shell, and uses a QSocketNotifier to async-read data from the
 * PTY master file descriptor.
 *
 * Platform: macOS / Linux (POSIX).
 */
class QTermLocalBackend : public QTermSessionBackend
{
    Q_OBJECT

public:
    explicit QTermLocalBackend(QObject *parent = nullptr);
    ~QTermLocalBackend() override;

    void start() override;
    void sendData(const QByteArray &data) override;
    void resize(int rows, int cols) override;

private Q_SLOTS:
    void onReadReady();

private:
    void cleanup();

    int              m_masterFd    = -1;
    pid_t            m_childPid    = -1;
    QSocketNotifier *m_readNotifier = nullptr;
};

#endif // QTERMLOCALBACKEND_H
