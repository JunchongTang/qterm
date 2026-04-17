#pragma once

#include <QByteArray>
#include <QObject>

class QTermSessionBackend : public QObject
{
    Q_OBJECT

public:
    explicit QTermSessionBackend(QObject *parent = nullptr);
    ~QTermSessionBackend() override;

    virtual bool start() = 0;
    virtual qint64 bytesAvailable() const = 0;
    virtual QByteArray read(qint64 maxSize) = 0;
    virtual QByteArray readAll() = 0;
    virtual void write(const QByteArray &data) = 0;
    virtual void resize(int columns, int rows) = 0;
    virtual void close() = 0;

signals:
    void readyRead();
    void sessionExited(int exitCode);
    void titleChanged(const QString &title);
    void bellTriggered();
};