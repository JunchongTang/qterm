#ifndef QTERMCHARWIDTH_H
#define QTERMCHARWIDTH_H

#include <QStringView>

// How many cells a character occupies. **One answer, used by everyone.**
//
// This used to live in an anonymous namespace inside QTermInputExecutor.cpp,
// which meant the renderer could not reach it -- and so the renderer grew its
// own guesses. Those guesses disagreed with the emulator, and a disagreement
// here does not look like a bug in the width code: it looks like "the columns
// of `ls` do not line up" (reported against a directory full of Chinese file
// names) or "the box drawing of a TUI is too wide".
//
// The rule: the emulator decides how many cells a character claims; the
// renderer must advance by exactly that much. Both sides call the functions
// below and nothing else.
namespace QTerm {
namespace CharWidth {

// Decodes the leading code point without allocating. toUcs4() builds a whole
// QList for what is almost always a single character, and on CJK-heavy output
// that allocation dominates the parse.
char32_t leadingCodePoint(QStringView text, qsizetype *unitsConsumed = nullptr);

// A lone combining mark; a base plus its marks is handled by the caller.
bool isCombiningMark(QStringView text);

// East Asian Wide / Fullwidth, plus the emoji planes -- the usual wcwidth==2 set.
bool isWide(char32_t codePoint);

// 0 for a combining mark, 2 for a wide code point, 1 otherwise.
int displayWidth(QStringView text);

} // namespace CharWidth
} // namespace QTerm

#endif // QTERMCHARWIDTH_H
