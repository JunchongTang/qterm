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

void QTermSessionBackend::emitErrorOccurred(int kind, const QString &message)
{
    // Emit errorOccurred first (so consumers record the kind), then move to
    // the Error state. This way a consumer reacting to stateChanged already
    // has the kind available when it sees Error (reconnect policy branches
    // on the kind).
    emit errorOccurred(kind, message);
    setState(Error);
}

} // namespace QTerm