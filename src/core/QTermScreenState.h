#ifndef QTERM_QTERMSCREENSTATE_H
#define QTERM_QTERMSCREENSTATE_H

#include <optional>

#include <QtGlobal>

#include "QTermBuffer.h"
#include "QTermSavedCursorState.h"

namespace QTerm {

struct QTermScreenState
{
    explicit QTermScreenState(int columns = 80, int rows = 24)
        : buffer(columns, rows)
        , scrollBottom(rows - 1)
    {
    }

    void clear()
    {
        buffer.clear();
        cursorState = QTermCursorState();
        wrapPending = false;
        currentAttributes = QTermCellAttributes();
        savedCursorState.reset();
        scrollTop = 0;
        scrollBottom = buffer.rows() - 1;
    }

    void resize(int columns, int rows)
    {
        buffer.resize(columns, rows);
        cursorState.row = qBound(0, cursorState.row, rows - 1);
        cursorState.column = qBound(0, cursorState.column, columns - 1);
        if (savedCursorState.has_value()) {
            savedCursorState->cursorState.row = qBound(0, savedCursorState->cursorState.row, rows - 1);
            savedCursorState->cursorState.column = qBound(0, savedCursorState->cursorState.column, columns - 1);
        }
        scrollTop = qBound(0, scrollTop, rows - 1);
        scrollBottom = qBound(0, scrollBottom, rows - 1);
        if (scrollBottom < scrollTop) {
            scrollTop = 0;
            scrollBottom = rows - 1;
        }
    }

    QTermBuffer buffer;
    QTermCursorState cursorState;
    bool wrapPending = false;
    QTermCellAttributes currentAttributes;
    std::optional<QTermSavedCursorState> savedCursorState;
    int scrollTop = 0;
    int scrollBottom = 23;
};

} // namespace QTerm

#endif // QTERM_QTERMSCREENSTATE_H