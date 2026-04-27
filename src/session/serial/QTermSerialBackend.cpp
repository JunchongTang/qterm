#include <QTerm/QTermSerialBackend.h>

#include <QSerialPort>

namespace QTerm {

// ── Constructor / destructor ──────────────────────────────────────────────────

QTermSerialBackend::QTermSerialBackend(QObject *parent)
    : QTermSessionBackend(parent)
    , m_serial(new QSerialPort(this))
{
    connect(m_serial, &QSerialPort::readyRead, this, [this]() {
        const QByteArray data = m_serial->readAll();
        if (!data.isEmpty())
            emitDataReceived(data);
    });

    connect(m_serial, &QSerialPort::errorOccurred, this,
            [this](QSerialPort::SerialPortError err) {
        if (err == QSerialPort::NoError) return;
        emitErrorOccurred(m_serial->errorString());
        if (state() == Open || state() == Opening) {
            m_serial->close();
            setState(Error);
        }
    });
}

QTermSerialBackend::~QTermSerialBackend()
{
    close();
}

// ── Port selection ────────────────────────────────────────────────────────────

QString QTermSerialBackend::portName() const { return m_portName; }
void    QTermSerialBackend::setPortName(const QString &p) { m_portName = p; }

// ── Line parameters ───────────────────────────────────────────────────────────

int  QTermSerialBackend::baudRate() const noexcept { return m_baudRate; }
void QTermSerialBackend::setBaudRate(int b)
{
    m_baudRate = b;
    emit baudRateChanged();
}

int  QTermSerialBackend::dataBits() const noexcept { return m_dataBits; }
void QTermSerialBackend::setDataBits(int d)
{
    m_dataBits = d;
    emit dataBitsChanged();
}

QString QTermSerialBackend::parity() const { return m_parity; }
void    QTermSerialBackend::setParity(const QString &p)
{
    m_parity = p;
    emit parityChanged();
}

int  QTermSerialBackend::stopBits() const noexcept { return m_stopBits; }
void QTermSerialBackend::setStopBits(int s)        { m_stopBits = s; }

QString QTermSerialBackend::flowControl() const        { return m_flowControl; }
void    QTermSerialBackend::setFlowControl(const QString &f) { m_flowControl = f; }

// ── open / close ──────────────────────────────────────────────────────────────

void QTermSerialBackend::open()
{
    if (state() == Open || state() == Opening) return;

    if (m_portName.isEmpty()) {
        emitErrorOccurred(QStringLiteral("No serial port specified."));
        setState(Error);
        return;
    }

    setState(Opening);
    m_serial->setPortName(m_portName);
    applySettings();

    if (!m_serial->open(QIODevice::ReadWrite)) {
        emitErrorOccurred(m_serial->errorString());
        setState(Error);
        return;
    }

    setState(Open);
}

void QTermSerialBackend::close()
{
    if (state() == Closed) return;
    setState(Closing);
    m_serial->close();
    setState(Closed);
}

void QTermSerialBackend::writeData(const QByteArray &data)
{
    if (data.isEmpty() || state() != Open) return;
    m_serial->write(data);
}

void QTermSerialBackend::resize(int /*columns*/, int /*rows*/)
{
    // Serial devices have no terminal geometry; intentionally ignored.
}

// ── Private helpers ───────────────────────────────────────────────────────────

void QTermSerialBackend::applySettings()
{
    m_serial->setBaudRate(m_baudRate);

    switch (m_dataBits) {
    case 5: m_serial->setDataBits(QSerialPort::Data5); break;
    case 6: m_serial->setDataBits(QSerialPort::Data6); break;
    case 7: m_serial->setDataBits(QSerialPort::Data7); break;
    default: m_serial->setDataBits(QSerialPort::Data8); break;
    }

    if (m_parity == QLatin1String("E"))
        m_serial->setParity(QSerialPort::EvenParity);
    else if (m_parity == QLatin1String("O"))
        m_serial->setParity(QSerialPort::OddParity);
    else if (m_parity == QLatin1String("M"))
        m_serial->setParity(QSerialPort::MarkParity);
    else if (m_parity == QLatin1String("S"))
        m_serial->setParity(QSerialPort::SpaceParity);
    else
        m_serial->setParity(QSerialPort::NoParity);

    m_serial->setStopBits(m_stopBits == 2
        ? QSerialPort::TwoStop
        : QSerialPort::OneStop);

    if (m_flowControl == QLatin1String("hardware"))
        m_serial->setFlowControl(QSerialPort::HardwareControl);
    else if (m_flowControl == QLatin1String("software"))
        m_serial->setFlowControl(QSerialPort::SoftwareControl);
    else
        m_serial->setFlowControl(QSerialPort::NoFlowControl);
}

} // namespace QTerm
