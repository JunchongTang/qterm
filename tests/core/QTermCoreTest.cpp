#include <QtTest>

#include "core/QTermCore.h"

using namespace Qt::StringLiterals;

namespace QTerm {

class QTermCoreTest : public QObject
{
    Q_OBJECT

private slots:
    void usesDefaultTerminalSize();
    void clampsMinimumTerminalSize();
    void tracksCrlfAsLineBreak();
    void preservesColumnOnLineFeed();
    void overwritesCellsAfterCarriageReturn();
    void wrapsCursorByColumnCount();
    void supportsCsiCursorBackward();
    void keepsPartialCsiStateAcrossWrites();
    void supportsCsiCursorPosition();
    void supportsCsiEraseInLine();
    void supportsCsiEraseInDisplay();
    void preservesSgrAttributesOnCells();
    void resetsSgrAttributes();
    void supportsInsertCharacters();
    void supportsDeleteCharacters();
    void supportsInsertLines();
    void supportsDeleteLines();
    void supportsSaveAndRestoreCursor();
    void supportsEscapeSaveAndRestoreCursor();
    void supportsScrollRegionDuringLineFeed();
    void clearsState();
};

void QTermCoreTest::usesDefaultTerminalSize()
{
    QTermCore core;

    QCOMPARE(core.columns(), 80);
    QCOMPARE(core.rows(), 24);
    QCOMPARE(core.debugPlainText(), QString());
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 0);
}

void QTermCoreTest::clampsMinimumTerminalSize()
{
    QTermCore core;

    core.setTerminalSize(0, 0);

    QCOMPARE(core.columns(), 2);
    QCOMPARE(core.rows(), 1);
}

void QTermCoreTest::tracksCrlfAsLineBreak()
{
    QTermCore core;

    core.writePlainText("ab\r\ncd"_L1);

    QCOMPARE(core.debugPlainText(), "ab\ncd"_L1);
    QCOMPARE(core.cursorState().row, 1);
    QCOMPARE(core.cursorState().column, 2);
}

void QTermCoreTest::preservesColumnOnLineFeed()
{
    QTermCore core;

    core.writePlainText("ab\ncd"_L1);

    QCOMPARE(core.debugPlainText(), "ab\n  cd"_L1);
    QCOMPARE(core.cursorState().row, 1);
    QCOMPARE(core.cursorState().column, 4);
}

void QTermCoreTest::overwritesCellsAfterCarriageReturn()
{
    QTermCore core;

    core.writePlainText("abc\rZ"_L1);

    QCOMPARE(core.debugPlainText(), "Zbc"_L1);
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 1);
}

void QTermCoreTest::wrapsCursorByColumnCount()
{
    QTermCore core;
    core.setTerminalSize(4, 24);

    core.writePlainText("abcde"_L1);

    QCOMPARE(core.debugPlainText(), "abcde"_L1);
    QCOMPARE(core.cursorState().row, 1);
    QCOMPARE(core.cursorState().column, 1);
}

void QTermCoreTest::supportsCsiCursorBackward()
{
    QTermCore core;

    core.writePlainText("abcd\x1b[2DXY"_L1);

    QCOMPARE(core.debugPlainText(), "abXY"_L1);
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 4);
}

void QTermCoreTest::keepsPartialCsiStateAcrossWrites()
{
    QTermCore core;

    core.writePlainText("abcd"_L1);
    core.writePlainText("\x1b[2"_L1);
    core.writePlainText("DXY"_L1);

    QCOMPARE(core.debugPlainText(), "abXY"_L1);
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 4);
}

void QTermCoreTest::supportsCsiCursorPosition()
{
    QTermCore core;

    core.writePlainText("ab\r\ncd\x1b[1;1HXY"_L1);

    QCOMPARE(core.debugPlainText(), "XY\ncd"_L1);
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 2);
}

void QTermCoreTest::supportsCsiEraseInLine()
{
    QTermCore core;

    core.writePlainText("abcde\x1b[2D\x1b[K"_L1);

    QCOMPARE(core.debugPlainText(), "abc"_L1);
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 3);
}

void QTermCoreTest::supportsCsiEraseInDisplay()
{
    QTermCore core;

    core.writePlainText("ab\r\ncd\x1b[2J"_L1);

    QCOMPARE(core.debugPlainText(), QString());
    QCOMPARE(core.cursorState().row, 1);
    QCOMPARE(core.cursorState().column, 2);
}

void QTermCoreTest::preservesSgrAttributesOnCells()
{
    QTermCore core;

    core.writePlainText("\x1b[1;4;31mA"_L1);

    const QTermCell &cell = core.buffer().lineAt(0).cellAt(0);
    QCOMPARE(cell.text, "A"_L1);
    QVERIFY(cell.attributes.bold);
    QVERIFY(cell.attributes.underline);
    QCOMPARE(cell.attributes.foregroundIndex, 1);
}

void QTermCoreTest::resetsSgrAttributes()
{
    QTermCore core;

    core.writePlainText("\x1b[1;31mA\x1b[0mB"_L1);

    const QTermCell &firstCell = core.buffer().lineAt(0).cellAt(0);
    const QTermCell &secondCell = core.buffer().lineAt(0).cellAt(1);
    QVERIFY(firstCell.attributes.bold);
    QCOMPARE(firstCell.attributes.foregroundIndex, 1);
    QVERIFY(!secondCell.attributes.bold);
    QCOMPARE(secondCell.attributes.foregroundIndex, -1);
}

void QTermCoreTest::supportsInsertCharacters()
{
    QTermCore core;

    core.writePlainText("abcd\x1b[3D\x1b[@Z"_L1);

    QCOMPARE(core.debugPlainText(), "aZbcd"_L1);
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 2);
}

void QTermCoreTest::supportsDeleteCharacters()
{
    QTermCore core;

    core.writePlainText("abcd\x1b[3D\x1b[P"_L1);

    QCOMPARE(core.debugPlainText(), "acd"_L1);
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 1);
}

void QTermCoreTest::supportsInsertLines()
{
    QTermCore core;

    core.writePlainText("1\r\n2\r\n3\x1b[2;1H\x1b[LX"_L1);

    QCOMPARE(core.debugPlainText(), "1\nX\n2\n3"_L1);
    QCOMPARE(core.cursorState().row, 1);
    QCOMPARE(core.cursorState().column, 1);
}

void QTermCoreTest::supportsDeleteLines()
{
    QTermCore core;

    core.writePlainText("1\r\n2\r\n3\x1b[2;1H\x1b[M"_L1);

    QCOMPARE(core.debugPlainText(), "1\n3"_L1);
    QCOMPARE(core.cursorState().row, 1);
    QCOMPARE(core.cursorState().column, 0);
}

void QTermCoreTest::supportsSaveAndRestoreCursor()
{
    QTermCore core;

    core.writePlainText("ab\x1b[s\x1b[1;1HXY\x1b[uZ"_L1);

    QCOMPARE(core.debugPlainText(), "XYZ"_L1);
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 3);
}

void QTermCoreTest::supportsEscapeSaveAndRestoreCursor()
{
    QTermCore core;

    core.writePlainText("ab\x1b" "7\x1b[1;1HXY\x1b" "8Z"_L1);

    QCOMPARE(core.debugPlainText(), "XYZ"_L1);
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 3);
}

void QTermCoreTest::supportsScrollRegionDuringLineFeed()
{
    QTermCore core;

    core.writePlainText("1\r\n2\r\n3\x1b[2;3r\x1b[3;1H\nX"_L1);

    QCOMPARE(core.debugPlainText(), "1\n3\nX"_L1);
    QCOMPARE(core.cursorState().row, 2);
    QCOMPARE(core.cursorState().column, 1);
}

void QTermCoreTest::clearsState()
{
    QTermCore core;
    core.writePlainText("bootstrap"_L1);

    core.clear();

    QCOMPARE(core.debugPlainText(), QString());
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 0);
}

} // namespace QTerm

QTEST_MAIN(QTerm::QTermCoreTest)

#include "QTermCoreTest.moc"