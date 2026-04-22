#ifndef QTERM_QTERMSAVEDCURSORSTATE_H
#define QTERM_QTERMSAVEDCURSORSTATE_H

#include "QTermCell.h"
#include "QTermCursorState.h"

namespace QTerm {

struct QTermSavedCursorState
{
    QTermCursorState cursorState;
    QTermCellAttributes attributes;
};

} // namespace QTerm

#endif // QTERM_QTERMSAVEDCURSORSTATE_H