#include <QTerm/QTermSessionBackend.h>

namespace QTerm {

QTermSessionBackend::QTermSessionBackend(QObject *parent)
    : QObject(parent)
{
}

QTermSessionBackend::State QTermSessionBackend::state() const noexcept
{
    return m_state;
}

void QTermSessionBackend::setState(State state)
{
    if (m_state == state) {
        return;
    }

    m_state = state;
    emit stateChanged();
}

void QTermSessionBackend::emitDataReceived(const QByteArray &data)
{
    emit dataReceived(data);
}

void QTermSessionBackend::emitErrorOccurred(const QString &message)
{
    setState(Error);
    emit errorOccurred(message);
}

} // namespace QTerm