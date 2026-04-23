#include <QTerm/QTermSession.h>

namespace QTerm {

QTermSession::QTermSession(QObject *parent)
    : QObject(parent)
{
}

QTermSessionBackend *QTermSession::backend() const noexcept
{
    return m_backend;
}

QTermSessionBackend::State QTermSession::state() const noexcept
{
    return m_backend ? m_backend->state() : QTermSessionBackend::Closed;
}

void QTermSession::setBackend(QTermSessionBackend *backend)
{
    if (m_backend == backend) {
        return;
    }

    disconnectBackend();
    m_backend = backend;
    connectBackend();
    emit backendChanged();
    emit stateChanged();
}

void QTermSession::open()
{
    if (!m_backend) {
        return;
    }

    m_backend->open();
}

void QTermSession::close()
{
    if (!m_backend) {
        return;
    }

    m_backend->close();
}

void QTermSession::resize(int columns, int rows)
{
    if (!m_backend) {
        return;
    }

    m_backend->resize(columns, rows);
}

void QTermSession::writeData(const QByteArray &data)
{
    if (!m_backend || data.isEmpty()) {
        return;
    }

    m_backend->writeData(data);
}

void QTermSession::disconnectBackend()
{
    QObject::disconnect(m_backendDataConnection);
    QObject::disconnect(m_backendStateConnection);
    QObject::disconnect(m_backendErrorConnection);
    QObject::disconnect(m_backendDestroyedConnection);
}

void QTermSession::connectBackend()
{
    if (!m_backend) {
        return;
    }

    m_backendDataConnection = connect(m_backend, &QTermSessionBackend::dataReceived, this, &QTermSession::dataReceived);
    m_backendStateConnection = connect(m_backend, &QTermSessionBackend::stateChanged, this, &QTermSession::stateChanged);
    m_backendErrorConnection = connect(m_backend, &QTermSessionBackend::errorOccurred, this, &QTermSession::errorOccurred);
    m_backendDestroyedConnection = connect(m_backend, &QObject::destroyed, this, [this]() {
        disconnectBackend();
        m_backend = nullptr;
        emit backendChanged();
        emit stateChanged();
    });
}

} // namespace QTerm