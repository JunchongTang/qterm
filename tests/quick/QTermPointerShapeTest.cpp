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

#include <QQuickWindow>

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
    void theWindowActuallyShowsTheBeamUnderThePointer();
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

// The three cases above pin the item's own cursor. This one pins the thing the
// user actually sees: a window only adopts an item's cursor if the item is
// registered as a cursor owner and the window recomputes on mouse move. An item
// that "has" the right cursor while the window keeps drawing the arrow would pass
// every check above and still look broken.
void QTermPointerShapeTest::theWindowActuallyShowsTheBeamUnderThePointer()
{
    QQuickWindow window;
    window.resize(200, 120);

    QTermQuickItem item(window.contentItem());
    item.setSize(QSizeF(200, 120));

    window.show();
    if (!QTest::qWaitForWindowExposed(&window))
        QSKIP("the window was never exposed -- no compositor for this run");

    QTest::mouseMove(&window, QPoint(100, 60));
    QTRY_COMPARE(window.cursor().shape(), Qt::IBeamCursor);
}

QTEST_MAIN(QTermPointerShapeTest)
#include "QTermPointerShapeTest.moc"
