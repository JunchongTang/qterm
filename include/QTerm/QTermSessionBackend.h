#ifndef QTERM_QTERMSESSIONBACKEND_H
#define QTERM_QTERMSESSIONBACKEND_H

#include <QByteArray>
#include <QObject>
#include <QString>

#include <QtQml/qqmlregistration.h>

namespace QTerm {

/*!
    \class QTermSessionBackend
    \inmodule QTerm
    \brief Abstract base class for terminal backends that provide I/O and state changes.
*/
class QTermSessionBackend : public QObject
{
    Q_OBJECT
    // Registered (but uncreatable) so QML can name the enumerations: `state` is a
    // QML-visible property on both this class and QTermSession, and a consumer that
    // paints a connection indicator has to compare it against something. Without a
    // QML name the only way to read it from QML is by magic number.
    QML_ELEMENT
    QML_UNCREATABLE("QTermSessionBackend is an abstract base; instantiate a concrete "
                    "backend such as QTermSerialBackend.")
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    // Read-only and on-demand (no NOTIFY): consumers query these via property()
    // at the moment of closing; they are never used in a QML binding.
    Q_PROPERTY(WorkState workState READ workState)
    Q_PROPERTY(QString foregroundProcessName READ foregroundProcessName)

public:
    enum State {
        Closed,
        Opening,
        Open,
        Closing,
        Error,
    };
    Q_ENUM(State)

    // Typed connection error. errorOccurred carries a kind so consumers can
    // branch programmatically (e.g. reconnect policy), plus a human-readable
    // message for display / logging. The kind namespace follows the role
    // pattern of QAbstractItemModel:
    //
    //   - Well-known range (NoError..Other): cross-backend connection error
    //     categories that generic consumers (such as a terminal view's
    //     auto-reconnect logic) branch on. Any backend error that affects
    //     generic policy MUST map onto one of these.
    //   - UserError and above: backend-private diagnostic detail, for when a
    //     backend wants finer error types for its own dedicated UI. It MUST
    //     NOT be used for anything a generic consumer branches on; generic
    //     consumers treat values >= UserError as Other (message only).
    //
    // The signal carries int (not the enum type) so subclass values pass
    // through, mirroring data(int role).
    enum ErrorKind {
        NoError = 0,
        Cancelled,             // user aborted (host-key reject / prompt cancel / close mid-connect)
        ConnectionRefused,     // peer refused the connection
        HostNotFound,          // DNS / name resolution failed
        Timeout,               // connect timed out
        AuthenticationFailed,  // credentials rejected
        PermissionDenied,      // local permission (serial / file)
        DeviceNotFound,        // device absent at open time (serial)
        DeviceRemoved,         // device unplugged while connected (serial hotplug)
        ConnectionLost,        // was Open, dropped mid-session (network blip / peer close)
        SpawnFailed,           // local shell / ConPTY failed to start
        Other,                 // catch-all; see message
        UserError = 0x100,     // start of backend-private diagnostic codes (see above)
    };
    Q_ENUM(ErrorKind)

    // Whether the session currently has "in-progress foreground work" — i.e. the
    // kind that closing would interrupt and that warrants a confirmation. This is
    // a polymorphic seam: the base returns Unknown, and each subclass overrides it
    // with the best precision it can achieve:
    //   - Local shell: inspects the PTY foreground process group (Unix tcgetpgrp /
    //     Windows child-process probe) — precise.
    //   - Serial: no process concept -> Idle.
    //   - Remote (SSH/Telnet/Mosh): the remote process is not observable from the
    //     wire -> stays Unknown; the consumer falls back to OSC 133 shell
    //     integration / a conservative proxy.
    // Queried purely on demand (read at close time); no state is kept and no change
    // signal is emitted.
    enum WorkState {
        WorkUnknown = 0,   // can't tell -> consumer decides (shellZone / proxy)
        WorkIdle    = 1,   // definitely idle (e.g. local shell sitting at a prompt)
        WorkBusy    = 2,   // definitely running a foreground process
    };
    Q_ENUM(WorkState)

    explicit QTermSessionBackend(QObject *parent = nullptr);

    /*!
        \brief Returns the current state of the backend.
        \return The current backend state.
    */
    State state() const noexcept;

    /*!
        \brief Returns whether the session currently has in-progress foreground work.

        Defaults to \c WorkUnknown; subclasses override it with the best precision
        they can achieve. Consumers read this dynamically via QObject::property
        ("workState"), hence the read-only, no-NOTIFY Q_PROPERTY above.
    */
    virtual WorkState workState() const { return WorkUnknown; }
    /*!
        \brief Returns the foreground process name, or an empty string when unknown.
    */
    virtual QString foregroundProcessName() const { return {}; }

    /*!
        \brief Opens the backend and starts the underlying process or connection.
    */
    virtual void open() = 0;

    /*!
        \brief Closes the backend and releases any associated resources.
    */
    virtual void close() = 0;

    /*!
        \brief Writes raw data to the backend input stream.
        \param data The bytes to write.
    */
    virtual void writeData(const QByteArray &data) = 0;

    /*!
        \brief Resizes the underlying terminal dimensions.
        \param columns The new number of columns.
        \param rows The new number of rows.
    */
    virtual void resize(int columns, int rows) = 0;

signals:
    void dataReceived(const QByteArray &data);
    void stateChanged();
    // kind is an ErrorKind value (subclasses may extend with UserError+n);
    // message is human-readable.
    void errorOccurred(int kind, const QString &message);

protected:
    void setState(State state);
    void emitDataReceived(const QByteArray &data);
    void emitErrorOccurred(int kind, const QString &message);

private:
    State m_state = Closed;
};

} // namespace QTerm

#endif // QTERM_QTERMSESSIONBACKEND_H