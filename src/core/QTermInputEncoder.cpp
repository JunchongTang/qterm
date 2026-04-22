#include "QTermInputEncoder.h"

#include <Qt>

namespace {

QByteArray applicationOrNormalCursorSequence(bool applicationCursorKeys, char normalFinal, char applicationFinal)
{
    return applicationCursorKeys
        ? QByteArray("\x1bO") + applicationFinal
        : QByteArray("\x1b[") + normalFinal;
}

} // namespace

namespace QTerm {

QByteArray QTermInputEncoder::encodeKey(const QTermModeState &modeState, int key, const QString &text)
{
    switch (key) {
    case Qt::Key_Up:
        return applicationOrNormalCursorSequence(modeState.applicationCursorKeys, 'A', 'A');
    case Qt::Key_Down:
        return applicationOrNormalCursorSequence(modeState.applicationCursorKeys, 'B', 'B');
    case Qt::Key_Right:
        return applicationOrNormalCursorSequence(modeState.applicationCursorKeys, 'C', 'C');
    case Qt::Key_Left:
        return applicationOrNormalCursorSequence(modeState.applicationCursorKeys, 'D', 'D');
    case Qt::Key_Home:
        return applicationOrNormalCursorSequence(modeState.applicationCursorKeys, 'H', 'H');
    case Qt::Key_End:
        return applicationOrNormalCursorSequence(modeState.applicationCursorKeys, 'F', 'F');
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return QByteArray("\r");
    case Qt::Key_Backspace:
        return QByteArray("\x7f");
    case Qt::Key_Tab:
        return QByteArray("\t");
    case Qt::Key_Escape:
        return QByteArray("\x1b");
    default:
        return text.toUtf8();
    }
}

QByteArray QTermInputEncoder::encodePaste(const QTermModeState &modeState, const QString &text)
{
    const QByteArray utf8 = text.toUtf8();
    if (!modeState.bracketedPaste) {
        return utf8;
    }

    return QByteArray("\x1b[200~") + utf8 + QByteArray("\x1b[201~");
}

} // namespace QTerm