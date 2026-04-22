#include "QTermInputExecutor.h"

#include <QtGlobal>

namespace {

constexpr int kTabWidth = 8;

} // namespace

namespace QTerm {

QTermInputExecutor::QTermInputExecutor(QTermBuffer &buffer,
                                       const QTermCursorState &cursorState,
                                       bool wrapPending,
                                       const QTermCellAttributes &currentAttributes,
                                       const std::optional<QTermSavedCursorState> &savedCursorState,
                                       int scrollTop,
                                       int scrollBottom)
    : m_buffer(buffer)
    , m_cursorState(cursorState)
    , m_wrapPending(wrapPending)
    , m_currentAttributes(currentAttributes)
    , m_savedCursorState(savedCursorState)
    , m_scrollTop(qBound(0, scrollTop, buffer.rows() - 1))
    , m_scrollBottom(qBound(0, scrollBottom, buffer.rows() - 1))
{
    if (m_scrollBottom < m_scrollTop) {
        m_scrollTop = 0;
        m_scrollBottom = buffer.rows() - 1;
    }
}

QTermCursorState QTermInputExecutor::cursorState() const noexcept
{
    return m_cursorState;
}

bool QTermInputExecutor::wrapPending() const noexcept
{
    return m_wrapPending;
}

QTermCellAttributes QTermInputExecutor::currentAttributes() const noexcept
{
    return m_currentAttributes;
}

std::optional<QTermSavedCursorState> QTermInputExecutor::savedCursorState() const
{
    return m_savedCursorState;
}

int QTermInputExecutor::scrollTop() const noexcept
{
    return m_scrollTop;
}

int QTermInputExecutor::scrollBottom() const noexcept
{
    return m_scrollBottom;
}

int QTermInputExecutor::rows() const noexcept
{
    return m_buffer.rows();
}

void QTermInputExecutor::print(const QString &text)
{
    if (m_wrapPending) {
        wrapToNextLine();
    }

    m_buffer.lineAt(m_cursorState.row).setCell(m_cursorState.column, QTermCell{text, 1, false, m_currentAttributes});

    if (m_cursorState.column == m_buffer.columns() - 1) {
        m_wrapPending = true;
        return;
    }

    setCursorState(QTermCursorState{m_cursorState.row, m_cursorState.column + 1});
}

void QTermInputExecutor::lineFeed()
{
    m_wrapPending = false;
    m_buffer.lineAt(m_cursorState.row).setWrappedToNextLine(false);
    advanceToNextRow();
}

void QTermInputExecutor::carriageReturn()
{
    m_wrapPending = false;
    m_buffer.lineAt(m_cursorState.row).setWrappedToNextLine(false);
    setCursorState(QTermCursorState{m_cursorState.row, 0});
}

void QTermInputExecutor::backspace()
{
    if (m_wrapPending) {
        m_wrapPending = false;
        return;
    }

    setCursorState(QTermCursorState{m_cursorState.row, qMax(0, m_cursorState.column - 1)});
}

void QTermInputExecutor::horizontalTab()
{
    const int nextTabStop = ((m_cursorState.column / kTabWidth) + 1) * kTabWidth;
    const int targetColumn = qMax(nextTabStop, m_cursorState.column + 1);

    for (int column = m_cursorState.column; column < targetColumn; ++column) {
        if (m_wrapPending) {
            wrapToNextLine();
        } else if (m_cursorState.column == m_buffer.columns() - 1) {
            m_wrapPending = true;
        } else {
            setCursorState(QTermCursorState{m_cursorState.row, m_cursorState.column + 1});
        }
    }
}

void QTermInputExecutor::cursorUp(int count)
{
    m_wrapPending = false;
    setCursorState(QTermCursorState{qMax(0, m_cursorState.row - qMax(1, count)), m_cursorState.column});
}

void QTermInputExecutor::cursorDown(int count)
{
    m_wrapPending = false;
    setCursorState(QTermCursorState{qMin(m_buffer.rows() - 1, m_cursorState.row + qMax(1, count)), m_cursorState.column});
}

void QTermInputExecutor::cursorForward(int count)
{
    m_wrapPending = false;
    setCursorState(QTermCursorState{m_cursorState.row, qMin(m_buffer.columns() - 1, m_cursorState.column + qMax(1, count))});
}

void QTermInputExecutor::cursorBackward(int count)
{
    m_wrapPending = false;
    setCursorState(QTermCursorState{m_cursorState.row, qMax(0, m_cursorState.column - qMax(1, count))});
}

void QTermInputExecutor::cursorPosition(int row, int column)
{
    m_wrapPending = false;
    setCursorState(QTermCursorState{
        qBound(0, row, m_buffer.rows() - 1),
        qBound(0, column, m_buffer.columns() - 1)
    });
}

void QTermInputExecutor::eraseInLine(int mode)
{
    m_wrapPending = false;

    switch (mode) {
    case 1:
        m_buffer.lineAt(m_cursorState.row).clearToColumn(m_cursorState.column);
        break;
    case 2:
        m_buffer.lineAt(m_cursorState.row).clear();
        break;
    case 0:
    default:
        m_buffer.lineAt(m_cursorState.row).clearToEnd(m_cursorState.column);
        break;
    }
}

void QTermInputExecutor::eraseInDisplay(int mode)
{
    m_wrapPending = false;

    switch (mode) {
    case 1:
        m_buffer.clearVisibleTo(m_cursorState.row, m_cursorState.column);
        break;
    case 2:
        m_buffer.clearVisible();
        break;
    case 0:
    default:
        m_buffer.clearVisibleFrom(m_cursorState.row, m_cursorState.column);
        break;
    }
}

void QTermInputExecutor::characterAttributes(const QVector<int> &parameters)
{
    const QVector<int> normalized = parameters.isEmpty() ? QVector<int>{0} : parameters;

    for (int parameter : normalized) {
        switch (parameter) {
        case 0:
            m_currentAttributes = QTermCellAttributes();
            break;
        case 1:
            m_currentAttributes.bold = true;
            break;
        case 3:
            m_currentAttributes.italic = true;
            break;
        case 4:
            m_currentAttributes.underline = true;
            break;
        case 22:
            m_currentAttributes.bold = false;
            break;
        case 23:
            m_currentAttributes.italic = false;
            break;
        case 24:
            m_currentAttributes.underline = false;
            break;
        case 39:
            m_currentAttributes.foregroundIndex = -1;
            break;
        case 49:
            m_currentAttributes.backgroundIndex = -1;
            break;
        default:
            if (parameter >= 30 && parameter <= 37) {
                m_currentAttributes.foregroundIndex = parameter - 30;
            } else if (parameter >= 40 && parameter <= 47) {
                m_currentAttributes.backgroundIndex = parameter - 40;
            } else if (parameter >= 90 && parameter <= 97) {
                m_currentAttributes.foregroundIndex = 8 + (parameter - 90);
            } else if (parameter >= 100 && parameter <= 107) {
                m_currentAttributes.backgroundIndex = 8 + (parameter - 100);
            }
            break;
        }
    }
}

void QTermInputExecutor::insertCharacters(int count)
{
    m_wrapPending = false;
    m_buffer.lineAt(m_cursorState.row).insertCells(m_cursorState.column, qMax(1, count));
}

void QTermInputExecutor::deleteCharacters(int count)
{
    m_wrapPending = false;
    m_buffer.lineAt(m_cursorState.row).deleteCells(m_cursorState.column, qMax(1, count));
}

void QTermInputExecutor::insertLines(int count)
{
    m_wrapPending = false;
    m_buffer.insertLines(m_cursorState.row, qMax(1, count), m_scrollTop, m_scrollBottom);
}

void QTermInputExecutor::deleteLines(int count)
{
    m_wrapPending = false;
    m_buffer.deleteLines(m_cursorState.row, qMax(1, count), m_scrollTop, m_scrollBottom);
}

void QTermInputExecutor::setScrollRegion(int top, int bottom)
{
    const int normalizedTop = qBound(0, top, m_buffer.rows() - 1);
    const int normalizedBottom = qBound(0, bottom, m_buffer.rows() - 1);
    if (normalizedTop >= normalizedBottom) {
        m_scrollTop = 0;
        m_scrollBottom = m_buffer.rows() - 1;
    } else {
        m_scrollTop = normalizedTop;
        m_scrollBottom = normalizedBottom;
    }

    setCursorState(QTermCursorState{m_scrollTop, 0});
}

void QTermInputExecutor::saveCursor()
{
    m_savedCursorState = QTermSavedCursorState{m_cursorState, m_currentAttributes};
}

void QTermInputExecutor::restoreCursor()
{
    if (!m_savedCursorState.has_value()) {
        return;
    }

    m_currentAttributes = m_savedCursorState->attributes;
    setCursorState(m_savedCursorState->cursorState);
    m_wrapPending = false;
}

void QTermInputExecutor::setCursorState(const QTermCursorState &cursorState)
{
    m_cursorState = cursorState;
}

void QTermInputExecutor::wrapToNextLine()
{
    m_buffer.lineAt(m_cursorState.row).setWrappedToNextLine(true);
    m_wrapPending = false;
    advanceToNextRow();
    setCursorState(QTermCursorState{m_cursorState.row, 0});
}

void QTermInputExecutor::advanceToNextRow()
{
    if (m_cursorState.row == m_scrollBottom) {
        m_buffer.deleteLines(m_scrollTop, 1, m_scrollTop, m_scrollBottom);
        return;
    }

    if (m_cursorState.row == m_buffer.rows() - 1) {
        m_buffer.scrollUp();
        return;
    }

    setCursorState(QTermCursorState{m_cursorState.row + 1, m_cursorState.column});
}

} // namespace QTerm