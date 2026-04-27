#ifndef QTERM_QTERMLOCALPTYBACKEND_H
#define QTERM_QTERMLOCALPTYBACKEND_H

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <QtQml/qqmlregistration.h>

#include <QTerm/QTermSessionBackend.h>

class QSocketNotifier;
class QTimer;

namespace QTerm {

class QTermLocalPtyBackend : public QTermSessionBackend
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString program READ program WRITE setProgram NOTIFY programChanged)
    Q_PROPERTY(QStringList arguments READ arguments WRITE setArguments NOTIFY argumentsChanged)
    Q_PROPERTY(QString workingDirectory READ workingDirectory WRITE setWorkingDirectory NOTIFY workingDirectoryChanged)

public:
    explicit QTermLocalPtyBackend(QObject *parent = nullptr);
    ~QTermLocalPtyBackend() override;

    QString program() const;
    QStringList arguments() const;
    QString workingDirectory() const;
    QProcessEnvironment processEnvironment() const;

    void setProgram(const QString &program);
    void setArguments(const QStringList &arguments);
    void setWorkingDirectory(const QString &workingDirectory);
    void setProcessEnvironment(const QProcessEnvironment &environment);

    void open() override;
    void close() override;
    void writeData(const QByteArray &data) override;
    void resize(int columns, int rows) override;

signals:
    void programChanged();
    void argumentsChanged();
    void workingDirectoryChanged();

private:
    void applyPendingResize();
    void handleReadable();
    void pollChildExit();
    void closeMasterFd();
    void stopRuntimeWatchers();
    QString resolvedProgram() const;
    QStringList resolvedArguments() const;
    QProcessEnvironment resolvedEnvironment() const;
    void applyEnvironmentOverrides() const;

    QString m_program;
    QStringList m_arguments;
    QString m_workingDirectory;
    QProcessEnvironment m_environment;
    int m_columns = 80;
    int m_rows = 24;
    int m_masterFd = -1;
    qint64 m_childPid = -1;
    QSocketNotifier *m_readNotifier = nullptr;
    QTimer *m_resizeDebounceTimer = nullptr;
    QTimer *m_childExitPollTimer = nullptr;
};

} // namespace QTerm

#endif // QTERM_QTERMLOCALPTYBACKEND_H