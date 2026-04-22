#ifndef QTERM_QTERMTERMINAL_H
#define QTERM_QTERMTERMINAL_H

#include <QObject>
#include <QString>

#include <QTerm/QTermSurfaceModel.h>

namespace QTerm {

class QTermCore;

class QTermTerminal final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int rows READ rows NOTIFY sizeChanged)
    Q_PROPERTY(int columns READ columns NOTIFY sizeChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QTerm::QTermSurfaceModel *surfaceModel READ surfaceModel CONSTANT)

public:
    explicit QTermTerminal(QObject *parent = nullptr);

    int rows() const noexcept;
    int columns() const noexcept;
    QString title() const;
    QTermSurfaceModel *surfaceModel() noexcept;

    Q_INVOKABLE void clear();
    Q_INVOKABLE void feedText(const QString &text);
    Q_INVOKABLE void setTerminalSize(int columns, int rows);

public slots:
    void setTitle(const QString &title);

signals:
    void sizeChanged();
    void titleChanged();

private:
    QTermCore *m_core = nullptr;
    QTermSurfaceModel m_surfaceModel;
    QString m_title;
};

} // namespace QTerm

#endif // QTERM_QTERMTERMINAL_H