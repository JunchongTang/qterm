#pragma once

#include <memory>

#include <QByteArray>
#include <QObject>

class QTermSessionBackend;

class QTermSession : public QObject
{
    Q_OBJECT

public:
    explicit QTermSession(QObject *parent = nullptr);
    ~QTermSession() override;

    bool start();
    qint64 bytesAvailable() const;
    QByteArray read(qint64 maxSize);
    QByteArray readAll();
    void write(const QByteArray &data);
    void resize(int columns, int rows);
    void close();

    void setBackend(std::unique_ptr<QTermSessionBackend> backend);
    QTermSessionBackend *backend() const;

signals:
    void readyRead();
    void sessionExited(int exitCode);
    void titleChanged(const QString &title);
    void bellTriggered();

private:
    void connectBackendSignals();

    std::unique_ptr<QTermSessionBackend> m_backend;
};