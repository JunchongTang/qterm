#include <QTerm/qtermplaintextsurfaceadapter.h>

#include <QTerm/qtermparser.h>
#include <QTerm/qtermlibvtermparser.h>
#include <QTerm/qtermsession.h>
#include <QTerm/qtermsurfacemodel.h>

QTermPlainTextSurfaceAdapter::QTermPlainTextSurfaceAdapter(QObject *parent)
    : QObject(parent)
    , m_parser(new QTermLibvtermParser(this))
{
}

QTermPlainTextSurfaceAdapter::~QTermPlainTextSurfaceAdapter() = default;

QTermSession *QTermPlainTextSurfaceAdapter::session() const
{
    return m_session;
}

void QTermPlainTextSurfaceAdapter::setSession(QTermSession *session)
{
    if (m_session == session) {
        return;
    }

    if (m_session) {
        m_session->disconnect(this);
    }

    m_session = session;
    if (m_session) {
        connect(m_session, &QTermSession::readyRead,
                this, &QTermPlainTextSurfaceAdapter::consumeSessionData);
        connect(m_session, &QTermSession::sessionExited,
            this, &QTermPlainTextSurfaceAdapter::resetParser);
    }
}

QTermSurfaceModel *QTermPlainTextSurfaceAdapter::surfaceModel() const
{
    return m_surfaceModel;
}

void QTermPlainTextSurfaceAdapter::setSurfaceModel(QTermSurfaceModel *surfaceModel)
{
    if (m_surfaceModel == surfaceModel) {
        return;
    }

    if (m_surfaceModel) {
        m_surfaceModel->disconnect(this);
    }

    m_surfaceModel = surfaceModel;
    if (m_surfaceModel) {
        connect(m_surfaceModel, &QTermSurfaceModel::terminalSizeChanged,
                this, &QTermPlainTextSurfaceAdapter::synchronizeParserTerminalSize);
    }

    if (m_parser) {
        m_parser->setSurfaceModel(m_surfaceModel);
    }

    synchronizeParserTerminalSize();
}

QTermParser *QTermPlainTextSurfaceAdapter::parser() const
{
    return m_parser;
}

void QTermPlainTextSurfaceAdapter::setParser(QTermParser *parser)
{
    if (m_parser == parser) {
        return;
    }

    if (m_parser && m_parser->parent() == this) {
        m_parser->deleteLater();
    }

    m_parser = parser;
    if (m_parser && !m_parser->parent()) {
        m_parser->setParent(this);
    }

    if (m_parser) {
        m_parser->setSurfaceModel(m_surfaceModel);
    }

    synchronizeParserTerminalSize();
}

void QTermPlainTextSurfaceAdapter::consumeSessionData()
{
    if (!m_session || !m_parser) {
        return;
    }

    const QByteArray data = m_session->readAll();
    if (data.isEmpty()) {
        return;
    }

    m_parser->processBytes(data);
}

void QTermPlainTextSurfaceAdapter::synchronizeParserTerminalSize()
{
    if (!m_parser || !m_surfaceModel) {
        return;
    }

    m_parser->setTerminalSize(m_surfaceModel->terminalSize());
}

void QTermPlainTextSurfaceAdapter::resetParser()
{
    if (!m_parser) {
        return;
    }

    m_parser->reset();
}