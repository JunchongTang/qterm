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
    /*!
        No shape has been requested by the program: the **host's** configured shape
        applies (`QTermQuickItem::cursorStyle` and friends).

        This is the initial state, and what `CSI 0 SP q` returns to -- DECSCUSR 0 means
        "back to the terminal's default", and the terminal's default is whatever the
        user configured, not a hardcoded block.
    */
    Default   = -1,
    Block     = 0,   /*!< Block cursor shape. */
    Underline = 1,   /*!< Underline cursor shape. */
    Bar       = 2,   /*!< Vertical bar (I-beam) cursor shape. */
};

/*!
    \brief Resolves which cursor shape to draw.

    \a fromTerminal is what the program asked for (`QTermSurfaceModel::cursorShape()`),
    \a hostDefault what the host configured. A program that never sent DECSCUSR leaves
    the former at \c CursorShape::Default (-1), and then the host's choice wins.

    **The rule lives here because all three renderers need it and they disagreed once:**
    the two Quick items used to read the terminal's shape unconditionally -- so the
    documented `cursorStyle` property did nothing whenever a terminal was attached,
    which is always -- while the widget read only its own property and ignored DECSCUSR
    entirely. A cursor-shape preference that silently does nothing is worse than not
    offering one.
*/
inline int resolveCursorShape(int fromTerminal, int hostDefault) noexcept
{
    return fromTerminal >= 0 ? fromTerminal : hostDefault;
}

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
    /*!
        Cursor shape requested via DECSCUSR. Starts at \c CursorShape::Default, meaning
        the program has not asked for one and the host's configured shape applies.
    */
    CursorShape cursorShape = CursorShape::Default;
};

} // namespace QTerm

#endif // QTERM_QTERMMODESTATE_H
