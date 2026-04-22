#include "QTermCore.h"

#include <QtGlobal>

namespace {

constexpr int kMinimumColumns = 2;
constexpr int kMinimumRows = 1;
constexpr int kTabWidth = 8;

} // namespace

namespace QTerm {

QTermCore::QTermCore(QObject *parent)
    : QObject(parent)
    , m_buffer(80, 24)
{
}

int QTermCore::rows() const noexcept
{
    return m_buffer.rows();
}

int QTermCore::columns() const noexcept
{
    return m_buffer.columns();
}

QString QTermCore::debugPlainText() const
{
    return m_buffer.debugPlainText();
}

QTermCursorState QTermCore::cursorState() const noexcept
{
    return m_cursorState;
}

const QTermBuffer &QTermCore::buffer() const noexcept
{
    return m_buffer;
}

void QTermCore::clear()
{
    m_buffer.clear();
    m_wrapPending = false;
    m_currentAttributes = QTermCellAttributes();
    m_savedCursorState.reset();
    m_scrollTop = 0;
    m_scrollBottom = rows() - 1;
    emit debugPlainTextChanged();
    setCursorState(QTermCursorState());
}

void QTermCore::writePlainText(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    QTermInputExecutor executor(m_buffer,
                                m_cursorState,
                                m_wrapPending,
                                m_currentAttributes,
                                m_savedCursorState,
                                m_scrollTop,
                                m_scrollBottom);
    m_textParser.parse(text, executor);
    m_wrapPending = executor.wrapPending();
    m_currentAttributes = executor.currentAttributes();
    m_savedCursorState = executor.savedCursorState();
    m_scrollTop = executor.scrollTop();
    m_scrollBottom = executor.scrollBottom();
    setCursorState(executor.cursorState());

    emit debugPlainTextChanged();
}

void QTermCore::setTerminalSize(int columns, int rows)
{
    const int boundedColumns = qMax(columns, kMinimumColumns);
    const int boundedRows = qMax(rows, kMinimumRows);
    if (m_buffer.columns() == boundedColumns && m_buffer.rows() == boundedRows) {
        return;
    }

    m_buffer.resize(boundedColumns, boundedRows);
    if (m_cursorState.row >= boundedRows) {
        m_cursorState.row = boundedRows - 1;
    }
    if (m_cursorState.column >= boundedColumns) {
        m_cursorState.column = boundedColumns - 1;
    }
    if (m_savedCursorState.has_value()) {
        m_savedCursorState->cursorState.row = qBound(0, m_savedCursorState->cursorState.row, boundedRows - 1);
        m_savedCursorState->cursorState.column = qBound(0, m_savedCursorState->cursorState.column, boundedColumns - 1);
    }
    m_scrollTop = qBound(0, m_scrollTop, boundedRows - 1);
    m_scrollBottom = qBound(0, m_scrollBottom, boundedRows - 1);
    if (m_scrollBottom < m_scrollTop) {
        m_scrollTop = 0;
        m_scrollBottom = boundedRows - 1;
    }
    emit sizeChanged();
    emit debugPlainTextChanged();
    emit cursorStateChanged();
}

void QTermCore::setCursorState(const QTermCursorState &cursorState)
{
    if (m_cursorState.row == cursorState.row && m_cursorState.column == cursorState.column) {
        return;
    }

    m_cursorState = cursorState;
    emit cursorStateChanged();
}

} // namespace QTerm