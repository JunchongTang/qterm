#include "QTermInputEncoder.h"

#include <Qt>
#include <cmath>

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

QByteArray QTermInputEncoder::encodeMouse(int row, int column, Qt::MouseButton button,
                                           Qt::KeyboardModifiers modifiers, bool isPress,
                                           const QTermModeState &modeState)
{
    // 鼠标模式禁用则不编码
    if (modeState.mouseMode == MouseMode::Disabled) {
        return QByteArray();
    }

    // 确定按钮码
    int buttonCode = 3;  // 默认为释放
    if (isPress) {
        switch (button) {
        case Qt::LeftButton:
            buttonCode = 0;
            break;
        case Qt::MiddleButton:
            buttonCode = 1;
            break;
        case Qt::RightButton:
            buttonCode = 2;
            break;
        case Qt::NoButton:
            // 特殊虚拟码：64=滚轮向上，65=滚轮向下
            buttonCode = 3;  // 默认移动事件
            break;
        default:
            // 检查虚拟按钮码：64 和 65 用于滚轮
            if (static_cast<int>(button) == 64) {
                buttonCode = 64;  // 滚轮向上
            } else if (static_cast<int>(button) == 65) {
                buttonCode = 65;  // 滚轮向下
            } else {
                buttonCode = 3;
            }
            break;
        }
    } else {
        // 松开时，使用 3（可选加修饰符）
        buttonCode = 3;
    }

    // 修饰符编码（Shift +4, Ctrl +8, Alt +16）
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

    // SGR 扩展格式 (?1006)
    if (modeState.mouseMode == MouseMode::SGR) {
        // ESC [ <button> ; <x> ; <y> M/m
        return QByteArray("\x1b[") + QByteArray::number(buttonCode + modifierCode) +
               ';' + QByteArray::number(x) +
               ';' + QByteArray::number(y) +
               (isPress ? 'M' : 'm');
    }

    // URXVT 格式 (?1015)
    if (modeState.mouseMode == MouseMode::URXVT) {
        // ESC [ <button> ; <x> ; <y> M
        return QByteArray("\x1b[") + QByteArray::number(buttonCode + modifierCode) +
               ';' + QByteArray::number(x) +
               ';' + QByteArray::number(y) +
               'M';
    }

    // 基础格式 X10/Button/AnyEvent (?1000, ?1002, ?1003)
    // ESC [ M <button> <x> <y>
    // button/x/y are 单字节，范围 0-255（通常映射到 ASCII）
    // x = column + 33, y = row + 33（VT100 编码）
    const unsigned char buttonByte = static_cast<unsigned char>(buttonCode + modifierCode + 0x20);
    const unsigned char xByte = static_cast<unsigned char>(std::min(255, column + 33));
    const unsigned char yByte = static_cast<unsigned char>(std::min(255, row + 33));

    return QByteArray("\x1b[M") + QByteArray(reinterpret_cast<const char *>(&buttonByte), 1) +
           QByteArray(reinterpret_cast<const char *>(&xByte), 1) +
           QByteArray(reinterpret_cast<const char *>(&yByte), 1);
}

} // namespace QTerm