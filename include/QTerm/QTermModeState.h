#ifndef QTERM_QTERMMODESTATE_H
#define QTERM_QTERMMODESTATE_H

namespace QTerm {

// 鼠标事件类型（?1000/?1002/?1003）：控制上报哪些事件
enum class MouseTracking : int {
    Disabled  = 0,     // 无鼠标上报
    X10       = 1000,  // ?1000 — 仅上报按下
    Button    = 1002,  // ?1002 — 按下 + 拖拽
    AnyEvent  = 1003,  // ?1003 — 所有移动
};

// 鼠标编码格式（?1006/?1015）：控制坐标如何编码
enum class MouseEncoding : int {
    Default = 0,    // X10 单字节（最大坐标 223）
    SGR     = 1006, // ?1006 — ESC[<btn;x;yM/m，无大小限制
    URXVT   = 1015, // ?1015 — ESC[btn;x;yM
};

// DECSCUSR 光标形状（CSI n SP q）
// 0/1 = blinking block, 2 = steady block, 3 = blinking underline,
// 4 = steady underline, 5 = blinking bar, 6 = steady bar.
// We map blink/steady pairs to the same shape; blink is handled by the UI.
enum class CursorShape : int {
    Block     = 0,   // default / 0 / 1 / 2
    Underline = 1,   // 3 / 4
    Bar       = 2,   // 5 / 6 (I-beam)
};

struct QTermModeState
{
    bool cursorVisible = true;
    bool applicationCursorKeys = false;
    bool applicationKeypad = false;
    bool autoWrap = true;
    bool bracketedPaste = false;
    bool alternateScreenActive = false;
    MouseTracking mouseTracking = MouseTracking::Disabled;
    MouseEncoding mouseEncoding = MouseEncoding::Default;
    // OSC 8 hyperlink: 0 = none active; positive = index into core URL table
    int activeHyperlinkId = 0;
    // DECSCUSR cursor shape (CSI n SP q)
    CursorShape cursorShape = CursorShape::Block;
};

} // namespace QTerm

#endif // QTERM_QTERMMODESTATE_H
