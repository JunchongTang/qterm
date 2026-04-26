#ifndef QTERM_QTERMMODESTATE_H
#define QTERM_QTERMMODESTATE_H

namespace QTerm {

// 鼠标模式枚举（对应 DEC private mode numbers）
enum class MouseMode : int {
    Disabled = 0,      // 无鼠标
    X10 = 1000,        // ?1000 - X10 basic mouse
    Button = 1002,     // ?1002 - button-event mouse
    AnyEvent = 1003,   // ?1003 - any-event mouse
    UTF8 = 1005,       // ?1005 - UTF-8 mouse encoding
    SGR = 1006,        // ?1006 - SGR extended mouse
    URXVT = 1015       // ?1015 - URXVT mouse
};

struct QTermModeState
{
    bool cursorVisible = true;
    bool applicationCursorKeys = false;
    bool applicationKeypad = false;
    bool autoWrap = true;
    bool bracketedPaste = false;
    bool alternateScreenActive = false;
    MouseMode mouseMode = MouseMode::Disabled;
};

} // namespace QTerm

#endif // QTERM_QTERMMODESTATE_H
