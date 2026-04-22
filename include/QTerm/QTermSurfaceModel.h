#ifndef QTERM_QTERMSURFACEMODEL_H
#define QTERM_QTERMSURFACEMODEL_H

#include <QObject>
#include <QString>

namespace QTerm {

class QTermSurfaceModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int rows READ rows NOTIFY sizeChanged)
    Q_PROPERTY(int columns READ columns NOTIFY sizeChanged)
    Q_PROPERTY(QString plainText READ plainText NOTIFY plainTextChanged)

public:
    explicit QTermSurfaceModel(QObject *parent = nullptr);

    int rows() const noexcept;
    int columns() const noexcept;
    QString plainText() const;

    void setSize(int columns, int rows);
    void setPlainText(const QString &plainText);

signals:
    void sizeChanged();
    void plainTextChanged();

private:
    int m_rows = 24;
    int m_columns = 80;
    QString m_plainText;
};

} // namespace QTerm

#endif // QTERM_QTERMSURFACEMODEL_H