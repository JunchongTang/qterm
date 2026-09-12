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

#include <QQmlApplicationEngine>
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
    void aQmlCreatedTerminalAlsoGetsTheBeam();
    void aHostCanPinTheShapeAndHandItBack();
    void rightClickAsksTheHostForAMenu();
    void anApplicationWithTheMouseKeepsTheRightButton();
    void thePaintedItemCarriesTheSameApi();
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

// The cases above pin the item's own cursor. This one pins what the user actually
// sees, and it is built the way a host builds it: the QML engine creates the item,
// and a right-click MouseArea is laid over the terminal.
//
// That overlay is the whole point. **A MouseArea claims the pointer even when it
// never assigns a cursorShape**, so it silently overrode the beam the terminal sets
// on itself -- every C++-level check passed while the running application still
// showed an arrow. The fix is for the overlay to carry the shape, which is why
// QTermTerminal exposes mouseProtocolActive.
//
// **Only one window case on purpose.** Two of them in the same process interfere:
// the window recomputes its cursor from the real pointer position, so the second
// one reads a stale value and fails (or takes ten seconds) while passing alone.
void QTermPointerShapeTest::aQmlCreatedTerminalAlsoGetsTheBeam()
{
    QQmlApplicationEngine engine;
    engine.loadData(R"(
        import QtQuick
        import QTerm
        Window {
            width: 200; height: 120; visible: true
            QTermTerminal { id: term }
            QTermQuickItem { anchors.fill: parent; terminal: term }
            // **No MouseArea over the terminal.** Hosts used to lay one there for the
            // right-click menu, and it silently replaced the beam with an arrow: a
            // MouseArea claims the pointer even when it assigns no cursorShape. The
            // menu now arrives as contextMenuRequested, so the overlay -- and with it
            // the whole class of bug -- is gone.
        }
    )");
    QVERIFY2(!engine.rootObjects().isEmpty(), "the QML did not load -- import path wrong?");

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    if (!QTest::qWaitForWindowExposed(window))
        QSKIP("the window was never exposed -- no compositor for this run");

    QTest::mouseMove(window, QPoint(100, 60));
    QTRY_COMPARE(window->cursor().shape(), Qt::IBeamCursor);
}

// A host that wants a different pointer must be able to say so: the automatic shape
// is a default, not a decree. Assigning pins it (even against the mouse protocol),
// resetting hands control back.
void QTermPointerShapeTest::aHostCanPinTheShapeAndHandItBack()
{
    QTermTerminal terminal;
    QTermQuickItem item;
    item.setTerminal(&terminal);

    item.setCursorShape(Qt::PointingHandCursor);
    QCOMPARE(item.cursorShape(), Qt::PointingHandCursor);

    // The protocol flipping must not walk over the host's choice.
    terminal.feedText(QStringLiteral("\x1b[?1002h"));
    QCOMPARE(item.cursorShape(), Qt::PointingHandCursor);

    item.resetCursorShape();
    QCOMPARE(item.cursorShape(), Qt::ArrowCursor);   // back under the protocol
    terminal.feedText(QStringLiteral("\x1b[?1002l"));
    QCOMPARE(item.cursorShape(), Qt::IBeamCursor);
}

// The right button raises a signal carrying the cell under it, so a host can pop a
// menu without covering the terminal with a MouseArea.
void QTermPointerShapeTest::rightClickAsksTheHostForAMenu()
{
    QTermTerminal terminal;
    QTermQuickItem item;
    item.setTerminal(&terminal);
    item.setSize(QSizeF(200, 120));

    QSignalSpy spy(&item, &QTermQuickItem::contextMenuRequested);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10),
                      Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&item, &press);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toPointF(), QPointF(10, 10));
    QVERIFY(press.isAccepted());
}

// ... except while an application has taken the mouse over: there the click belongs
// to the program, and a menu would eat it.
void QTermPointerShapeTest::anApplicationWithTheMouseKeepsTheRightButton()
{
    QTermTerminal terminal;
    QTermQuickItem item;
    item.setTerminal(&terminal);
    item.setSize(QSizeF(200, 120));
    terminal.feedText(QStringLiteral("\x1b[?1002h"));

    QSignalSpy spy(&item, &QTermQuickItem::contextMenuRequested);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10),
                      Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&item, &press);

    QCOMPARE(spy.count(), 0);
}

// The painted renderer is a separate implementation of the same two APIs, so it
// needs its own check -- the first cut of it silently never landed and every other
// test here stayed green, because they all exercise QTermQuickItem.
void QTermPointerShapeTest::thePaintedItemCarriesTheSameApi()
{
    QTermTerminal terminal;
    QTermQuickPaintedItem item;
    item.setTerminal(&terminal);
    item.setSize(QSizeF(200, 120));

    item.setCursorShape(Qt::PointingHandCursor);
    QCOMPARE(item.cursorShape(), Qt::PointingHandCursor);
    item.resetCursorShape();
    QCOMPARE(item.cursorShape(), Qt::IBeamCursor);

    QSignalSpy spy(&item, &QTermQuickPaintedItem::contextMenuRequested);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10),
                      Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&item, &press);
    QCOMPARE(spy.count(), 1);

    // ... and the same exception while an application owns the mouse.
    terminal.feedText(QStringLiteral("\x1b[?1002h"));
    QMouseEvent grabbed(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10),
                        Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&item, &grabbed);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(item.cursorShape(), Qt::ArrowCursor);
}

QTEST_MAIN(QTermPointerShapeTest)
#include "QTermPointerShapeTest.moc"
