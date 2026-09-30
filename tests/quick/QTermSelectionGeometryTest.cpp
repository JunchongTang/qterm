// Which cells a selection highlights: one span per row that has something to draw.
//
// The regression this guards: `rebuildSelection` sized its vertex buffer from
// `endRow - startRow + 1` but wrote a quad only for rows that also passed
// `selEnd > selStart` and `row < rows`. `QSGGeometry::allocate()` does not zero
// memory, so every skipped row left 6 vertices of heap garbage that `DrawTriangles`
// drew anyway -- skewed triangles across the rows instead of a rectangle. The scene
// graph node itself cannot be checked headlessly (offscreen rendering does not draw
// custom geometry nodes), so the row/column decision lives in a pure function and is
// tested here; the buffer is then sized from that same list, which makes the two
// counts equal by construction.
#include <QtTest>

#include "QTermSelectionGeometry.h"

using namespace QTerm::Internal;

class QTermSelectionGeometryTest : public QObject
{
    Q_OBJECT

private slots:
    void aSelectionEndingAtColumnZeroHasNoSpanForItsLastRow();
    void aMultiRowSelectionCoversMiddleRowsEdgeToEdge();
    void aSingleRowSelectionIsOneSpan();
    void anEmptySingleRowSelectionHasNoSpans();
    void rowsPastTheGridAreClippedNotDrawn();
    void aSelectionStartingAboveTheGridStartsAtColumnZero();
    void columnsPastTheWidthAreClamped();
    void everyCombinationYieldsOnlyDrawableSpans();
};

void QTermSelectionGeometryTest::aSelectionEndingAtColumnZeroHasNoSpanForItsLastRow()
{
    // A whole-line selection (triple-click, or a drag that ends at the start of the
    // next line) ends at column 0 of the row *after* the last selected one. That row
    // is empty. This is the case the old sizing got wrong: it counted row 6 (5 quads)
    // but only wrote 4.
    const auto spans = selectionSpans(/*startRow*/ 2, /*startColumn*/ 5,
                                      /*endRow*/ 6, /*endColumn*/ 0,
                                      /*rows*/ 10, /*columns*/ 80);
    QCOMPARE(spans.size(), 4);
    QCOMPARE(spans.first().row, 2);
    QCOMPARE(spans.last().row, 5);
    for (const SelectionSpan &s : spans)
        QVERIFY2(s.row != 6, "the empty last row must not produce a quad");
}

void QTermSelectionGeometryTest::aMultiRowSelectionCoversMiddleRowsEdgeToEdge()
{
    const auto spans = selectionSpans(1, 10, 4, 20, 10, 80);
    QCOMPARE(spans.size(), 4);
    // First row: from the start column to the right edge.
    QCOMPARE(spans[0].row, 1);
    QCOMPARE(spans[0].startColumn, 10);
    QCOMPARE(spans[0].endColumn, 80);
    // Middle rows: the whole width.
    for (int i : { 1, 2 }) {
        QCOMPARE(spans[i].startColumn, 0);
        QCOMPARE(spans[i].endColumn, 80);
    }
    // Last row: from the left edge to the end column.
    QCOMPARE(spans[3].row, 4);
    QCOMPARE(spans[3].startColumn, 0);
    QCOMPARE(spans[3].endColumn, 20);
}

void QTermSelectionGeometryTest::aSingleRowSelectionIsOneSpan()
{
    const auto spans = selectionSpans(4, 3, 4, 7, 10, 80);
    QCOMPARE(spans.size(), 1);
    QCOMPARE(spans[0].row, 4);
    QCOMPARE(spans[0].startColumn, 3);
    QCOMPARE(spans[0].endColumn, 7);
}

void QTermSelectionGeometryTest::anEmptySingleRowSelectionHasNoSpans()
{
    QVERIFY(selectionSpans(4, 7, 4, 7, 10, 80).isEmpty()); // zero width
    QVERIFY(selectionSpans(4, 9, 4, 3, 10, 80).isEmpty()); // end before start
}

void QTermSelectionGeometryTest::rowsPastTheGridAreClippedNotDrawn()
{
    // A selection left over from before a resize can point past the new row count.
    // Rows 8 and 9 exist; 10..15 do not. The old loop stopped at `row < rows` after
    // the buffer had been sized for all of them.
    const auto spans = selectionSpans(8, 4, 15, 30, 10, 80);
    QCOMPARE(spans.size(), 2);
    QCOMPARE(spans[0].row, 8);
    QCOMPARE(spans[0].startColumn, 4);
    QCOMPARE(spans[1].row, 9);
    QCOMPARE(spans[1].startColumn, 0);
    QCOMPARE(spans[1].endColumn, 80); // not `endColumn`: this is not the end row
}

void QTermSelectionGeometryTest::aSelectionStartingAboveTheGridStartsAtColumnZero()
{
    // Row 0 is not the row the selection began on, so `startColumn` must not apply
    // to it -- otherwise a clipped selection would lose its left part.
    const auto spans = selectionSpans(-3, 40, 2, 20, 10, 80);
    QCOMPARE(spans.size(), 3);
    QCOMPARE(spans[0].row, 0);
    QCOMPARE(spans[0].startColumn, 0);
    QCOMPARE(spans[0].endColumn, 80);
    QCOMPARE(spans[2].row, 2);
    QCOMPARE(spans[2].endColumn, 20);
}

void QTermSelectionGeometryTest::columnsPastTheWidthAreClamped()
{
    // Narrowing the terminal can leave a column beyond the new width. A quad wider
    // than the item is not a highlight.
    const auto spans = selectionSpans(3, 100, 5, 200, 10, 80);
    for (const SelectionSpan &s : spans) {
        QVERIFY(s.startColumn >= 0);
        QVERIFY(s.endColumn <= 80);
        QVERIFY(s.startColumn < s.endColumn);
    }
}

void QTermSelectionGeometryTest::everyCombinationYieldsOnlyDrawableSpans()
{
    // Exhaustive over a small grid, including out-of-range rows and columns on both
    // ends. The invariants are what make the vertex buffer safe: every span is a real
    // non-empty rectangle inside the grid, and rows strictly increase (no row is
    // drawn twice, which would show as a brighter band with a translucent colour).
    constexpr int rows = 4;
    constexpr int cols = 5;
    int combinations = 0;
    for (int sr = -2; sr <= rows + 1; ++sr)
    for (int sc = -1; sc <= cols + 1; ++sc)
    for (int er = -2; er <= rows + 1; ++er)
    for (int ec = -1; ec <= cols + 1; ++ec) {
        ++combinations;
        const auto spans = selectionSpans(sr, sc, er, ec, rows, cols);
        int previousRow = -1;
        for (const SelectionSpan &s : spans) {
            QVERIFY2(s.row >= 0 && s.row < rows,
                     qPrintable(QStringLiteral("row %1 outside the grid for (%2,%3)-(%4,%5)")
                                    .arg(s.row).arg(sr).arg(sc).arg(er).arg(ec)));
            QVERIFY2(s.startColumn >= 0 && s.startColumn < s.endColumn && s.endColumn <= cols,
                     qPrintable(QStringLiteral("bad columns [%1,%2) for (%3,%4)-(%5,%6)")
                                    .arg(s.startColumn).arg(s.endColumn).arg(sr).arg(sc).arg(er).arg(ec)));
            QVERIFY2(s.row > previousRow, "rows must strictly increase");
            previousRow = s.row;
        }
    }
    // Guard against the loops silently becoming empty: rows -2..rows+1 and columns
    // -1..cols+1 are 8 values each, on both ends.
    QCOMPARE(combinations, 8 * 8 * 8 * 8);
}

QTEST_APPLESS_MAIN(QTermSelectionGeometryTest)
#include "QTermSelectionGeometryTest.moc"
