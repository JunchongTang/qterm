// When a selection gesture *ends*, and who decides what the middle button means.
//
// Both signals exist because the library cannot answer two questions the host must:
//
//   * "Has the user finished picking a region?" -- the surface model only carries the
//     *state* (`hasSelection`), and that flips true on the **first** mouse move of a
//     drag. A host watching the state therefore copies whatever was selected at that
//     instant and never hears about the rest of the drag: the clipboard ends up with
//     the first couple of characters of a much longer selection. It is a state, but
//     the host needs the event. That bug is what `selectionFinished` fixes, and the
//     "still extending" case below is the one that used to fire too early.
//
//   * "What does a middle click do?" -- on X11 it pastes the *primary selection*, on
//     the Windows console it pastes the clipboard, and some macOS terminals open a
//     URL with it. None of that is the library's business, and it must not read the
//     clipboard on its own (see clipboardWriteRequested for the other direction), so
//     it reports the gesture and stops there.
#include <QtTest>

#include <QMouseEvent>

#include <QTerm/QTermQuickItem.h>
#include <QTerm/QTermQuickPaintedItem.h>
#include <QTerm/QTermTerminal.h>
#include <QTerm/QTermWidget.h>

using namespace QTerm;

class QTermSelectionFinishedTest : public QObject
{
    Q_OBJECT

private slots:
    void aDragIsAnnouncedOnceWhenTheButtonComesUp();
    void aDragIsNotAnnouncedWhileItIsStillExtending();
    void aBareClickAnnouncesNothing();
    void aClickClearsTheSelection();
    void aDoubleClickAnnouncesTheWord();
    void aSelectionTheHostSetItselfIsNotAnnounced();
    void theMiddleButtonIsReportedWhenNoApplicationGrabbedTheMouse();
    void anApplicationWithTheMouseKeepsTheMiddleButton();
    void allThreeViewsCarryBothSignals();
};

namespace {

//! The x coordinate that lands in the middle of \a column.
qreal xOfColumn(const QTermQuickItem &item, int column)
{
    return (column + 0.5) * item.cellWidth();
}

//! y of the first row -- row 0 is enough for every case here.
constexpr qreal kFirstRowY = 5.0;

QMouseEvent pressAt(const QPointF &pos, Qt::MouseButton button)
{
    return QMouseEvent(QEvent::MouseButtonPress, pos, pos, button, button, Qt::NoModifier);
}

QMouseEvent dragTo(const QPointF &pos)
{
    // A move carries no button of its own, but the buttons that are *held* -- that is
    // what the controller tests before it extends a selection at all.
    return QMouseEvent(QEvent::MouseMove, pos, pos, Qt::NoButton, Qt::LeftButton,
                       Qt::NoModifier);
}

QMouseEvent releaseAt(const QPointF &pos)
{
    return QMouseEvent(QEvent::MouseButtonRelease, pos, pos, Qt::LeftButton,
                       Qt::NoButton, Qt::NoModifier);
}

} // namespace

// The gesture the bug was about: drag across a region, let go, get told once -- with
// everything the region covers, not with what was selected when the drag started.
void QTermSelectionFinishedTest::aDragIsAnnouncedOnceWhenTheButtonComesUp()
{
    QTermTerminal terminal;
    QTermQuickItem item;
    item.setTerminal(&terminal);
    item.setSize(QSizeF(400, 200));
    terminal.feedText(QStringLiteral("hello world\r\n"));

    QSignalSpy spy(&item, &QTermQuickItem::selectionFinished);

    QMouseEvent press = pressAt(QPointF(xOfColumn(item, 0), kFirstRowY), Qt::LeftButton);
    QCoreApplication::sendEvent(&item, &press);
    QMouseEvent release = releaseAt(QPointF(xOfColumn(item, 5), kFirstRowY));
    QMouseEvent move = dragTo(QPointF(xOfColumn(item, 5), kFirstRowY));
    QCoreApplication::sendEvent(&item, &move);
    QCoreApplication::sendEvent(&item, &release);

    QCOMPARE(spy.count(), 1);
    // Releasing *on* a cell includes it -- the drag went through column 5, so the
    // region is columns 0..5 and the space comes along. Pinned here because the
    // endpoint being inclusive is exactly the kind of thing an off-by-one hides in.
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("hello "));
    // And it is the whole region: the same thing the host would read off the model.
    QCOMPARE(spy.first().at(0).toString(), terminal.surfaceModel()->selectedText());
}

// The case the old copy-on-select logic got wrong. Halfway through a drag there *is*
// a selection -- a short one -- and the host used to copy exactly that.
void QTermSelectionFinishedTest::aDragIsNotAnnouncedWhileItIsStillExtending()
{
    QTermTerminal terminal;
    QTermQuickItem item;
    item.setTerminal(&terminal);
    item.setSize(QSizeF(400, 200));
    terminal.feedText(QStringLiteral("hello world\r\n"));

    QSignalSpy spy(&item, &QTermQuickItem::selectionFinished);

    QMouseEvent press = pressAt(QPointF(xOfColumn(item, 0), kFirstRowY), Qt::LeftButton);
    QCoreApplication::sendEvent(&item, &press);
    QMouseEvent move = dragTo(QPointF(xOfColumn(item, 3), kFirstRowY));
    QCoreApplication::sendEvent(&item, &move);

    // A selection exists *and is not empty* -- so "nothing was announced" is not just
    // "nothing was selected", which is exactly the distinction the host needs.
    QVERIFY(terminal.surfaceModel()->hasSelection());
    QVERIFY(!terminal.surfaceModel()->selectedText().isEmpty());
    QCOMPARE(spy.count(), 0);
}

// A click on its own clears the selection. That is a gesture ending too, but there is
// nothing to copy -- and announcing an empty region would have every host guard
// against writing an empty string to the clipboard.
void QTermSelectionFinishedTest::aBareClickAnnouncesNothing()
{
    QTermTerminal terminal;
    QTermQuickItem item;
    item.setTerminal(&terminal);
    item.setSize(QSizeF(400, 200));
    terminal.feedText(QStringLiteral("hello world\r\n"));

    QSignalSpy spy(&item, &QTermQuickItem::selectionFinished);

    const QPointF pos(xOfColumn(item, 4), kFirstRowY);
    QMouseEvent press = pressAt(pos, Qt::LeftButton);
    QCoreApplication::sendEvent(&item, &press);
    QMouseEvent release = releaseAt(pos);
    QCoreApplication::sendEvent(&item, &release);

    QCOMPARE(spy.count(), 0);
}

/*!
    **A single click clears the selection** -- every case of it: inside the selected
    region, outside it, on another row, and with the pixel of jitter a real click always
    has between press and release.

    This is what a user reaches for to *undo* a selection, and it is the press that does
    the work (`handleMousePress` -> `clearSelection()`), with the release then asked to
    select the interval [cell, cell] -- which `setSelectionFromDragCells` resolves to
    "nothing" by construction. Both halves have to hold: a release that "selects" the
    cell it was clicked on would leave a one-character highlight behind, which reads as
    "clicking did not deselect".

    It sits here rather than in the signal tests because it shares the fixture and the
    very same path -- and because a report of "clicking again does not deselect" is worth
    a test even though the reported case could not be reproduced in this fixture.
*/
void QTermSelectionFinishedTest::aClickClearsTheSelection()
{
    QTermTerminal terminal;
    QTermQuickItem item;
    item.setTerminal(&terminal);
    item.setSize(QSizeF(400, 200));
    terminal.feedText(QStringLiteral("hello world\r\nsecond line\r\n"));

    const auto dragSelect = [&](int fromColumn, int toColumn) {
        QMouseEvent press = pressAt(QPointF(xOfColumn(item, fromColumn), kFirstRowY),
                                    Qt::LeftButton);
        QCoreApplication::sendEvent(&item, &press);
        QMouseEvent move = dragTo(QPointF(xOfColumn(item, toColumn), kFirstRowY));
        QCoreApplication::sendEvent(&item, &move);
        QMouseEvent release = releaseAt(QPointF(xOfColumn(item, toColumn), kFirstRowY));
        QCoreApplication::sendEvent(&item, &release);
        QVERIFY2(terminal.surfaceModel()->hasSelection(), "the drag itself did not select");
    };
    const auto clickAt = [&](int column, qreal dy) {
        const QPointF pos(xOfColumn(item, column), kFirstRowY + dy);
        QMouseEvent press = pressAt(pos, Qt::LeftButton);
        QCoreApplication::sendEvent(&item, &press);
        QMouseEvent release = releaseAt(pos);
        QCoreApplication::sendEvent(&item, &release);
    };
    const auto verifyCleared = [&](const char *what) {
        QVERIFY2(!terminal.surfaceModel()->hasSelection(), what);
        QVERIFY2(terminal.surfaceModel()->selectedText().isEmpty(), what);
    };

    dragSelect(0, 5);
    clickAt(3, 0);                       // inside the selection
    verifyCleared("clicking inside the selection left it selected");

    dragSelect(0, 5);
    clickAt(9, 0);                       // outside it, same row
    verifyCleared("clicking outside the selection left it selected");

    dragSelect(0, 5);
    clickAt(3, 1.0);                     // the pixel of jitter a real click has
    verifyCleared("a click that jitters by a pixel left a one-character selection");

    dragSelect(0, 5);
    clickAt(3, 30.0);                    // another row
    verifyCleared("clicking on another row left the selection behind");
}

// A double click builds its word selection in the double-click handler and suppresses
// the release that follows so the word is not thrown away. The gesture still ends at
// that release, and that is where the host hears about it -- one place, not three.
void QTermSelectionFinishedTest::aDoubleClickAnnouncesTheWord()
{
    QTermTerminal terminal;
    QTermQuickItem item;
    item.setTerminal(&terminal);
    item.setSize(QSizeF(400, 200));
    terminal.feedText(QStringLiteral("hello world\r\n"));

    QSignalSpy spy(&item, &QTermQuickItem::selectionFinished);

    const QPointF pos(xOfColumn(item, 1), kFirstRowY);
    QMouseEvent dbl(QEvent::MouseButtonDblClick, pos, pos, Qt::LeftButton, Qt::LeftButton,
                    Qt::NoModifier);
    QCoreApplication::sendEvent(&item, &dbl);
    QMouseEvent release = releaseAt(pos);
    QCoreApplication::sendEvent(&item, &release);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("hello"));
}

// Selections the host asked for are commands, not gestures: it already knows. Firing
// here would make "select all" copy twice over (once from the command, once from the
// announcement) for a host that copies on select.
void QTermSelectionFinishedTest::aSelectionTheHostSetItselfIsNotAnnounced()
{
    QTermTerminal terminal;
    QTermQuickItem item;
    item.setTerminal(&terminal);
    item.setSize(QSizeF(400, 200));
    terminal.feedText(QStringLiteral("hello world\r\n"));

    QSignalSpy spy(&item, &QTermQuickItem::selectionFinished);
    terminal.selectAll();

    QVERIFY(terminal.surfaceModel()->hasSelection());
    QCOMPARE(spy.count(), 0);
}

// No application has grabbed the mouse: the button is the host's, and the host decides
// what pasting means. Note what is *not* here -- the library does not touch the
// clipboard, and it does not send anything to the far end either.
void QTermSelectionFinishedTest::theMiddleButtonIsReportedWhenNoApplicationGrabbedTheMouse()
{
    QTermTerminal terminal;
    QTermQuickItem item;
    item.setTerminal(&terminal);
    item.setSize(QSizeF(400, 200));

    QSignalSpy clicked(&item, &QTermQuickItem::middleButtonPressed);
    QSignalSpy sent(&terminal, &QTermTerminal::outboundData);

    QMouseEvent press = pressAt(QPointF(20, kFirstRowY), Qt::MiddleButton);
    QCoreApplication::sendEvent(&item, &press);

    QCOMPARE(clicked.count(), 1);
    QCOMPARE(sent.count(), 0);
    QVERIFY(press.isAccepted());
}

// ...and while an application has taken the mouse over (DECSET 1002 -- what vim and
// htop turn on) the button belongs to that application. Emitting here as well would
// paste into the shell *and* send the click on, which breaks every mouse-driven TUI.
void QTermSelectionFinishedTest::anApplicationWithTheMouseKeepsTheMiddleButton()
{
    QTermTerminal terminal;
    QTermQuickItem item;
    item.setTerminal(&terminal);
    item.setSize(QSizeF(400, 200));
    terminal.feedText(QStringLiteral("\x1b[?1002h"));

    QSignalSpy clicked(&item, &QTermQuickItem::middleButtonPressed);
    QSignalSpy sent(&terminal, &QTermTerminal::outboundData);

    QMouseEvent press = pressAt(QPointF(20, kFirstRowY), Qt::MiddleButton);
    QCoreApplication::sendEvent(&item, &press);

    QCOMPARE(clicked.count(), 0);
    QCOMPARE(sent.count(), 1);
}

// The three view types carry the same API -- a widget host gets copy-on-select and
// middle-click paste on the same terms a QML host does. Constructing the spies is the
// check: it does not compile unless both signals exist on each type.
void QTermSelectionFinishedTest::allThreeViewsCarryBothSignals()
{
    QTermQuickPaintedItem painted;
    QSignalSpy paintedSelection(&painted, &QTermQuickPaintedItem::selectionFinished);
    QSignalSpy paintedMiddle(&painted, &QTermQuickPaintedItem::middleButtonPressed);
    QCOMPARE(paintedSelection.count(), 0);
    QCOMPARE(paintedMiddle.count(), 0);

    QTermWidget widget;
    QSignalSpy widgetSelection(&widget, &QTermWidget::selectionFinished);
    QSignalSpy widgetMiddle(&widget, &QTermWidget::middleButtonPressed);
    QCOMPARE(widgetSelection.count(), 0);
    QCOMPARE(widgetMiddle.count(), 0);
}

QTEST_MAIN(QTermSelectionFinishedTest)
#include "QTermSelectionFinishedTest.moc"
