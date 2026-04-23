#ifndef QTERM_QTERMSESSION_H
#define QTERM_QTERMSESSION_H

#include <QByteArray>
#include <QObject>
#include <QPointer>

#include <QTerm/QTermSessionBackend.h>

namespace QTerm {

class QTermSession final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QTerm::QTermSessionBackend *backend READ backend WRITE setBackend NOTIFY backendChanged)
    Q_PROPERTY(QTerm::QTermSessionBackend::State state READ state NOTIFY stateChanged)

public:
    explicit QTermSession(QObject *parent = nullptr);

    QTermSessionBackend *backend() const noexcept;
    QTermSessionBackend::State state() const noexcept;

    void setBackend(QTermSessionBackend *backend);

    Q_INVOKABLE void open();
    Q_INVOKABLE void close();
    Q_INVOKABLE void resize(int columns, int rows);
    Q_INVOKABLE void writeData(const QByteArray &data);

signals:
    void backendChanged();
    void stateChanged();
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &message);

private:
    void disconnectBackend();
    void connectBackend();

    QPointer<QTermSessionBackend> m_backend;
    QMetaObject::Connection m_backendDataConnection;
    QMetaObject::Connection m_backendStateConnection;
    QMetaObject::Connection m_backendErrorConnection;
    QMetaObject::Connection m_backendDestroyedConnection;
};

} // namespace QTerm

#endif // QTERM_QTERMSESSION_H