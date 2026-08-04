#ifndef QTERM_QTERMMODESTATE_H
#define QTERM_QTERMMODESTATE_H

namespace QTerm {

/*!
    \enum QTerm::MouseTracking
    \inmodule QTerm
    \brief Controls which mouse events are reported to terminal applications.
*/
enum class MouseTracking : int {
    Disabled  = 0,     /*!< Mouse reporting disabled. */
    X10       = 1000,  /*!< Report button press events only (DECSET 1000). */
    Button    = 1002,  /*!< Report button presses and drag motion (DECSET 1002). */
    AnyEvent  = 1003,  /*!< Report all pointer motion events (DECSET 1003). */
};

/*!
    \enum QTerm::MouseEncoding
    \inmodule QTerm
    \brief Controls how mouse coordinates are encoded in escape sequences.
*/
enum class MouseEncoding : int {
    Default = 0,    /*!< Legacy X10-style single-byte coordinate encoding. */
    SGR     = 1006, /*!< SGR mouse encoding (DECSET 1006). */
    URXVT   = 1015, /*!< URXVT mouse encoding (DECSET 1015). */
};

/*!
    \enum QTerm::CursorShape
    \inmodule QTerm
    \brief Cursor shapes mapped from DECSCUSR control sequences.
*/
enum class CursorShape : int {
    Block     = 0,   /*!< Block cursor shape. */
    Underline = 1,   /*!< Underline cursor shape. */
    Bar       = 2,   /*!< Vertical bar (I-beam) cursor shape. */
};

/*!
    \struct QTermModeState
    \inmodule QTerm
    \brief Aggregates terminal mode flags such as cursor visibility, mouse tracking and alternate screen state.
*/
struct QTermModeState
{
    bool cursorVisible = true;                    /*!< Whether the cursor should be rendered. */
    bool applicationCursorKeys = false;           /*!< Application cursor-key mode (DECCKM). */
    bool applicationKeypad = false;               /*!< Application keypad mode (DECKPAM/DECKPNM). */
    bool autoWrap = true;                         /*!< Autowrap mode at right margin (DECAWM). */
    bool bracketedPaste = false;                  /*!< Bracketed paste mode (DECSET 2004). */
    bool alternateScreenActive = false;           /*!< Whether the alternate screen buffer is active. */
    MouseTracking mouseTracking = MouseTracking::Disabled; /*!< Current mouse tracking policy. */
    MouseEncoding mouseEncoding = MouseEncoding::Default;  /*!< Current mouse encoding mode. */
    int activeHyperlinkId = 0;                    /*!< Active OSC 8 hyperlink id; 0 means none. */
    CursorShape cursorShape = CursorShape::Block; /*!< Current cursor shape derived from DECSCUSR. */
};

} // namespace QTerm

#endif // QTERM_QTERMMODESTATE_H
