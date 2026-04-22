#include <QTerm/QTermSurfaceModel.h>

namespace QTerm {

QTermSurfaceModel::QTermSurfaceModel(QObject *parent)
    : QObject(parent)
{
}

int QTermSurfaceModel::rows() const noexcept
{
    return m_rows;
}

int QTermSurfaceModel::columns() const noexcept
{
    return m_columns;
}

QString QTermSurfaceModel::plainText() const
{
    return m_plainText;
}

void QTermSurfaceModel::setSize(int columns, int rows)
{
    if (m_columns == columns && m_rows == rows) {
        return;
    }

    m_columns = columns;
    m_rows = rows;
    emit sizeChanged();
}

void QTermSurfaceModel::setPlainText(const QString &plainText)
{
    if (m_plainText == plainText) {
        return;
    }

    m_plainText = plainText;
    emit plainTextChanged();
}

} // namespace QTerm