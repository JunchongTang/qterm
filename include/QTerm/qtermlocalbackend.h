#pragma once

#include <QString>
#include <QStringList>

#include <QTerm/qtermsessionbackend.h>

class QSocketNotifier;
class QTimer;

class QTermLocalBackend : public QTermSessionBackend
{
    Q_OBJECT
    Q_PROPERTY(QString program READ program WRITE setProgram NOTIFY programChanged)
    Q_PROPERTY(QStringList arguments READ arguments WRITE setArguments NOTIFY argumentsChanged)
    Q_PROPERTY(QString workingDirectory READ workingDirectory WRITE setWorkingDirectory NOTIFY workingDirectoryChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)

public:
    explicit QTermLocalBackend(QObject *parent = nullptr);
    ~QTermLocalBackend() override;

    QString program() const;
    void setProgram(const QString &program);

    QStringList arguments() const;
    void setArguments(const QStringList &arguments);

    QString workingDirectory() const;
    void setWorkingDirectory(const QString &workingDirectory);

    bool isRunning() const;

    bool start() override;
    qint64 bytesAvailable() const override;
    QByteArray read(qint64 maxSize) override;
    QByteArray readAll() override;
    void write(const QByteArray &data) override;
    void resize(int columns, int rows) override;
    void close() override;

signals:
    void programChanged();
    void argumentsChanged();
    void workingDirectoryChanged();
    void runningChanged();

private slots:
    void handleMasterReadable();
    void pollChildExit();

private:
    QString resolvedProgram() const;
    void setRunning(bool running);
    void cleanupMasterFd();
    void finalizeExit(int exitCode);

    QString m_program;
    QStringList m_arguments;
    QString m_workingDirectory;
    QSocketNotifier *m_readNotifier = nullptr;
    QTimer *m_childPollTimer = nullptr;
    int m_masterFd = -1;
    int m_childPid = -1;
    bool m_running = false;
    bool m_exitEmitted = false;
    QByteArray m_readBuffer;
};