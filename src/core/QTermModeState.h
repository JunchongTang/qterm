#ifndef QTERM_QTERMMODESTATE_H
#define QTERM_QTERMMODESTATE_H

namespace QTerm {

struct QTermModeState
{
    bool cursorVisible = true;
    bool applicationCursorKeys = false;
    bool bracketedPaste = false;
    bool alternateScreenActive = false;
};

} // namespace QTerm

#endif // QTERM_QTERMMODESTATE_H