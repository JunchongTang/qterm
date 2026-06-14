#ifndef QTERM_QTERMSESSIONBACKEND_H
#define QTERM_QTERMSESSIONBACKEND_H

#include <QByteArray>
#include <QObject>
#include <QString>

#include <QtQml/qqmlregistration.h>

namespace QTerm {

class QTermSessionBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)

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

    explicit QTermSessionBackend(QObject *parent = nullptr);

    State state() const noexcept;

    virtual void open() = 0;
    virtual void close() = 0;
    virtual void writeData(const QByteArray &data) = 0;
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