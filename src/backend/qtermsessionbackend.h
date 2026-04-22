#ifndef QTERMSESSIONBACKEND_H
#define QTERMSESSIONBACKEND_H

#include <QObject>
#include <QByteArray>

/**
 * @brief Abstract base class for terminal session backends.
 *
 * Subclasses implement different backends, e.g. local PTY or remote SSH.
 * The backend owns the I/O channel and emits dataReceived() when new
 * bytes arrive from the remote end.
 */
class QTermSessionBackend : public QObject
{
    Q_OBJECT

public:
    explicit QTermSessionBackend(QObject *parent = nullptr);
    ~QTermSessionBackend() override;

    /**
     * @brief Start the backend (fork shell, open connection, etc.).
     */
    virtual void start() = 0;

    /**
     * @brief Send raw bytes to the remote end (PTY master, socket, etc.).
     */
    virtual void sendData(const QByteArray &data) = 0;

    /**
     * @brief Notify the backend that the terminal size changed.
     * @param rows  New number of rows.
     * @param cols  New number of columns.
     */
    virtual void resize(int rows, int cols) = 0;

Q_SIGNALS:
    /** Emitted when new data arrives from the remote end. */
    void dataReceived(const QByteArray &data);

    /** Emitted when the remote process terminates. */
    void finished(int exitCode);
};

#endif // QTERMSESSIONBACKEND_H
