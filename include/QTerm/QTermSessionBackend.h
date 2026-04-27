#ifndef QTERM_QTERMSESSIONBACKEND_H
#define QTERM_QTERMSESSIONBACKEND_H

#include <QByteArray>
#include <QObject>
#include <QString>

#include <QtQml/qqmlregistration.h>

namespace QTerm {

class QTermSessionBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)

public:
    enum State {
        Closed,
        Opening,
        Open,
        Closing,
        Error,
    };
    Q_ENUM(State)

    explicit QTermSessionBackend(QObject *parent = nullptr);

    State state() const noexcept;

    virtual void open() = 0;
    virtual void close() = 0;
    virtual void writeData(const QByteArray &data) = 0;
    virtual void resize(int columns, int rows) = 0;

signals:
    void dataReceived(const QByteArray &data);
    void stateChanged();
    void errorOccurred(const QString &message);

protected:
    void setState(State state);
    void emitDataReceived(const QByteArray &data);
    void emitErrorOccurred(const QString &message);

private:
    State m_state = Closed;
};

} // namespace QTerm

#endif // QTERM_QTERMSESSIONBACKEND_H