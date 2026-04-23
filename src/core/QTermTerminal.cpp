#include <QTerm/QTermTerminal.h>

#include "QTermCore.h"

#include <QString>

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

    connect(m_core, &QTermCore::titleChanged, this, &QTermTerminal::setTitle);

    connect(m_core, &QTermCore::outboundData, this, &QTermTerminal::outboundData);

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

QTermSession *QTermTerminal::session() const noexcept
{
    return m_session;
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

void QTermTerminal::sendKey(int key, const QString &text)
{
    m_core->sendKey(key, text);
}

void QTermTerminal::sendPaste(const QString &text)
{
    m_core->sendPaste(text);
}

void QTermTerminal::setSession(QTermSession *session)
{
    if (m_session == session) {
        return;
    }

    QObject::disconnect(m_sessionDataConnection);
    QObject::disconnect(m_sessionDestroyedConnection);
    QObject::disconnect(m_coreOutboundConnection);
    QObject::disconnect(m_sizeToSessionResizeConnection);

    m_sessionUtf8Decoder = QStringDecoder(QStringDecoder::Utf8);

    m_session = session;

    if (m_session) {
        m_sessionDataConnection = connect(m_session, &QTermSession::dataReceived, this, [this](const QByteArray &data) {
            m_core->writePlainText(m_sessionUtf8Decoder(data));
        });
        m_sessionDestroyedConnection = connect(m_session, &QObject::destroyed, this, [this]() {
            QObject::disconnect(m_sessionDataConnection);
            QObject::disconnect(m_sessionDestroyedConnection);
            QObject::disconnect(m_coreOutboundConnection);
            QObject::disconnect(m_sizeToSessionResizeConnection);
            m_sessionUtf8Decoder = QStringDecoder(QStringDecoder::Utf8);
            m_session = nullptr;
            emit sessionChanged();
        });
        m_coreOutboundConnection = connect(m_core, &QTermCore::outboundData, m_session, &QTermSession::writeData);
        m_sizeToSessionResizeConnection = connect(this, &QTermTerminal::sizeChanged, this, [this]() {
            if (m_session) {
                m_session->resize(columns(), rows());
            }
        });
        m_session->resize(columns(), rows());
    }

    emit sessionChanged();
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