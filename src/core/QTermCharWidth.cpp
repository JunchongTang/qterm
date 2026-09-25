#include "QTermCharWidth.h"

#include <QChar>

namespace QTerm {
namespace CharWidth {

// Decodes the leading code point without allocating. toUcs4() builds a whole
// QList for what is almost always a single character, and on CJK-heavy output
// that allocation dominates the parse.
char32_t leadingCodePoint(QStringView text, qsizetype *unitsConsumed)
{
    if (text.isEmpty()) {
        if (unitsConsumed)
            *unitsConsumed = 0;
        return 0;
    }

    const QChar first = text.front();
    if (first.isHighSurrogate() && text.size() > 1 && text.at(1).isLowSurrogate()) {
        if (unitsConsumed)
            *unitsConsumed = 2;
        return QChar::surrogateToUcs4(first, text.at(1));
    }

    if (unitsConsumed)
        *unitsConsumed = 1;
    return first.unicode();
}

bool isCombiningMark(QStringView text)
{
    qsizetype consumed = 0;
    const char32_t codePoint = leadingCodePoint(text, &consumed);
    // Only a lone mark counts; a base plus its marks is handled elsewhere.
    if (codePoint == 0 || consumed != text.size()) {
        return false;
    }

    switch (QChar::category(codePoint)) {
    case QChar::Mark_NonSpacing:
    case QChar::Mark_SpacingCombining:
    case QChar::Mark_Enclosing:
        return true;
    default:
        return false;
    }
}

bool isWide(char32_t codePoint)
{
    return (codePoint >= 0x1100 && codePoint <= 0x115f) ||
           codePoint == 0x2329 ||
           codePoint == 0x232a ||
           (codePoint >= 0x2e80 && codePoint <= 0xa4cf) ||
           (codePoint >= 0xac00 && codePoint <= 0xd7a3) ||
           (codePoint >= 0xf900 && codePoint <= 0xfaff) ||
           (codePoint >= 0xfe10 && codePoint <= 0xfe19) ||
           (codePoint >= 0xfe30 && codePoint <= 0xfe6f) ||
           (codePoint >= 0xff00 && codePoint <= 0xff60) ||
           (codePoint >= 0xffe0 && codePoint <= 0xffe6) ||
           (codePoint >= 0x1f300 && codePoint <= 0x1faff) ||
           (codePoint >= 0x20000 && codePoint <= 0x3fffd);
}

int displayWidth(QStringView text)
{
    const char32_t codePoint = leadingCodePoint(text);
    if (codePoint == 0) {
        return 0;
    }

    if (isCombiningMark(text)) {
        return 0;
    }

    return isWide(codePoint) ? 2 : 1;
}

} // namespace CharWidth
} // namespace QTerm
