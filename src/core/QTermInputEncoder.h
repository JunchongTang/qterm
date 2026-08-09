#ifndef QTERM_QTERMINPUTENCODER_H
#define QTERM_QTERMINPUTENCODER_H

#include <QByteArray>
#include <Qt>
#include <QString>

#include <QTerm/QTermModeState.h>

namespace QTerm {

class QTermInputEncoder
{
public:
    /*
        Encodes a key press for the child process.

        \a modifiers is appended rather than inserted so existing callers keep
        working; it is required for anything beyond an unmodified key, since
        xterm carries the modifier state inside the sequence itself
        (CSI 1 ; 5 C for Ctrl+Right) rather than as a separate prefix.

        \a text is what the platform produced for the key. It is preferred for
        ordinary characters because the keyboard layout has already been
        applied; the key code is only consulted where the layout is irrelevant
        (named keys) or where the platform withholds the text.
    */
    static QByteArray encodeKey(const QTermModeState &modeState, int key,
                                const QString &text = QString(),
                                Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    static QByteArray encodePaste(const QTermModeState &modeState, const QString &text);
    static QByteArray encodeMouse(int row, int column, Qt::MouseButton button,
                                  Qt::KeyboardModifiers modifiers, bool isPress,
                                  const QTermModeState &modeState,
                                  bool isMotion = false);
};

} // namespace QTerm

#endif // QTERM_QTERMINPUTENCODER_H