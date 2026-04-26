#ifndef QTERM_QTERMINPUTENCODER_H
#define QTERM_QTERMINPUTENCODER_H

#include <QByteArray>
#include <Qt>
#include <QString>

#include "QTermModeState.h"

namespace QTerm {

class QTermInputEncoder
{
public:
    static QByteArray encodeKey(const QTermModeState &modeState, int key, const QString &text = QString());
    static QByteArray encodePaste(const QTermModeState &modeState, const QString &text);
    static QByteArray encodeMouse(int row, int column, Qt::MouseButton button,
                                  Qt::KeyboardModifiers modifiers, bool isPress,
                                  const QTermModeState &modeState,
                                  bool isMotion = false);
};

} // namespace QTerm

#endif // QTERM_QTERMINPUTENCODER_H