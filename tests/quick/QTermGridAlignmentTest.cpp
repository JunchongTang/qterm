// Text must land on the cell grid, whatever font ends up drawing it.
//
// The regression this guards: every renderer used to hand a whole style run to
// Qt (drawText / one QTextLayout) and let the font advance the pen. That matches
// the grid only while every glyph comes from the monospaced primary font. The
// moment a character falls back to another font it stops matching, and the error
// *accumulates across the line*.
//
// Measured with Menlo at 15 px: a cell is 9.016 px, so a wide character owes
// 18.03 px -- but the CJK fallback advances 15.00. Three pixels short per
// character; a ten-character Chinese file name pushes everything after it three
// cells to the left. That is what "the columns of `ls` don't line up" looked
// like, against a phone full of Chinese folder names. Narrow characters are not
// safe either: with JetBrains Mono a box-drawing U+2500 advances 15.0 into a
// 12.875 px cell, so a TUI's horizontal rule grows two pixels per character.
//
// None of that is a crash, and none of it shows up on ASCII -- which is why it
// survived this long and why it is pinned here instead of in a screenshot.
#include <QtTest>

#include <QFont>
#include <QFontMetricsF>

// The segmentation lives in an internal header shared by all three renderers.
#include "../../src/QTermRenderUtils.h"

class QTermGridAlignmentTest : public QObject
{
    Q_OBJECT

private:
    // A monospaced face that is present on every machine we build on, at a size
    // where the CJK fallback is measurably narrower than two cells.
    static QFont terminalFont()
    {
        QFont font(QStringLiteral("Menlo"));
        font.setPixelSize(15);
        return font;
    }

    static qreal cellWidthFor(const QFont &font)
    {
        // Same rule the view controller uses.
        return QFontMetricsF(font).horizontalAdvance(QLatin1Char('M'));
    }

    // Where each segment is drawn, and where the next one starts, in pixels --
    // this is the number the renderers actually position with.
    static qreal segmentEnd(const QTermGridSegment &segment, qreal cellW)
    {
        return (segment.column + segment.columns) * cellW;
    }

private slots:
    void asciiStaysOneSegmentWithNoCorrection()
    {
        // The common case must not get slower or fuzzier: one draw call, and no
        // letter spacing at all (the primary font already advances one cell).
        const QFont font = terminalFont();
        const qreal cellW = cellWidthFor(font);
        const auto segments = qtermGridSegments(QStringLiteral("hello world"),
                                                QFontMetricsF(font), cellW);
        QCOMPARE(segments.size(), 1);
        QCOMPARE(segments.first().column, 0);
        QCOMPARE(segments.first().columns, 11);
        QVERIFY2(qAbs(segments.first().letterSpacing) < 0.01,
                 qPrintable(QStringLiteral("ASCII picked up a correction of %1")
                                .arg(segments.first().letterSpacing)));
    }

    void wideCharactersClaimTwoCellsEachEvenWhenTheGlyphIsNarrower()
    {
        const QFont font = terminalFont();
        const qreal cellW = cellWidthFor(font);
        const QFontMetricsF metrics(font);

        const QString text = QString::fromUtf8("中文");
        const qreal glyphAdvance = metrics.horizontalAdvance(QString::fromUtf8("中"));
        // Precondition for this whole test file: the fallback really is narrower
        // than two cells. If a future Qt/macOS picks a full-width fallback this
        // stops being a problem -- and the assertions below would be vacuous.
        QVERIFY2(glyphAdvance < 2 * cellW - 0.5,
                 qPrintable(QStringLiteral("CJK advance %1 already fills 2 cells (%2)")
                                .arg(glyphAdvance).arg(2 * cellW)));

        const auto segments = qtermGridSegments(text, metrics, cellW);
        QCOMPARE(segments.size(), 1);
        QCOMPARE(segments.first().columns, 4);        // two wide characters
        // The correction is exactly what turns the glyph's own advance into two
        // cells -- that is the whole trick.
        QVERIFY(qAbs(glyphAdvance + segments.first().letterSpacing - 2 * cellW) < 0.01);
    }

    void aMixedRunSplitsSoTheNextColumnStartsOnTheGrid()
    {
        // The real shape of `ls` output: a Chinese name, padding, an ASCII name.
        // What matters is not how many segments come out but that the ASCII part
        // starts at its cell -- that is the column the user sees drift.
        const QFont font = terminalFont();
        const qreal cellW = cellWidthFor(font);
        const QString text = QString::fromUtf8("北京车展  a.txt");
        const auto segments = qtermGridSegments(text, QFontMetricsF(font), cellW);

        QVERIFY(segments.size() >= 2);
        // Segments tile the run with no gap and no overlap, in order.
        int expectedColumn = 0;
        qsizetype expectedStart = 0;
        for (const QTermGridSegment &segment : segments) {
            QCOMPARE(segment.column, expectedColumn);
            QCOMPARE(segment.start, expectedStart);
            expectedColumn += segment.columns;
            expectedStart += segment.length;
        }
        QCOMPARE(expectedStart, qsizetype(text.size()));
        // 4 wide characters (8 cells) + two spaces + "a.txt".
        QCOMPARE(expectedColumn, 8 + 2 + 5);

        // The narrow tail (the two spaces and the name -- one segment, because a
        // monospaced space and letter advance alike) is drawn starting at cell 8:
        // **right after the wide run's cells**, not at "wherever the Chinese
        // glyphs happened to end", which is three pixels per character earlier.
        const QTermGridSegment &tail = segments.last();
        QCOMPARE(tail.column, 8);
        QCOMPARE(segmentEnd(tail, cellW), 15 * cellW);
    }

    void twoRowsWithDifferentWideCountsEndAtTheSameColumn()
    {
        // This is the user-visible property, stated directly: pad to the same
        // number of *cells* and the next column must start at the same x, no
        // matter how many of those cells were filled by wide characters.
        const QFont font = terminalFont();
        const qreal cellW = cellWidthFor(font);
        const QFontMetricsF metrics(font);

        //  6 cells of Chinese + 4 spaces  |  2 cells of Chinese + 8 spaces
        const QString rowA = QString::fromUtf8("北京车    ");
        const QString rowB = QString::fromUtf8("天        ");

        const auto a = qtermGridSegments(rowA, metrics, cellW);
        const auto b = qtermGridSegments(rowB, metrics, cellW);
        QCOMPARE(segmentEnd(a.last(), cellW), segmentEnd(b.last(), cellW));
        QCOMPARE(segmentEnd(a.last(), cellW), 10 * cellW);
    }

    void combiningMarksRideWithTheirBase()
    {
        // A base plus its marks is one cell and must be measured as a unit --
        // splitting it would put the mark in the next cell.
        const QFont font = terminalFont();
        const qreal cellW = cellWidthFor(font);
        const QString text = QString::fromUtf8("éx");   // e + combining acute, then x
        const auto segments = qtermGridSegments(text, QFontMetricsF(font), cellW);

        QCOMPARE(text.size(), 3);              // 前提:真的是 基字 + 组合符 + x
        int columns = 0;
        qsizetype covered = 0;
        for (const QTermGridSegment &segment : segments) {
            columns += segment.columns;
            covered += segment.length;
        }
        QCOMPARE(columns, 2);                  // 「é」和「x」,不是三格
        QCOMPARE(covered, qsizetype(text.size()));
    }

    void emptyTextProducesNoSegments()
    {
        const QFont font = terminalFont();
        QVERIFY(qtermGridSegments(QString(), QFontMetricsF(font), cellWidthFor(font)).isEmpty());
    }
};

QTEST_MAIN(QTermGridAlignmentTest)
#include "QTermGridAlignmentTest.moc"
