#ifndef QTERM_QTERMSESSION_H
#define QTERM_QTERMSESSION_H

#include <QByteArray>
#include <QObject>
#include <QPointer>

#include <QtQml/qqmlregistration.h>

#include <QTerm/QTermSessionBackend.h>

namespace QTerm {

/*!
    \class QTermSession
    \inmodule QTerm
    \brief Manages a terminal session backed by a concrete session backend.

    QTermSession acts as the high-level controller for opening, closing and
    exchanging data with a terminal backend such as QTermLocalShellBackend.
*/
class QTermSession : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QTerm::QTermSessionBackend *backend READ backend WRITE setBackend NOTIFY backendChanged)
    Q_PROPERTY(QTerm::QTermSessionBackend::State state READ state NOTIFY stateChanged)

public:
    explicit QTermSession(QObject *parent = nullptr);

    /*!
        \brief Returns the current backend used by the session.
        \return The attached backend, or nullptr if none is set.
    */
    QTermSessionBackend *backend() const noexcept;

    /*!
        \brief Returns the current session state.
        \return The backend state such as Closed, Open or Error.
    */
    QTermSessionBackend::State state() const noexcept;

    /*!
        \brief Attaches a backend to this session.
        \param backend The backend that should provide I/O for the session.
    */
    void setBackend(QTermSessionBackend *backend);

    /*!
        \brief Opens the session and starts the backend process.
    */
    Q_INVOKABLE void open();

    /*!
        \brief Closes the session and stops the backend process.
    */
    Q_INVOKABLE void close();

    /*!
        \brief Resizes the terminal viewport.
        \param columns The new number of columns.
        \param rows The new number of rows.
    */
    Q_INVOKABLE void resize(int columns, int rows);

    /*!
        \brief Sends raw data to the backend.
        \param data The data to write to the terminal input stream.
    */
    Q_INVOKABLE void writeData(const QByteArray &data);

signals:
    void backendChanged();
    void stateChanged();
    void dataReceived(const QByteArray &data);
    void errorOccurred(int kind, const QString &message);

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