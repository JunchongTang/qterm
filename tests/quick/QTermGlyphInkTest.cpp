// SPDX-License-Identifier: MIT
//
// The glyph-in-the-cell placement, and the raster margin that used to shift it.

#include <QtTest>
#include <QImage>
#include <QRawFont>

#include "QTermGlyphAtlas.h"
#include "QTermGlyphInk.h"

using namespace QTerm;

class QTermGlyphInkTest : public QObject
{
    Q_OBJECT
private slots:
    void theInkOffsetIsWhereTheInkIs();
    void anEmptyRasterHasNoInk();
    void theDrawnInkLandsWhereBoundingRectSays();
};

void QTermGlyphInkTest::theInkOffsetIsWhereTheInkIs()
{
    QImage raster(6, 6, QImage::Format_Alpha8);
    raster.fill(Qt::transparent);
    // A two-by-two blob whose top-left pixel is (2, 3).
    for (int y = 3; y <= 4; ++y)
        for (int x = 2; x <= 3; ++x)
            raster.setPixelColor(x, y, QColor(0, 0, 0, 255));

    QCOMPARE(inkOffsetIn(raster), QPoint(2, 3));

    // Ink touching the top-left corner must still read as (0, 0) -- and that is also
    // what "no margin at all" looks like, which is why the offset is applied
    // unconditionally rather than only when it is non-zero.
    QImage cornered(4, 4, QImage::Format_Alpha8);
    cornered.fill(Qt::transparent);
    cornered.setPixelColor(0, 0, QColor(0, 0, 0, 255));
    QCOMPARE(inkOffsetIn(cornered), QPoint(0, 0));
}

void QTermGlyphInkTest::anEmptyRasterHasNoInk()
{
    QCOMPARE(inkOffsetIn(QImage()), QPoint(0, 0));

    QImage blank(5, 5, QImage::Format_Alpha8);
    blank.fill(Qt::transparent);
    QCOMPARE(inkOffsetIn(blank), QPoint(0, 0));
}

/*!
    **画出来的墨迹必须落在 `boundingRect()` 说的位置。**

    这是这条修复唯一有意义的判据,而且它**不是重言式**:atlas 用的是"位图左上角"的偏移,
    而 `boundingRect()` 描述的是**墨迹**的位置,两者差着位图自带的那圈透明边。把减法去掉,
    左边就会多出一个 `inkOffsetIn()`(实测 Menlo 上是 (1,1)),这条立刻红。

    (比"墨迹尺寸对不对"可靠得多:尺寸只说明画的是哪个字形,说明不了画在哪儿。)
*/
void QTermGlyphInkTest::theDrawnInkLandsWhereBoundingRectSays()
{
    QString family = QStringLiteral("Menlo");
    if (!QFontDatabase::families().contains(family))
        family = QFontDatabase::families().value(0);
    if (family.isEmpty())
        QSKIP("这台机器上一个字体都没有");

    QFont font(family);
    font.setPixelSize(13);
    const QRawFont face = QRawFont::fromFont(font);
    QVERIFY2(face.isValid(), "QRawFont 解析失败");

    const QVector<quint32> indexes = face.glyphIndexesForString(QStringLiteral("A"));
    QVERIFY2(!indexes.isEmpty() && indexes.first() != 0, "取不到 'A' 的字形");
    const quint32 index = indexes.first();

    QTermGlyphAtlas atlas;
    atlas.setFont(font);
    const QTermGlyphAtlas::Glyph *glyph = atlas.glyphFor(U'A', QTermGlyphAtlas::Regular);
    QVERIFY2(glyph, "atlas 里没有 'A' 的字形");

    const QImage raster = face.alphaMapForGlyph(index, QRawFont::PixelAntialiasing);
    QVERIFY2(!raster.isNull(), "取不到位图");
    const QPoint ink = inkOffsetIn(raster);

    QCOMPARE(glyph->bearing + QPointF(ink), face.boundingRect(index).topLeft());
}

QTEST_MAIN(QTermGlyphInkTest)
#include "QTermGlyphInkTest.moc"
