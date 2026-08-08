// Reverse video's glyph colour must survive a translucent background.
//
// The regression this guards: `backgroundColor` is *both* the fill for the whole
// terminal area *and* the fallback glyph colour for reverse-video (SGR 7) cells
// that never set a background of their own. A translucent terminal needs alpha on
// the first role and must not have it on the second -- a semi-transparent glyph
// painted over a solid block of its own foreground is a smear, and at alpha 0 the
// text disappears outright (vim's status line becomes a blank bar).
#include <QtTest>

#include <QTerm/QTermQuickItem.h>

using namespace QTerm;

class QTermInverseTextTest : public QObject
{
    Q_OBJECT

private slots:
    void derivedKeepsHueAndForcesOpaque();
    void derivedSurvivesAFullyTransparentBackground();
    void explicitValueWins();
    void clearingFallsBackToDerived();
};

void QTermInverseTextTest::derivedKeepsHueAndForcesOpaque()
{
    QTermQuickItem item;
    item.setBackgroundColor(QColor(0x1e, 0x2a, 0x33, 0x8c));

    const QColor c = item.effectiveInverseTextColor();
    // Hue must come from the background: a hardcoded constant only looks right on
    // one of the two themes (dark-on-dark on the other).
    QCOMPARE(c.red(), 0x1e);
    QCOMPARE(c.green(), 0x2a);
    QCOMPARE(c.blue(), 0x33);
    QCOMPARE(c.alpha(), 255);
}

void QTermInverseTextTest::derivedSurvivesAFullyTransparentBackground()
{
    QTermQuickItem item;
    item.setBackgroundColor(QColor(0xfd, 0xf6, 0xe3, 0));

    const QColor c = item.effectiveInverseTextColor();
    QCOMPARE(c.alpha(), 255);
    QCOMPARE(c.red(), 0xfd);
}

void QTermInverseTextTest::explicitValueWins()
{
    QTermQuickItem item;
    item.setBackgroundColor(QColor(0xfd, 0xf6, 0xe3, 0));
    item.setInverseTextColor(QColor(0x11, 0x22, 0x33));

    QCOMPARE(item.effectiveInverseTextColor(), QColor(0x11, 0x22, 0x33));
}

void QTermInverseTextTest::clearingFallsBackToDerived()
{
    QTermQuickItem item;
    item.setBackgroundColor(QColor(0xfd, 0xf6, 0xe3, 0));
    item.setInverseTextColor(QColor(0x11, 0x22, 0x33));
    item.setInverseTextColor(QColor());   // invalid = derive again

    QCOMPARE(item.effectiveInverseTextColor().red(), 0xfd);
    QCOMPARE(item.effectiveInverseTextColor().alpha(), 255);
}

QTEST_MAIN(QTermInverseTextTest)
#include "QTermInverseTextTest.moc"
