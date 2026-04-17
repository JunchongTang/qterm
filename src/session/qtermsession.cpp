#include <QTerm/qtermsession.h>

#include <QTerm/qtermsessionbackend.h>

QTermSession::QTermSession(QObject *parent)
    : QObject(parent)
{
}

QTermSession::~QTermSession() = default;

bool QTermSession::start()
{
    return m_backend ? m_backend->start() : false;
}

qint64 QTermSession::bytesAvailable() const
{
    return m_backend ? m_backend->bytesAvailable() : 0;
}

QByteArray QTermSession::read(qint64 maxSize)
{
    return m_backend ? m_backend->read(maxSize) : QByteArray();
}

QByteArray QTermSession::readAll()
{
    return m_backend ? m_backend->readAll() : QByteArray();
}

void QTermSession::write(const QByteArray &data)
{
    if (m_backend) {
        m_backend->write(data);
    }
}

void QTermSession::resize(int columns, int rows)
{
    if (m_backend) {
        m_backend->resize(columns, rows);
    }
}

void QTermSession::close()
{
    if (m_backend) {
        m_backend->close();
    }
}

void QTermSession::setBackend(std::unique_ptr<QTermSessionBackend> backend)
{
    if (m_backend) {
        m_backend->disconnect(this);
    }

    m_backend = std::move(backend);
    connectBackendSignals();
}

QTermSessionBackend *QTermSession::backend() const
{
    return m_backend.get();
}

void QTermSession::connectBackendSignals()
{
    if (!m_backend) {
        return;
    }

        connect(m_backend.get(), &QTermSessionBackend::readyRead,
            this, &QTermSession::readyRead);
    connect(m_backend.get(), &QTermSessionBackend::sessionExited,
            this, &QTermSession::sessionExited);
    connect(m_backend.get(), &QTermSessionBackend::titleChanged,
            this, &QTermSession::titleChanged);
    connect(m_backend.get(), &QTermSessionBackend::bellTriggered,
            this, &QTermSession::bellTriggered);
}