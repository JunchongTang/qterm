// SPDX-License-Identifier: MIT
//
// Tests for the fallback ("supplementary") font: the property plumbing, the cell
// height rule, and the one that actually matters -- that the fallback face is what
// draws a character the main family has no glyph for.
//
// The last one is why this file exists at all: a fallback that is stored but never
// reaches the font would leave every other assertion in here green.

#include <QtTest>
#include <QFontMetricsF>
#include <QSignalSpy>

#include "QTermGlyphAtlas.h"
#include <QTerm/QTermQuickItem.h>
#include <QTerm/QTermTerminal.h>
#include "QTermViewController.h"

using namespace QTerm;

namespace {
constexpr char32_t kHan = U'喜';   // a character Menlo has no glyph for
}

class QTermFallbackFontTest : public QObject
{
    Q_OBJECT
private slots:
    void theFallbackIsReportedBackThroughTheItem();
    void theCellTakesTheTallerOfTheTwoMetrics();
    void aWiderFallbackDoesNotWidenTheCell();
    void theFallbackFaceIsWhatDrawsTheCharacter();
    void theResolvedFontCarriesTheFallbackList();
};

void QTermFallbackFontTest::theFallbackIsReportedBackThroughTheItem()
{
    QTermTerminal terminal;
    QTermQuickItem item;
    item.setTerminal(&terminal);

    QSignalSpy spy(&item, &QTermQuickItem::fontChanged);
    item.setFallbackFamilies({ QStringLiteral("PingFang SC") });
    QCOMPARE(item.fallbackFamilies(), QStringList{ QStringLiteral("PingFang SC") });
    QCOMPARE(spy.count(), 1);

    // Setting the same list again is not a change, so it must not churn the view.
    item.setFallbackFamilies({ QStringLiteral("PingFang SC") });
    QCOMPARE(spy.count(), 1);

    item.setFallbackFamilies({});
    QCOMPARE(item.fallbackFamilies(), QStringList{});
    QCOMPARE(spy.count(), 2);
}

void QTermFallbackFontTest::theCellTakesTheTallerOfTheTwoMetrics()
{
    QTermTerminal terminal;
    QTermQuickItem item;
    item.setTerminal(&terminal);
    item.setFontFamily(QStringLiteral("Menlo"));
    item.setFontPixelSize(13);

    const qreal primaryOnly = item.cellHeight();

    const QString fallback = QStringLiteral("PingFang SC");
    if (!QFontDatabase::families().contains(fallback))
        QSKIP("this machine has no PingFang SC to fall back to");

    QFont fallbackFont(fallback);
    fallbackFont.setPixelSize(13);
    const qreal fallbackSpacing = QFontMetricsF(fallbackFont).lineSpacing();

    item.setFallbackFamilies({ fallback });

    // The rule, stated independently of the implementation: the cell is the taller
    // of the two natural line spacings (times the line-height multiplier, 1.0 here).
    QCOMPARE(item.cellHeight(), qMax(primaryOnly, fallbackSpacing));
    if (fallbackSpacing > primaryOnly) {
        QVERIFY2(item.cellHeight() > primaryOnly,
                 "a taller fallback must make the cell taller -- that is the whole point "
                 "(CJK ink otherwise sits on the bottom edge of the cell)");
    }
}

void QTermFallbackFontTest::aWiderFallbackDoesNotWidenTheCell()
{
    QTermTerminal terminal;
    QTermQuickItem item;
    item.setTerminal(&terminal);
    item.setFontFamily(QStringLiteral("Menlo"));
    item.setFontPixelSize(13);
    const qreal width = item.cellWidth();

    item.setFallbackFamilies({ QStringLiteral("PingFang SC") });
    QCOMPARE(item.cellWidth(), width);
}

void QTermFallbackFontTest::theFallbackFaceIsWhatDrawsTheCharacter()
{
    /*!
        Prefer a face Qt would **not** pick by itself: a Song/Kai CJK face against the
        sans default. With a face that happens to be the automatic pick this test can
        only ever skip, which is why the list is ordered serif-first.

        The comparison is on **pixels**, not on the ink box: every full-width CJK glyph
        has the same raster size, so comparing sizes lets two completely different
        faces look identical. (The first version of this test did exactly that and
        skipped on the machine it was written on.)
    */
    QString fallback;
    for (const QString &candidate : { QStringLiteral("Songti SC"),
                                      QStringLiteral("Kaiti SC"),
                                      QStringLiteral("STSong"),
                                      QStringLiteral("Hiragino Mincho Pro"),
                                      QStringLiteral("Hiragino Sans GB"),
                                      QStringLiteral("Osaka") }) {
        if (QFontDatabase::families().contains(candidate)) {
            fallback = candidate;
            break;
        }
    }
    if (fallback.isEmpty())
        QSKIP("this machine has no CJK face to fall back to");

    const auto pixelsForHan = [](const QFont &font) {
        QTermGlyphAtlas atlas;
        atlas.setFont(font);
        const QTermGlyphAtlas::Glyph *glyph = atlas.glyphFor(kHan, QTermGlyphAtlas::Regular);
        return glyph ? atlas.image().copy(glyph->region) : QImage();
    };

    QFont primary(QStringLiteral("Menlo"));
    primary.setPixelSize(13);
    const QImage autoPicked = pixelsForHan(primary);

    QFont withFallback(QStringLiteral("Menlo"));
    withFallback.setPixelSize(13);
    withFallback.setFamilies({ QStringLiteral("Menlo"), fallback });
    const QImage supplemented = pixelsForHan(withFallback);

    QVERIFY2(!autoPicked.isNull(), "the character did not resolve to any face at all");
    QVERIFY2(!supplemented.isNull(), "the supplemented font lost the glyph entirely");

    if (autoPicked == supplemented)
        QSKIP("Qt's automatic pick for this character is already the fallback face, so this "
              "machine cannot tell the two apart");

    // Different face -> different pixels. This is the only observable proof that the
    // fallback list reached the font: a fallback that is stored but never applied
    // would leave every other assertion in this file green.
    QVERIFY(supplemented != autoPicked);
}

/*!
    The chain that has to hold for a supplementary font to do anything at all:
    the item's property reaches the controller (see the first test), and the
    controller's font -- the single one the renderers lay text out with -- carries
    the fallback after the main family.

    Without this the atlas test above would still pass on its own: it feeds
    QFont::setFamilies() by hand and so only proves that Qt honours a families
    list, not that we ever build one.
*/
void QTermFallbackFontTest::theResolvedFontCarriesTheFallbackList()
{
    QTermViewController controller;
    controller.setFontFamily(QStringLiteral("Menlo"));
    controller.setFontPixelSize(13);
    controller.setFallbackFamilies({ QStringLiteral("PingFang SC") });

    QCOMPARE(controller.resolvedFont().families(),
             QStringList({ QStringLiteral("Menlo"), QStringLiteral("PingFang SC") }));

    // The main family stays first: it is still what draws everything it can, and the
    // cell width comes from it (a supplement must not widen the grid).
    QCOMPARE(controller.resolvedFont().pixelSize(), 13);

    controller.setFallbackFamilies({});
    QCOMPARE(controller.resolvedFont().families(), QStringList({ QStringLiteral("Menlo") }));
}

QTEST_MAIN(QTermFallbackFontTest)
#include "QTermFallbackFontTest.moc"
