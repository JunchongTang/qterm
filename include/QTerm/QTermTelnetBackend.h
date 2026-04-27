#ifndef QTERM_QTERMTELNETBACKEND_H
#define QTERM_QTERMTELNETBACKEND_H

#include <QByteArray>
#include <QString>

#include <QTerm/QTermSessionBackend.h>

class QTcpSocket;

namespace QTerm {

// Telnet session backend backed by QTcpSocket.
//
// Implements RFC 854 (Telnet) with the following option negotiation:
//   NAWS  (RFC 1073) — sends terminal size on connect and on resize()
//   ECHO  (RFC 857)  — asks server to echo input (normal for interactive use)
//   SGA   (RFC 858)  — asks server to suppress Go-Ahead (full-duplex mode)
//
// Unknown DO/WILL requests are refused with WONT/DONT automatically.
//
// Usage:
//   QTermTelnetBackend *b = new QTermTelnetBackend(this);
//   b->setHost("192.168.1.1");
//   b->setPort(23);
//   b->open();
//   // wire to QTermSession the same way as QTermLocalPtyBackend
class QTermTelnetBackend final : public QTermSessionBackend
{
    Q_OBJECT

public:
    explicit QTermTelnetBackend(QObject *parent = nullptr);
    ~QTermTelnetBackend() override;

    // ── Connection parameters ─────────────────────────────────────────────────
    QString host() const;
    void    setHost(const QString &host);

    quint16 port() const noexcept;
    void    setPort(quint16 port);

    // ── QTermSessionBackend interface ─────────────────────────────────────────
    void open() override;
    void close() override;
    void writeData(const QByteArray &data) override;

    // Sends an IAC SB NAWS subnegotiation to inform the server of the new size.
    void resize(int columns, int rows) override;

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
