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
        breakPredecessorWrapOnWrite = false;
        cursorOnWrapTarget = false;
        currentAttributes = QTermCellAttributes();
        savedCursorState.reset();
        scrollTop = 0;
        scrollBottom = buffer.rows() - 1;
        lineDrawingMode = false;
    }

    void resize(int columns, int rows)
    {
        const int oldRows = buffer.rows();
        // If the scroll region covered the full screen before resize, keep it
        // covering the full screen afterwards. qBound alone would leave
        // scrollBottom at its old value when the terminal grows (e.g. 23 stays
        // 23 even though the buffer now has 43 rows), causing content to scroll
        // inside the original 24-row region while the lower half stays blank.
        const bool fullScreenRegion = (scrollTop == 0 && scrollBottom == oldRows - 1);
        cursorState = buffer.resize(columns, rows, cursorState);
        cursorOnWrapTarget = false;  // Reflow repositions cursor; invalidate auto-wrap tracking.
        if (savedCursorState.has_value()) {
            savedCursorState->cursorState.row = qBound(0, savedCursorState->cursorState.row, rows - 1);
            savedCursorState->cursorState.column = qBound(0, savedCursorState->cursorState.column, columns - 1);
        }
        if (fullScreenRegion) {
            scrollTop = 0;
            scrollBottom = rows - 1;
        } else {
            scrollTop = qBound(0, scrollTop, rows - 1);
            scrollBottom = qBound(0, scrollBottom, rows - 1);
            if (scrollBottom < scrollTop) {
                scrollTop = 0;
                scrollBottom = rows - 1;
            }
        }
    }

    QTermBuffer buffer;
    QTermCursorState cursorState;
    bool wrapPending = false;
    bool breakPredecessorWrapOnWrite = false;
    // True when the cursor was most recently placed on this row by wrapToNextLine().
    // Cleared on any explicit cursor-row change (cursor commands, resize, LF, etc.).
    // Used to suppress predecessor-chain severing when CR/CHA fires after auto-wrap.
    bool cursorOnWrapTarget = false;
    QTermCellAttributes currentAttributes;
    std::optional<QTermSavedCursorState> savedCursorState;
    int scrollTop = 0;
    int scrollBottom = 23;
    bool lineDrawingMode = false;
};

} // namespace QTerm

#endif // QTERM_QTERMSCREENSTATE_H