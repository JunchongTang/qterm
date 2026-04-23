#include <QTerm/QTermTerminal.h>

#include "QTermCore.h"
#include "QTermSelectionModel.h"

#include <memory>
#include <QString>
#include <QtGlobal>

namespace QTerm {

namespace {

void syncSurfaceCursor(QTermSurfaceModel &surfaceModel, QTermCore *core)
{
    const QTermCursorState cursorState = core->cursorState();
    surfaceModel.setCursor(cursorState.row, cursorState.column, core->modeState().cursorVisible);
}

} // namespace

QTermTerminal::QTermTerminal(QObject *parent)
    : QObject(parent)
    , m_core(new QTermCore(this))
    , m_surfaceModel(this)
    , m_selectionModel(std::make_unique<QTermSelectionModel>())
{
    m_surfaceModel.setSelectionController(this);
    m_selectionModel->setTerminalSize(m_core->columns(), m_core->rows());

    connect(m_core, &QTermCore::sizeChanged, this, [this]() {
        m_surfaceModel.setSize(m_core->columns(), m_core->rows());
        m_selectionModel->completeResize(m_core->buffer(), m_core->columns(), m_core->rows());
        syncSurfaceSelection();
        emit sizeChanged();
    });

    connect(m_core, &QTermCore::debugPlainTextChanged, this, [this]() {
        m_selectionModel->refreshSelectionText(m_core->buffer());
        syncSurfaceSelection();
        m_surfaceModel.setVisibleLines(m_core->buffer().visibleLineTexts());
        m_surfaceModel.setVisibleLineRuns(m_core->buffer().visibleLineRuns());
        m_surfaceModel.setPlainText(m_core->debugPlainText());
    });

    connect(m_core, &QTermCore::cursorStateChanged, this, [this]() {
        syncSurfaceCursor(m_surfaceModel, m_core);
    });

    connect(m_core, &QTermCore::bell, this, &QTermTerminal::bell);
    connect(m_core, &QTermCore::titleChanged, this, &QTermTerminal::setTitle);

    connect(m_core, &QTermCore::outboundData, this, &QTermTerminal::outboundData);

    m_surfaceModel.setSize(m_core->columns(), m_core->rows());
    m_selectionModel->refreshSelectionText(m_core->buffer());
    syncSurfaceSelection();
    m_surfaceModel.setVisibleLines(m_core->buffer().visibleLineTexts());
    m_surfaceModel.setVisibleLineRuns(m_core->buffer().visibleLineRuns());
    m_surfaceModel.setPlainText(m_core->debugPlainText());
    syncSurfaceCursor(m_surfaceModel, m_core);
}

QTermTerminal::~QTermTerminal() = default;

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
    clearSelection();
}

void QTermTerminal::feedText(const QString &text)
{
    m_core->writePlainText(text);
}

void QTermTerminal::setTerminalSize(int columns, int rows)
{
    const int boundedColumns = qMax(columns, 2);
    const int boundedRows = qMax(rows, 1);
    if (boundedColumns == m_core->columns() && boundedRows == m_core->rows()) {
        return;
    }

    m_selectionModel->prepareForResize(m_core->buffer());
    m_core->setTerminalSize(columns, rows);
}

void QTermTerminal::clearSelection()
{
    if (!m_selectionModel->snapshot().hasSelection) {
        return;
    }

    m_selectionModel->clearSelection();
    syncSurfaceSelection();
}

void QTermTerminal::setSelectionRange(int startRow, int startColumn, int endRow, int endColumn)
{
    const QTermSelectionSnapshot previousSnapshot = m_selectionModel->snapshot();
    m_selectionModel->setSelectionRange(startRow, startColumn, endRow, endColumn);
    m_selectionModel->refreshSelectionText(m_core->buffer());
    const QTermSelectionSnapshot &currentSnapshot = m_selectionModel->snapshot();
    if (previousSnapshot.hasSelection == currentSnapshot.hasSelection &&
        previousSnapshot.startRow == currentSnapshot.startRow &&
        previousSnapshot.startColumn == currentSnapshot.startColumn &&
        previousSnapshot.endRow == currentSnapshot.endRow &&
        previousSnapshot.endColumn == currentSnapshot.endColumn &&
        previousSnapshot.selectedText == currentSnapshot.selectedText) {
        return;
    }

    syncSurfaceSelection();
}

void QTermTerminal::selectWordAt(int row, int column)
{
    m_selectionModel->selectWordAt(m_core->buffer(), row, column);
    m_selectionModel->refreshSelectionText(m_core->buffer());
    syncSurfaceSelection();
}

void QTermTerminal::selectLogicalLineAt(int row)
{
    m_selectionModel->selectLogicalLineAt(m_core->buffer(), row);
    m_selectionModel->refreshSelectionText(m_core->buffer());
    syncSurfaceSelection();
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

void QTermTerminal::syncSurfaceSelection()
{
    const QTermSelectionSnapshot &snapshot = m_selectionModel->snapshot();
    m_surfaceModel.setSelectionSnapshot(snapshot.hasSelection,
                                        snapshot.startRow,
                                        snapshot.startColumn,
                                        snapshot.endRow,
                                        snapshot.endColumn,
                                        snapshot.selectedText);
}

} // namespace QTerm