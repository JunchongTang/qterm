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
    void syncsSizeImmediatelyOnGeometryChange();
    void clampsSizeToMinimumColumnsAndRows();

    // The core resize-regression bug: prompt lines must survive repeated
    // width oscillation driven through QTermQuickPaintedItem geometry changes.
    void preservesPromptLinesAcrossWidthOscillation();
    void preservesPromptLinesWhenShellUsesAbsoluteColumnMoveViaQuickItem();
};

// ─────────────────────────────────────────────────────────────────────────────
// QTermQuickPaintedItem::geometryChange must call syncTerminalSize() synchronously
// (no debounce) so the terminal column count reflects the new width immediately.
// ─────────────────────────────────────────────────────────────────────────────
void QTermQuickPaintedItemTest::syncsSizeImmediatelyOnGeometryChange()
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

    // Narrow: geometry change must propagate synchronously.
    item.setWidth(400);
    QCOMPARE(terminal.columns(), expectedColumns(item, 400));

    // Widen back.
    item.setWidth(800);
    QCOMPARE(terminal.columns(), expectedColumns(item, 800));
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

    // 80-char prompt — fits at wide width, wraps at narrow.
    const QString prompt =
        u"➜  tangjc@MBP /Users/tangjc/1-proj/2-mygithub/qterm/build/examples/quick-demo"_s;

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
        u"➜  tangjc@MBP /Users/tangjc/1-proj/2-mygithub/qterm/build/examples/quick-demo"_s;

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
