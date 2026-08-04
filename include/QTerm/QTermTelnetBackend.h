#ifndef QTERM_QTERMTELNETBACKEND_H
#define QTERM_QTERMTELNETBACKEND_H

#include <QByteArray>
#include <QString>

#include <QtQml/qqmlregistration.h>

#include <QTerm/QTermSessionBackend.h>

class QTcpSocket;

namespace QTerm {

/*!
    \class QTermTelnetBackend
    \inmodule QTerm
    \brief Telnet session backend based on QTcpSocket.

    The backend implements core Telnet negotiation and forwards data between
    a network Telnet endpoint and a QTerm session.
*/
class QTermTelnetBackend : public QTermSessionBackend
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(quint16 port READ port WRITE setPort NOTIFY portChanged)

public:
    explicit QTermTelnetBackend(QObject *parent = nullptr);
    ~QTermTelnetBackend() override;

    /*! \brief Returns the remote Telnet host name or IP address. */
    QString host() const;
    /*! \brief Sets the remote Telnet host name or IP address. */
    void    setHost(const QString &host);

    /*! \brief Returns the remote Telnet port. */
    quint16 port() const noexcept;
    /*! \brief Sets the remote Telnet port. */
    void    setPort(quint16 port);

    // ── QTermSessionBackend interface ─────────────────────────────────────────
    void open() override;
    void close() override;
    void writeData(const QByteArray &data) override;

    /*! \brief Sends terminal size updates through NAWS negotiation. */
    void resize(int columns, int rows) override;

signals:
    void hostChanged();
    void portChanged();

private:
    // IAC parser states
    enum class TelnetState {
        Normal,           // accumulating data
        IacReceived,      // 0xFF seen, waiting for command
        CommandReceived,  // WILL/WONT/DO/DONT seen, waiting for option byte
        SubNegotiating,   // inside IAC SB … collecting until IAC SE
        SubIac,           // IAC seen inside subnegotiation
    };

    void onConnected();
    void onReadyRead();
    void onSocketError();

    // Send the initial option negotiation burst.
    void sendInitialNegotiation();

    // Process one full IAC command.  cmd = WILL/WONT/DO/DONT, opt = option byte.
    void handleCommand(quint8 cmd, quint8 opt);

    // Write raw bytes to the socket (no escaping).
    void sendRaw(const QByteArray &bytes);

    // Send a single IAC <cmd> <opt> triplet.
    void sendCommand(quint8 cmd, quint8 opt);

    // Send IAC SB NAWS <cols-hi> <cols-lo> <rows-hi> <rows-lo> IAC SE.
    void sendNaws();

    QTcpSocket *m_socket = nullptr;
    QString     m_host;
    quint16     m_port   = 23;
    int         m_cols   = 80;
    int         m_rows   = 24;

    // Parser state
    TelnetState m_state  = TelnetState::Normal;
    quint8      m_cmd    = 0;   // pending WILL/WONT/DO/DONT byte
    QByteArray  m_subBuf;       // accumulates subnegotiation payload
};

} // namespace QTerm

#endif // QTERM_QTERMTELNETBACKEND_H
