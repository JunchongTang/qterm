#ifndef QTERM_QTERMSELECTIONGEOMETRY_H
#define QTERM_QTERMSELECTIONGEOMETRY_H

#include <QList>
#include <QtGlobal>

// Which cells of which rows a selection highlights -- the pure part of drawing a
// selection, kept apart from the scene graph so it can be tested.
//
// Why this is its own function instead of living inside `rebuildSelection`:
//
// The highlight is one quad (6 vertices) per selected row, written into a
// `QSGGeometry` that `allocate()` hands back **uninitialised**. The old code sized
// the buffer from `endRow - startRow + 1` and then wrote a quad only for rows that
// survived two further conditions (`selEnd > selStart`, `row < rows`). Any row that
// failed one of them left its 6 vertices unwritten, and `DrawTriangles` draws them
// anyway: they are whatever the heap held -- typically coordinates from the previous
// frame's quads, because a drag rebuilds the geometry on every mouse move. Mixed
// with real vertices they form skewed triangles: a wedge from the item's top-left
// corner across the rows, nothing like a rectangle.
//
// Two everyday selections hit it:
//   * a whole-line selection (triple-click, or a drag that ends at the start of the
//     next line) has `endColumn == 0` on its last row, so that row is empty;
//   * a selection left over from before a resize can point past the new `rows`.
//
// Deriving the buffer size from *this list* -- the same list the caller iterates to
// write vertices -- makes the two counts equal by construction.

namespace QTerm {
namespace Internal {

struct SelectionSpan
{
    int row = 0;
    int startColumn = 0; // inclusive
    int endColumn = 0;   // exclusive; always > startColumn
};

// One entry per row that actually has something to highlight, in increasing row
// order. Every returned span satisfies 0 <= row < rows and
// 0 <= startColumn < endColumn <= columns.
//
// `startRow`/`endRow` may lie outside [0, rows) (a stale selection, or one that
// began above the visible area); rows outside the grid are clipped away, and a
// clipped first row starts at column 0 -- it is no longer the row the selection
// began on, so `startColumn` must not apply to it.
inline QList<SelectionSpan> selectionSpans(int startRow, int startColumn,
                                           int endRow, int endColumn,
                                           int rows, int columns)
{
    QList<SelectionSpan> spans;
    for (int row = qMax(startRow, 0); row <= endRow && row < rows; ++row) {
        const int from = (row == startRow) ? startColumn : 0;
        const int to = (row == endRow) ? endColumn : columns;
        // Clamp columns too: a selection made before the terminal narrowed can hold a
        // column past the new width, and a quad wider than the item is not a highlight.
        const int start = qBound(0, from, columns);
        const int end = qBound(0, to, columns);
        if (end <= start)
            continue;
        spans.append({ row, start, end });
    }
    return spans;
}

} // namespace Internal
} // namespace QTerm

#endif // QTERM_QTERMSELECTIONGEOMETRY_H
