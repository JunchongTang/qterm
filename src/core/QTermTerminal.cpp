#include <QTerm/QTermTerminal.h>

#include "QTermCore.h"

namespace QTerm {

QTermTerminal::QTermTerminal(QObject *parent)
    : QObject(parent)
    , m_core(new QTermCore(this))
    , m_surfaceModel(this)
{
    connect(m_core, &QTermCore::sizeChanged, this, [this]() {
        m_surfaceModel.setSize(m_core->columns(), m_core->rows());
        emit sizeChanged();
    });

    connect(m_core, &QTermCore::debugPlainTextChanged, this, [this]() {
        m_surfaceModel.setPlainText(m_core->debugPlainText());
    });

    m_surfaceModel.setSize(m_core->columns(), m_core->rows());
    m_surfaceModel.setPlainText(m_core->debugPlainText());
}

int QTermTerminal::rows() const noexcept
{
    return m_core->rows();
}

int QTermTerminal::columns() const noexcept
{
    return m_core->columns();
}

QString QTermTerminal::title() const
{
    return m_title;
}

QTermSurfaceModel *QTermTerminal::surfaceModel() noexcept
{
    return &m_surfaceModel;
}

void QTermTerminal::clear()
{
    m_core->clear();
}

void QTermTerminal::feedText(const QString &text)
{
    m_core->writePlainText(text);
}

void QTermTerminal::setTerminalSize(int columns, int rows)
{
    m_core->setTerminalSize(columns, rows);
}

void QTermTerminal::setTitle(const QString &title)
{
    if (m_title == title) {
        return;
    }

    m_title = title;
    emit titleChanged();
}

} // namespace QTerm