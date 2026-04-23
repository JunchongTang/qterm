#ifndef QTERM_QTERMLOCALPTYBACKEND_H
#define QTERM_QTERMLOCALPTYBACKEND_H

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <QTerm/QTermSessionBackend.h>

class QSocketNotifier;
class QTimer;

namespace QTerm {

class QTermLocalPtyBackend final : public QTermSessionBackend
{
    Q_OBJECT

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

private:
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
    QTimer *m_childExitPollTimer = nullptr;
};

} // namespace QTerm

#endif // QTERM_QTERMLOCALPTYBACKEND_H