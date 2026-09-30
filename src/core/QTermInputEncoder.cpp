#include "QTermInputEncoder.h"

#include <Qt>
#include <cmath>

namespace {

/*
    Which physical modifiers the terminal should act on.

    Qt swaps Control and Meta on macOS: the physical Control key arrives as
    Qt::MetaModifier and Command arrives as Qt::ControlModifier. Reading
    Qt::ControlModifier directly would therefore treat Command as the terminal's
    Ctrl, so Cmd+C would send an interrupt instead of copying. Command is left
    out entirely -- on macOS it belongs to the application's own shortcuts.
*/
struct TerminalModifiers
{
    bool shift = false;
    bool alt = false;
    bool control = false;
    bool keypad = false;
};

TerminalModifiers terminalModifiers(Qt::KeyboardModifiers modifiers)
{
    TerminalModifiers result;
    result.shift = modifiers.testFlag(Qt::ShiftModifier);
    result.alt = modifiers.testFlag(Qt::AltModifier);
    result.keypad = modifiers.testFlag(Qt::KeypadModifier);
#if defined(Q_OS_MACOS)
    result.control = modifiers.testFlag(Qt::MetaModifier);
#else
    result.control = modifiers.testFlag(Qt::ControlModifier);
#endif
    return result;
}

/*
    xterm's modifier parameter: a bit set, biased by one so that an unmodified
    key can be told apart by the absence of the parameter rather than by a
    magic value. Returns 0 when no modifier is held, meaning "omit it".
*/
int modifierParameter(const TerminalModifiers &modifiers)
{
    int bits = 0;
    if (modifiers.shift)   bits |= 1;
    if (modifiers.alt)     bits |= 2;
    if (modifiers.control) bits |= 4;
    return bits ? bits + 1 : 0;
}

/*
    Cursor keys, Home/End and F1-F4 all share this shape: a bare final byte
    normally, an SS3 introducer in application mode, and a CSI form carrying the
    modifier parameter as soon as anything is held. The modified form is CSI
    even in application mode -- SS3 has nowhere to put parameters.
*/
QByteArray cursorStyleSequence(char finalByte, int modifier, bool applicationMode)
{
    if (modifier > 0) {
        return QByteArray("\x1b[1;") + QByteArray::number(modifier) + finalByte;
    }
    return applicationMode ? QByteArray("\x1bO") + finalByte
                           : QByteArray("\x1b[") + finalByte;
}

// The VT220 editing and function keys: CSI Ps ~, with the modifier as a second
// parameter.
QByteArray tildeSequence(int code, int modifier)
{
    QByteArray sequence = QByteArray("\x1b[") + QByteArray::number(code);
    if (modifier > 0) {
        sequence += ';' + QByteArray::number(modifier);
    }
    return sequence + '~';
}

// F5-F12. The numbering skips 16 and 22 because the VT220 had no keys there.
int functionKeyTildeCode(int key)
{
    switch (key) {
    case Qt::Key_F5:  return 15;
    case Qt::Key_F6:  return 17;
    case Qt::Key_F7:  return 18;
    case Qt::Key_F8:  return 19;
    case Qt::Key_F9:  return 20;
    case Qt::Key_F10: return 21;
    case Qt::Key_F11: return 23;
    case Qt::Key_F12: return 24;
    default:          return 0;
    }
}

/*
    Application keypad (DECKPAM): the numeric keypad sends SS3 sequences so a
    program can tell it from the number row. Only reachable when the platform
    marks the event as coming from the keypad, which is why the main row keeps
    sending plain digits.
*/
char applicationKeypadFinal(int key)
{
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return char('p' + (key - Qt::Key_0));
    }
    switch (key) {
    case Qt::Key_Asterisk: return 'j';
    case Qt::Key_Plus:     return 'k';
    case Qt::Key_Comma:    return 'l';
    case Qt::Key_Minus:    return 'm';
    case Qt::Key_Period:   return 'n';
    case Qt::Key_Slash:    return 'o';
    case Qt::Key_Enter:    return 'M';
    case Qt::Key_Equal:    return 'X';
    default:               return '\0';
    }
}

/*
    Alt is transmitted as an ESC prefix on the *unmodified* character rather
    than on whatever the platform composed. On macOS Option+b produces "∫", and
    prefixing that would send a character the shell never asked for; readline
    expects ESC b.
*/
QByteArray altPrefixedCharacter(int key, const TerminalModifiers &modifiers)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        const char letter = char(key - Qt::Key_A + (modifiers.shift ? 'A' : 'a'));
        return QByteArray("\x1b") + letter;
    }
    if (key >= Qt::Key_Space && key <= Qt::Key_AsciiTilde) {
        return QByteArray("\x1b") + char(key);
    }
    return QByteArray();
}

// Ctrl+letter maps onto the C0 range; the punctuation cases matter in practice
// (Ctrl+] escapes telnet, Ctrl+\ raises SIGQUIT).
QByteArray controlCharacter(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return QByteArray(1, char(key - Qt::Key_A + 1));
    }
    switch (key) {
    case Qt::Key_Space:
    case Qt::Key_At:         return QByteArray(1, '\0');
    case Qt::Key_BracketLeft:  return QByteArray(1, '\x1b');
    case Qt::Key_Backslash:    return QByteArray(1, '\x1c');
    case Qt::Key_BracketRight: return QByteArray(1, '\x1d');
    case Qt::Key_AsciiCircum:  return QByteArray(1, '\x1e');
    case Qt::Key_Underscore:
    case Qt::Key_Question:     return QByteArray(1, '\x1f');
    default:                   return QByteArray();
    }
}

} // namespace

namespace QTerm {

QByteArray QTermInputEncoder::encodeKey(const QTermModeState &modeState, int key,
                                        const QString &text, Qt::KeyboardModifiers rawModifiers)
{
    const TerminalModifiers modifiers = terminalModifiers(rawModifiers);
    const int modifier = modifierParameter(modifiers);
    const bool applicationCursor = modeState.applicationCursorKeys;

    // Keypad first: in application mode the keypad digits must not fall through
    // to the plain characters the platform put in `text`.
    if (modifiers.keypad && modeState.applicationKeypad) {
        if (const char finalByte = applicationKeypadFinal(key)) {
            return QByteArray("\x1bO") + finalByte;
        }
    }

    switch (key) {
    case Qt::Key_Up:
        return cursorStyleSequence('A', modifier, applicationCursor);
    case Qt::Key_Down:
        return cursorStyleSequence('B', modifier, applicationCursor);
    case Qt::Key_Right:
        return cursorStyleSequence('C', modifier, applicationCursor);
    case Qt::Key_Left:
        return cursorStyleSequence('D', modifier, applicationCursor);
    case Qt::Key_Home:
        return cursorStyleSequence('H', modifier, applicationCursor);
    case Qt::Key_End:
        return cursorStyleSequence('F', modifier, applicationCursor);

    // F1-F4 keep the SS3 introducer whatever the cursor-key mode is; only F5
    // upwards moved to the tilde series.
    case Qt::Key_F1:
        return cursorStyleSequence('P', modifier, true);
    case Qt::Key_F2:
        return cursorStyleSequence('Q', modifier, true);
    case Qt::Key_F3:
        return cursorStyleSequence('R', modifier, true);
    case Qt::Key_F4:
        return cursorStyleSequence('S', modifier, true);
    case Qt::Key_F5:
    case Qt::Key_F6:
    case Qt::Key_F7:
    case Qt::Key_F8:
    case Qt::Key_F9:
    case Qt::Key_F10:
    case Qt::Key_F11:
    case Qt::Key_F12:
        return tildeSequence(functionKeyTildeCode(key), modifier);

    case Qt::Key_Insert:
        return tildeSequence(2, modifier);
    case Qt::Key_Delete:
        return tildeSequence(3, modifier);
    case Qt::Key_PageUp:
        return tildeSequence(5, modifier);
    case Qt::Key_PageDown:
        return tildeSequence(6, modifier);

    case Qt::Key_Return:
    case Qt::Key_Enter:
        return QByteArray("\r");
    case Qt::Key_Backspace:
        // Alt+Backspace is readline's delete-previous-word.
        return modifiers.alt ? QByteArray("\x1b\x7f") : QByteArray("\x7f");
    case Qt::Key_Tab:
        return QByteArray("\t");
    // Qt reports Shift+Tab as its own key rather than as a modified Tab, so the
    // modifier is already accounted for and must not be encoded again.
    case Qt::Key_Backtab:
        return QByteArray("\x1b[Z");
    case Qt::Key_Escape:
        return QByteArray("\x1b");

    default:
        break;
    }

    if (modifiers.control) {
        // On macOS, Cocoa intercepts Ctrl+letter combinations that match Emacs
        // text-editing shortcuts (Ctrl+B = moveBackward:, Ctrl+F = moveForward:)
        // at the NSTextInputClient level, leaving text() empty. Deriving the
        // control code from the key code covers that.
        const QByteArray control = controlCharacter(key);
        if (!control.isEmpty()) {
            return modifiers.alt ? QByteArray("\x1b") + control : control;
        }
    }

    if (modifiers.alt) {
        const QByteArray prefixed = altPrefixedCharacter(key, modifiers);
        if (!prefixed.isEmpty()) {
            return prefixed;
        }
    }

    // Ordinary text: the platform has already applied the keyboard layout.
    if (!text.isEmpty()) {
        return text.toUtf8();
    }

    // No text and no modifier we recognise: fall back to the control code, which
    // is the case macOS leaves us in when it swallows Ctrl+letter without
    // reporting the modifier at all.
    return controlCharacter(key);
}

QByteArray QTermInputEncoder::encodePaste(const QTermModeState &modeState, const QString &text)
{
    const QByteArray utf8 = text.toUtf8();
    if (!modeState.bracketedPaste) {
        return utf8;
    }

    return QByteArray("\x1b[200~") + utf8 + QByteArray("\x1b[201~");
}

QByteArray QTermInputEncoder::encodeMouse(int row, int column, Qt::MouseButton button,
                                           Qt::KeyboardModifiers modifiers, bool isPress,
                                           const QTermModeState &modeState, bool isMotion)
{
    // Nothing to encode while mouse tracking is off.
    if (modeState.mouseTracking == MouseTracking::Disabled) {
        return QByteArray();
    }

    // Base button code, independent of press versus motion.
    int buttonCode;
    switch (button) {
    case Qt::LeftButton:   buttonCode = 0; break;
    case Qt::MiddleButton: buttonCode = 1; break;
    case Qt::RightButton:  buttonCode = 2; break;
    case Qt::NoButton:     buttonCode = 3; break;
    default:
        if (static_cast<int>(button) == 64)       buttonCode = 64;  // wheel up
        else if (static_cast<int>(button) == 65)  buttonCode = 65;  // wheel down
        else                                       buttonCode = 3;
        break;
    }

    // Motion (a drag or a hover) adds 32 to the button code:
    //   3 (no button) + 32 = 35, motion with nothing held
    //   0 (left)      + 32 = 32, a left-button drag
    if (isMotion) {
        buttonCode += 32;
    }

    // The X10 format cannot say which button was released, so every release is
    // reported as button 3. SGR and URXVT keep the real code and tell a press
    // from a release with the final M or m.
    const bool isSGRFamily = (modeState.mouseEncoding == MouseEncoding::SGR ||
                              modeState.mouseEncoding == MouseEncoding::URXVT);
    if (!isPress && !isMotion && !isSGRFamily) {
        buttonCode = 3;
    }

    // Modifiers: Shift adds 4, Ctrl 8, Alt 16.
    int modifierCode = 0;
    if (modifiers & Qt::ShiftModifier) {
        modifierCode += 4;
    }
    if (modifiers & Qt::ControlModifier) {
        modifierCode += 8;
    }
    if (modifiers & Qt::AltModifier) {
        modifierCode += 16;
    }

    const int x = column + 1;
    const int y = row + 1;

    // SGR extended format (?1006)
    if (modeState.mouseEncoding == MouseEncoding::SGR) {
        // ESC [ < button ; x ; y M/m
        return QByteArray("\x1b[<") + QByteArray::number(buttonCode + modifierCode) +
               ';' + QByteArray::number(x) +
               ';' + QByteArray::number(y) +
               (isPress ? 'M' : 'm');
    }

    // URXVT format (?1015)
    if (modeState.mouseEncoding == MouseEncoding::URXVT) {
        // ESC [ button ; x ; y M
        return QByteArray("\x1b[") + QByteArray::number(buttonCode + modifierCode) +
               ';' + QByteArray::number(x) +
               ';' + QByteArray::number(y) +
               'M';
    }

    // Base X10 / Button / AnyEvent formats (?1000, ?1002, ?1003)
    // ESC [ M <button> <x> <y>
    // Button and coordinates are single bytes, so each is offset by 33 to keep
    // it in printable ASCII: x = column + 33, y = row + 33 (the VT100 encoding).
    const unsigned char buttonByte = static_cast<unsigned char>(buttonCode + modifierCode + 0x20);
    const unsigned char xByte = static_cast<unsigned char>(std::min(255, column + 33));
    const unsigned char yByte = static_cast<unsigned char>(std::min(255, row + 33));

    return QByteArray("\x1b[M") + QByteArray(reinterpret_cast<const char *>(&buttonByte), 1) +
           QByteArray(reinterpret_cast<const char *>(&xByte), 1) +
           QByteArray(reinterpret_cast<const char *>(&yByte), 1);
}

} // namespace QTerm