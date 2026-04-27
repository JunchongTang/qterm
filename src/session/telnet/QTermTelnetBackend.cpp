#include <QTerm/QTermTelnetBackend.h>

#include <QTcpSocket>

namespace QTerm {

// ── Telnet constants ──────────────────────────────────────────────────────────

static constexpr quint8 IAC  = 0xFF;
static constexpr quint8 DONT = 0xFE;
static constexpr quint8 DO   = 0xFD;
static constexpr quint8 WONT = 0xFC;
static constexpr quint8 WILL = 0xFB;
static constexpr quint8 SB   = 0xFA; // start subnegotiation
static constexpr quint8 SE   = 0xF0; // end subnegotiation

// Option bytes
static constexpr quint8 OPT_ECHO = 0x01;
static constexpr quint8 OPT_SGA  = 0x03; // Suppress Go-Ahead
static constexpr quint8 OPT_NAWS = 0x1F; // Negotiate About Window Size

// ── Constructor / destructor ──────────────────────────────────────────────────

QTermTelnetBackend::QTermTelnetBackend(QObject *parent)
    : QTermSessionBackend(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected,  this, &QTermTelnetBackend::onConnected);
    connect(m_socket, &QTcpSocket::readyRead,  this, &QTermTelnetBackend::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, [this]() {
        if (state() != Closed && state() != Closing) {
            setState(Closed);
        }
    });
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this, &QTermTelnetBackend::onSocketError);
}

QTermTelnetBackend::~QTermTelnetBackend()
{
    close();
}

// ── Connection parameters ─────────────────────────────────────────────────────

QString QTermTelnetBackend::host() const { return m_host; }
void    QTermTelnetBackend::setHost(const QString &h)
{
    m_host = h;
    emit hostChanged();
}

quint16 QTermTelnetBackend::port() const noexcept { return m_port; }
void    QTermTelnetBackend::setPort(quint16 p)
{
    m_port = p;
    emit portChanged();
}

// ── QTermSessionBackend interface ─────────────────────────────────────────────

void QTermTelnetBackend::open()
{
    if (state() == Open || state() == Opening) return;

    if (m_host.isEmpty()) {
        emitErrorOccurred(QStringLiteral("No host specified."));
        return;
    }

    setState(Opening);
    m_socket->connectToHost(m_host, m_port);
}

void QTermTelnetBackend::close()
{
    if (state() == Closed) return;
    setState(Closing);
    m_socket->disconnectFromHost();
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->waitForDisconnected(500);
    setState(Closed);
}

void QTermTelnetBackend::writeData(const QByteArray &data)
{
    if (state() != Open || data.isEmpty()) return;

    // Escape every 0xFF byte to 0xFF 0xFF per RFC 854.
    QByteArray escaped;
    escaped.reserve(data.size());
    for (unsigned char c : data) {
        if (c == IAC)
            escaped.append(static_cast<char>(IAC));
        escaped.append(static_cast<char>(c));
    }
    sendRaw(escaped);
}

void QTermTelnetBackend::resize(int columns, int rows)
{
    m_cols = columns;
    m_rows = rows;
    if (state() == Open)
        sendNaws();
}

// ── Private: socket event handlers ───────────────────────────────────────────

void QTermTelnetBackend::onConnected()
{
    m_state = TelnetState::Normal;
    m_subBuf.clear();
    setState(Open);
    sendInitialNegotiation();
}

void QTermTelnetBackend::onReadyRead()
{
    const QByteArray raw = m_socket->readAll();
    QByteArray plainData; // bytes to pass through to the terminal

    for (unsigned char byte : raw) {
        switch (m_state) {
        case TelnetState::Normal:
            if (byte == IAC) {
                m_state = TelnetState::IacReceived;
            } else {
                plainData.append(static_cast<char>(byte));
            }
            break;

        case TelnetState::IacReceived:
            if (byte == IAC) {
                // Escaped 0xFF — pass through as literal data byte.
                plainData.append(static_cast<char>(IAC));
                m_state = TelnetState::Normal;
            } else if (byte == SB) {
                m_subBuf.clear();
                m_state = TelnetState::SubNegotiating;
            } else if (byte == SE || byte == 0xF1 /* NOP */ || byte == 0xF2 /* DM */
                    || byte == 0xF3 /* BRK */ || byte == 0xF4 /* IP */
                    || byte == 0xF5 /* AO  */ || byte == 0xF6 /* AYT */
                    || byte == 0xF7 /* EC  */ || byte == 0xF8 /* EL */
                    || byte == 0xF9 /* GA */) {
                // Single-byte commands — ignore or could handle AYT/etc.
                m_state = TelnetState::Normal;
            } else if (byte == WILL || byte == WONT || byte == DO || byte == DONT) {
                m_cmd   = byte;
                m_state = TelnetState::CommandReceived;
            } else {
                // Unknown command byte; reset.
                m_state = TelnetState::Normal;
            }
            break;

        case TelnetState::CommandReceived:
            handleCommand(m_cmd, byte);
            m_state = TelnetState::Normal;
            break;

        case TelnetState::SubNegotiating:
            if (byte == IAC) {
                m_state = TelnetState::SubIac;
            } else {
                m_subBuf.append(static_cast<char>(byte));
            }
            break;

        case TelnetState::SubIac:
            if (byte == SE) {
                // Subnegotiation complete — we don't need to act on server's SB.
                m_subBuf.clear();
                m_state = TelnetState::Normal;
            } else if (byte == IAC) {
                // Escaped IAC inside SB.
                m_subBuf.append(static_cast<char>(IAC));
                m_state = TelnetState::SubNegotiating;
            } else {
                // Malformed; discard and reset.
                m_subBuf.clear();
                m_state = TelnetState::Normal;
            }
            break;
        }
    }

    if (!plainData.isEmpty())
        emitDataReceived(plainData);
}

void QTermTelnetBackend::onSocketError()
{
    if (state() != Closed && state() != Closing)
        emitErrorOccurred(m_socket->errorString());
}

// ── Private: protocol helpers ─────────────────────────────────────────────────

void QTermTelnetBackend::sendInitialNegotiation()
{
    // Tell the server we will report window size.
    sendCommand(WILL, OPT_NAWS);
    sendNaws();

    // Ask the server to handle echoing and suppress Go-Ahead (full-duplex).
    sendCommand(DO, OPT_ECHO);
    sendCommand(DO, OPT_SGA);
}

void QTermTelnetBackend::handleCommand(quint8 cmd, quint8 opt)
{
    if (cmd == DO) {
        if (opt == OPT_NAWS) {
            // Server asks us to send window size — we already offered WILL NAWS;
            // respond again and send the current size.
            sendCommand(WILL, OPT_NAWS);
            sendNaws();
        } else {
            // We don't support anything else the server might ask us to DO.
            sendCommand(WONT, opt);
        }
    } else if (cmd == WILL) {
        if (opt == OPT_ECHO || opt == OPT_SGA) {
            // Accept the server's offer.
            sendCommand(DO, opt);
        } else {
            sendCommand(DONT, opt);
        }
    } else if (cmd == DONT) {
        // Server told us to stop — acknowledge with WONT.
        sendCommand(WONT, opt);
    } else if (cmd == WONT) {
        // Server won't do it — acknowledge with DONT.
        sendCommand(DONT, opt);
    }
}

void QTermTelnetBackend::sendRaw(const QByteArray &bytes)
{
    m_socket->write(bytes);
}

void QTermTelnetBackend::sendCommand(quint8 cmd, quint8 opt)
{
    const char buf[3] = {
        static_cast<char>(IAC),
        static_cast<char>(cmd),
        static_cast<char>(opt),
    };
    m_socket->write(buf, 3);
}

void QTermTelnetBackend::sendNaws()
{
    // IAC SB NAWS <cols-hi> <cols-lo> <rows-hi> <rows-lo> IAC SE
    // Each value byte that equals 0xFF must be escaped to 0xFF 0xFF.
    auto appendEscaped = [](QByteArray &buf, quint16 val) {
        const quint8 hi = static_cast<quint8>(val >> 8);
        const quint8 lo = static_cast<quint8>(val & 0xFF);
        buf.append(static_cast<char>(hi));
        if (hi == IAC) buf.append(static_cast<char>(IAC));
        buf.append(static_cast<char>(lo));
        if (lo == IAC) buf.append(static_cast<char>(IAC));
    };

    QByteArray pkt;
    pkt.reserve(12);
    pkt.append(static_cast<char>(IAC));
    pkt.append(static_cast<char>(SB));
    pkt.append(static_cast<char>(OPT_NAWS));
    appendEscaped(pkt, static_cast<quint16>(m_cols));
    appendEscaped(pkt, static_cast<quint16>(m_rows));
    pkt.append(static_cast<char>(IAC));
    pkt.append(static_cast<char>(SE));
    m_socket->write(pkt);
}

} // namespace QTerm
