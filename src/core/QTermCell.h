#ifndef QTERM_QTERMCELL_H
#define QTERM_QTERMCELL_H

#include <QtGlobal>

#include <type_traits>

namespace QTerm {

struct QTermCellAttributes
{
    bool bold = false;
    bool dim = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;
    bool inverse = false;
    int foregroundIndex = -1;
    int backgroundIndex = -1;
    int foregroundRgb = -1;
    int backgroundRgb = -1;
    // OSC 8 hyperlink: 0 means no link; positive int is an index into the
    // core's hyperlink URL table.
    int hyperlinkId = 0;
};

/*
    One grid cell.

    The character is stored as a bare code point rather than a QString so the
    struct stays trivially constructible and destructible: a line's cell array
    can then be created and cleared in bulk instead of running a constructor per
    column, which dominated the cost of scrolling.

    A cell that carries combining marks cannot be expressed in a single code
    point. Those are rare, so instead of widening every cell the whole grapheme
    is kept in a side table owned by the line and referenced by combiningId.
    Use QTermLine::textAt() to read a cell's text; it resolves both cases.
*/
struct QTermCell
{
    char32_t codepoint = 0;   // 0 means blank
    quint16 combiningId = 0;  // 0 means "no combining marks"; else a line-side-table key
    quint8 width = 1;
    quint8 continuation = 0;  // trailing half of a double-width character
    QTermCellAttributes attributes;

    bool isBlank() const noexcept { return codepoint == 0 && combiningId == 0; }
};

// The whole point of the representation: cells must stay cheap to create and
// destroy in bulk, so a line's array can be built and cleared without running
// per-element constructors.
static_assert(std::is_trivially_copyable_v<QTermCell>,
              "QTermCell must stay trivially copyable");
static_assert(std::is_trivially_destructible_v<QTermCell>,
              "QTermCell must stay trivially destructible");
static_assert(sizeof(QTermCell) <= 40, "QTermCell grew unexpectedly");

} // namespace QTerm

#endif // QTERM_QTERMCELL_H
