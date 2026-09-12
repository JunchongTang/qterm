// The mouse pointer over a terminal is an I-beam, not an arrow.
//
// A terminal is a text surface: the arrow reads as "nothing here is selectable",
// and every terminal emulator shows an I-beam instead. The one exception is an
// application that has taken the mouse over (DECSET 1000/1002/1003 -- vim, htop,
// tmux): there, clicks are delivered to the program rather than starting a
// selection, so an I-beam would promise something that does not happen.
//
// Both halves are worth pinning. The shape is set in two places -- the constructor
// and updateMouseAcceptance() -- because the latter only runs when the mode
// changes, so a terminal that never enables mouse reporting would otherwise keep
// the widget-default arrow for its whole life.
#include <QtTest>

#include <QTerm/QTermQuickItem.h>
#include <QTerm/QTermQuickPaintedItem.h>
#include <QTerm/QTermTerminal.h>

using namespace QTerm;

class QTermPointerShapeTest : public QObject
{
    Q_OBJECT

private slots:
    void theSceneGraphItemStartsWithAnIBeam();
    void thePaintedItemStartsWithAnIBeam();
    void anApplicationThatGrabsTheMouseGetsTheArrowBack();
    void releasingTheMouseRestoresTheIBeam();
};

void QTermPointerShapeTest::theSceneGraphItemStartsWithAnIBeam()
{
    QTermQuickItem item;
    QCOMPARE(item.cursor().shape(), Qt::IBeamCursor);
}

void QTermPointerShapeTest::thePaintedItemStartsWithAnIBeam()
{
    QTermQuickPaintedItem item;
    QCOMPARE(item.cursor().shape(), Qt::IBeamCursor);
}

void QTermPointerShapeTest::anApplicationThatGrabsTheMouseGetsTheArrowBack()
{
    QTermTerminal terminal;
    QTermQuickItem item;
    item.setTerminal(&terminal);

    // DECSET 1002: report presses and drag motion -- what vim and htop turn on.
    terminal.feedText(QStringLiteral("\x1b[?1002h"));
    QCOMPARE(item.cursor().shape(), Qt::ArrowCursor);
}

void QTermPointerShapeTest::releasingTheMouseRestoresTheIBeam()
{
    QTermTerminal terminal;
    QTermQuickItem item;
    item.setTerminal(&terminal);

    terminal.feedText(QStringLiteral("\x1b[?1002h"));
    terminal.feedText(QStringLiteral("\x1b[?1002l"));
    QCOMPARE(item.cursor().shape(), Qt::IBeamCursor);
}

QTEST_MAIN(QTermPointerShapeTest)
#include "QTermPointerShapeTest.moc"
