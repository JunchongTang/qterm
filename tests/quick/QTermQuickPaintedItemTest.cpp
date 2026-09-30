#include <QtTest>

#include <QTerm/QTermQuickPaintedItem.h>
#include <QTerm/QTermTerminal.h>

#include <cmath>

using namespace Qt::StringLiterals;

namespace QTerm {

// ─────────────────────────────────────────────────────────────────────────────
// Helper: compute expected columns/rows from pixel size and cell metrics.
// Mirrors QTermQuickPaintedItem::syncTerminalSize() exactly.
// ─────────────────────────────────────────────────────────────────────────────
static int expectedColumns(const QTermQuickPaintedItem &item, qreal pixelWidth)
{
    constexpr int kMinimumColumns = 20;
    return qMax(kMinimumColumns,
                static_cast<int>(std::floor(pixelWidth / qMax<qreal>(1.0, item.cellWidth()))));
}

static int expectedRows(const QTermQuickPaintedItem &item, qreal pixelHeight)
{
    constexpr int kMinimumRows = 8;
    return qMax(kMinimumRows,
                static_cast<int>(std::floor(pixelHeight / qMax<qreal>(1.0, item.cellHeight()))));
}

// ─────────────────────────────────────────────────────────────────────────────
// Test suite
// ─────────────────────────────────────────────────────────────────────────────
class QTermQuickPaintedItemTest : public QObject
{
    Q_OBJECT

private slots:
    // Geometry → terminal size propagation
    void syncsSizeOnGeometryChange();
    void clampsSizeToMinimumColumnsAndRows();

    // Line height: taller cells, same glyphs, fewer rows.
    void lineHeightStretchesTheCellWithoutTouchingTheWidth();
    void lineHeightIsClampedAndOnlyReportsRealChanges();

    // The core resize-regression bug: prompt lines must survive repeated
    // width oscillation driven through QTermQuickPaintedItem geometry changes.
    void preservesPromptLinesAcrossWidthOscillation();
    void preservesPromptLinesWhenShellUsesAbsoluteColumnMoveViaQuickItem();
};

// ─────────────────────────────────────────────────────────────────────────────
// Line height multiplies the cell *height* only.
//
// The whole point of the setting is that the glyphs keep their size and the
// columns stay put — only the gap between rows grows. Everything downstream
// (hit testing, the cursor rect, scroll math, the row count pushed to the pty)
// is derived from cellHeight(), so this one assertion covers all of it: if the
// factor leaked into the width or into the font, columns would move too.
// ─────────────────────────────────────────────────────────────────────────────
void QTermQuickPaintedItemTest::lineHeightStretchesTheCellWithoutTouchingTheWidth()
{
    QTermTerminal terminal;
    QTermQuickPaintedItem item;

    item.setFontFamily(u"Courier New"_s);
    item.setFontPixelSize(14);
    item.setTerminal(&terminal);
    item.setWidth(800);
    item.setHeight(480);

    QCOMPARE(item.lineHeight(), 1.0);
    const qreal baseWidth = item.cellWidth();
    const qreal baseHeight = item.cellHeight();
    const int baseRows = terminal.rows();

    item.setLineHeight(1.5);

    QCOMPARE(item.cellWidth(), baseWidth);
    QVERIFY2(qFuzzyCompare(item.cellHeight(), baseHeight * 1.5),
             qPrintable(QStringLiteral("cell height %1, expected %2")
                            .arg(item.cellHeight()).arg(baseHeight * 1.5)));

    // Taller cells → fewer rows in the same viewport, and the terminal has to
    // hear about it (the shell reflows on SIGWINCH). The sync is debounced.
    QTRY_COMPARE(terminal.rows(), expectedRows(item, 480));
    QVERIFY2(terminal.rows() < baseRows, "row count did not shrink");
    QCOMPARE(terminal.columns(), expectedColumns(item, 800));
}

// ─────────────────────────────────────────────────────────────────────────────
// Out-of-range factors are clamped, and a set that changes nothing stays quiet.
//
// Zero or negative would collapse the cell, and every row/column computation
// divides by it. The "stays quiet" half matters because the hosts emit
// fontChanged() from this setter: comparing against the *argument* instead of
// the stored value would re-emit forever once a caller passes something out of
// range (a slider bound to a wider span, say).
// ─────────────────────────────────────────────────────────────────────────────
void QTermQuickPaintedItemTest::lineHeightIsClampedAndOnlyReportsRealChanges()
{
    QTermTerminal terminal;
    QTermQuickPaintedItem item;

    item.setFontFamily(u"Courier New"_s);
    item.setFontPixelSize(14);
    item.setTerminal(&terminal);
    item.setWidth(800);
    item.setHeight(480);

    QSignalSpy spy(&item, &QTermQuickPaintedItem::fontChanged);

    item.setLineHeight(99.0);
    QCOMPARE(item.lineHeight(), 3.0);
    QCOMPARE(spy.count(), 1);

    item.setLineHeight(99.0);               // still clamped to the same value
    QCOMPARE(spy.count(), 1);

    item.setLineHeight(0.0);
    QCOMPARE(item.lineHeight(), 0.8);
    QCOMPARE(spy.count(), 2);

    QVERIFY(item.cellHeight() >= 1.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// A geometry change must reach the terminal's column and row count.
//
// The first real size is applied synchronously: there is nothing on screen yet,
// so there is no reason to make the caller wait. Later changes are debounced,
// because dragging a window edge produces a geometry change per frame and each
// resize reflows the whole scrollback.
// ─────────────────────────────────────────────────────────────────────────────
void QTermQuickPaintedItemTest::syncsSizeOnGeometryChange()
{
    QTermTerminal terminal;
    QTermQuickPaintedItem item;

    item.setFontFamily(u"Courier New"_s);
    item.setFontPixelSize(14);
    item.setTerminal(&terminal);

    // Initial geometry.
    item.setWidth(800);
    item.setHeight(480);

    QCOMPARE(terminal.columns(), expectedColumns(item, 800));
    QCOMPARE(terminal.rows(),    expectedRows(item, 480));

    // Narrow: propagates once the debounce interval has elapsed.
    item.setWidth(400);
    QTRY_COMPARE(terminal.columns(), expectedColumns(item, 400));

    // Widen back.
    item.setWidth(800);
    QTRY_COMPARE(terminal.columns(), expectedColumns(item, 800));
}

// ─────────────────────────────────────────────────────────────────────────────
// Pixel widths that produce fewer than kMinimumColumns columns must still yield
// at least kMinimumColumns.
// ─────────────────────────────────────────────────────────────────────────────
void QTermQuickPaintedItemTest::clampsSizeToMinimumColumnsAndRows()
{
    QTermTerminal terminal;
    QTermQuickPaintedItem item;

    item.setFontFamily(u"Courier New"_s);
    item.setFontPixelSize(14);
    item.setTerminal(&terminal);

    item.setWidth(1);    // 1 pixel → well below 20 columns
    item.setHeight(1);   // 1 pixel → well below 8 rows
    QVERIFY(terminal.columns() >= 20);
    QVERIFY(terminal.rows() >= 8);
}

// ─────────────────────────────────────────────────────────────────────────────
// Reproduces the user-reported bug end-to-end through QTermQuickPaintedItem:
//
//   "I press Enter 5 times, then drag the window width from ~1120 px to ~50 px
//    and back, repeated 3-5 times. Eventually some prompt lines disappear."
//
// Driving geometry changes through QTermQuickPaintedItem means the resize path is
// exactly the same as the real application.
// ─────────────────────────────────────────────────────────────────────────────
void QTermQuickPaintedItemTest::preservesPromptLinesAcrossWidthOscillation()
{
    QTermTerminal terminal;
    QTermQuickPaintedItem item;

    // Font pixel size chosen so that the ~80-char prompt fits at wide width
    // but wraps at narrow width.  cellWidth ≈ 8-9 px @ 14 px.
    item.setFontFamily(u"Courier New"_s);
    item.setFontPixelSize(14);
    item.setTerminal(&terminal);

    // Wide window (≈ 1120 px) — prompt fits in one row.
    item.setWidth(1120);
    item.setHeight(600);

    // 77-char prompt — fits at wide width, wraps at narrow.
    const QString prompt =
        u"➜  dev@workstation /home/dev/workspace/terminal-app/build/examples/qtquick-terminal"_s;

    // Simulate 5 Enter presses: 4 completed lines + 1 active prompt.
    QString transcript;
    for (int i = 0; i < 4; ++i) {
        transcript += prompt + u"\r\n"_s;
    }
    transcript += prompt;
    terminal.feedText(transcript);

    const QString expected = QStringList(5, prompt).join(u'\n');
    QCOMPARE(terminal.surfaceModel()->plainText(), expected);

    // ── Simulate the user dragging the window 5 times ──────────────────────
    // Each "drag" is represented as a sequence of width changes exactly as
    // QTermQuickPaintedItem fires them from geometryChange — one call per pixel column.
    // We step by 10 px to keep the test fast.
    for (int cycle = 0; cycle < 5; ++cycle) {
        // Drag narrow (1120 → 50 px step by step).
        for (qreal w = 1100; w >= 50; w -= 50) {
            item.setWidth(w);
        }
        // Shell receives final SIGWINCH and redraws (via \r + ESC[K + prompt).
        terminal.feedText(u"\r\x1b[K"_s + prompt);
        QCOMPARE(terminal.surfaceModel()->plainText(), expected);

        // Drag wide (50 → 1120 px step by step).
        for (qreal w = 100; w <= 1100; w += 50) {
            item.setWidth(w);
        }
        item.setWidth(1120);
        // Shell redraws at wide width.
        terminal.feedText(u"\r\x1b[K"_s + prompt);
        QCOMPARE(terminal.surfaceModel()->plainText(), expected);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Same as above but the shell uses CSI G (ESC[1G, absolute column move) instead
// of CR for its redraw — the bug that was actually causing data loss in practice.
// ─────────────────────────────────────────────────────────────────────────────
void QTermQuickPaintedItemTest::preservesPromptLinesWhenShellUsesAbsoluteColumnMoveViaQuickItem()
{
    QTermTerminal terminal;
    QTermQuickPaintedItem item;

    item.setFontFamily(u"Courier New"_s);
    item.setFontPixelSize(14);
    item.setTerminal(&terminal);

    item.setWidth(1120);
    item.setHeight(600);

    const QString prompt =
        u"➜  dev@workstation /home/dev/workspace/terminal-app/build/examples/qtquick-terminal"_s;

    QString transcript;
    for (int i = 0; i < 4; ++i) {
        transcript += prompt + u"\r\n"_s;
    }
    transcript += prompt;
    terminal.feedText(transcript);

    const QString expected = QStringList(5, prompt).join(u'\n');
    QCOMPARE(terminal.surfaceModel()->plainText(), expected);

    for (int cycle = 0; cycle < 5; ++cycle) {
        for (qreal w = 1100; w >= 50; w -= 50) {
            item.setWidth(w);
        }
        // Shell uses ESC[1G (CHA) instead of CR — previously unimplemented,
        // causing the predecessor wrap chain to never be severed.
        terminal.feedText(u"\x1b[1G\x1b[K"_s + prompt);
        QCOMPARE(terminal.surfaceModel()->plainText(), expected);

        for (qreal w = 100; w <= 1100; w += 50) {
            item.setWidth(w);
        }
        item.setWidth(1120);
        terminal.feedText(u"\x1b[1G\x1b[K"_s + prompt);
        QCOMPARE(terminal.surfaceModel()->plainText(), expected);
    }
}

} // namespace QTerm

QTEST_MAIN(QTerm::QTermQuickPaintedItemTest)

#include "QTermQuickPaintedItemTest.moc"
