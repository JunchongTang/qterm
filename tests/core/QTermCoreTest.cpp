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
    void togglesCursorVisibilityMode();
    void togglesBracketedPasteMode();
    void togglesApplicationCursorKeysMode();
    void switchesToAlternateScreen();
    void restoresPrimaryScreenFromAlternate();
    void combinesNonSpacingMarks();
    void storesWideCharactersAcrossTwoCells();
    void keepsNonBmpWideCharactersAcrossWrites();
    void updatesWindowTitleFromOscBel();
    void keepsPartialOscTitleStateAcrossWrites();
    void emitsBellWithoutChangingBuffer();
    void supports256ColorSgrAttributes();
    void supportsTrueColorSgrAttributes();
    void encodesCursorKeysByMode();
    void encodesBracketedPasteByMode();
    void emitsOutboundDataForKeyAndPaste();
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

    core.writePlainText("\x1b[1;2;4;7;9;31mA"_L1);

    const QTermCell &cell = core.buffer().lineAt(0).cellAt(0);
    QCOMPARE(cell.text, "A"_L1);
    QVERIFY(cell.attributes.bold);
    QVERIFY(cell.attributes.dim);
    QVERIFY(cell.attributes.underline);
    QVERIFY(cell.attributes.inverse);
    QVERIFY(cell.attributes.strikethrough);
    QCOMPARE(cell.attributes.foregroundIndex, 1);
}

void QTermCoreTest::resetsSgrAttributes()
{
    QTermCore core;

    core.writePlainText("\x1b[1;2;7;9;31mA\x1b[22;27;29;39mB"_L1);

    const QTermCell &firstCell = core.buffer().lineAt(0).cellAt(0);
    const QTermCell &secondCell = core.buffer().lineAt(0).cellAt(1);
    QVERIFY(firstCell.attributes.bold);
    QVERIFY(firstCell.attributes.dim);
    QVERIFY(firstCell.attributes.inverse);
    QVERIFY(firstCell.attributes.strikethrough);
    QCOMPARE(firstCell.attributes.foregroundIndex, 1);
    QVERIFY(!secondCell.attributes.bold);
    QVERIFY(!secondCell.attributes.dim);
    QVERIFY(!secondCell.attributes.inverse);
    QVERIFY(!secondCell.attributes.strikethrough);
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

void QTermCoreTest::togglesCursorVisibilityMode()
{
    QTermCore core;

    core.writePlainText("\x1b[?25l"_L1);
    QVERIFY(!core.modeState().cursorVisible);

    core.writePlainText("\x1b[?25h"_L1);
    QVERIFY(core.modeState().cursorVisible);
}

void QTermCoreTest::togglesBracketedPasteMode()
{
    QTermCore core;

    core.writePlainText("\x1b[?2004h"_L1);
    QVERIFY(core.modeState().bracketedPaste);

    core.writePlainText("\x1b[?2004l"_L1);
    QVERIFY(!core.modeState().bracketedPaste);
}

void QTermCoreTest::togglesApplicationCursorKeysMode()
{
    QTermCore core;

    core.writePlainText("\x1b[?1h"_L1);
    QVERIFY(core.modeState().applicationCursorKeys);

    core.writePlainText("\x1b[?1l"_L1);
    QVERIFY(!core.modeState().applicationCursorKeys);
}

void QTermCoreTest::switchesToAlternateScreen()
{
    QTermCore core;

    core.writePlainText("main\x1b[?1049halt"_L1);

    QVERIFY(core.modeState().alternateScreenActive);
    QCOMPARE(core.debugPlainText(), "alt"_L1);
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 3);
}

void QTermCoreTest::restoresPrimaryScreenFromAlternate()
{
    QTermCore core;

    core.writePlainText("main"_L1);
    core.writePlainText("\x1b[?1049halt"_L1);
    core.writePlainText("\x1b[?1049l"_L1);

    QVERIFY(!core.modeState().alternateScreenActive);
    QCOMPARE(core.debugPlainText(), "main"_L1);
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 4);
}

void QTermCoreTest::combinesNonSpacingMarks()
{
    QTermCore core;
    const QString composed = QString::fromUtf8(u8"A\u0301B");
    const QString firstCellText = QString::fromUtf8(u8"A\u0301");

    core.writePlainText(composed);

    QCOMPARE(core.debugPlainText(), composed);
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 2);
    QCOMPARE(core.buffer().lineAt(0).cellAt(0).text, firstCellText);
}

void QTermCoreTest::storesWideCharactersAcrossTwoCells()
{
    QTermCore core;
    const QString text = QString::fromUtf8(u8"中a");
    const QString wideChar = QString::fromUtf8(u8"中");

    core.writePlainText(text);

    QCOMPARE(core.debugPlainText(), text);
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 3);
    QCOMPARE(core.buffer().lineAt(0).cellAt(0).text, wideChar);
    QCOMPARE(core.buffer().lineAt(0).cellAt(0).width, 2);
    QVERIFY(core.buffer().lineAt(0).cellAt(1).continuation);
    QCOMPARE(core.buffer().lineAt(0).cellAt(2).text, "a"_L1);
}

void QTermCoreTest::updatesWindowTitleFromOscBel()
{
    QTermCore core;
    QSignalSpy titleSpy(&core, &QTermCore::titleChanged);

    core.writePlainText("\x1b]0;qterm-title\x07"_L1);

    QCOMPARE(core.title(), "qterm-title"_L1);
    QCOMPARE(titleSpy.size(), 1);
    QCOMPARE(titleSpy.at(0).at(0).toString(), "qterm-title"_L1);
}

void QTermCoreTest::keepsPartialOscTitleStateAcrossWrites()
{
    QTermCore core;

    core.writePlainText("\x1b]2;split"_L1);
    QCOMPARE(core.title(), QString());

    core.writePlainText("-title\x1b\\"_L1);
    QCOMPARE(core.title(), "split-title"_L1);
}

void QTermCoreTest::keepsNonBmpWideCharactersAcrossWrites()
{
    QTermCore core;
    const QString emoji = QString::fromUcs4(U"😀");

    core.writePlainText(emoji.left(1));
    core.writePlainText(emoji.mid(1) + "a"_L1);

    QCOMPARE(core.debugPlainText(), emoji + "a"_L1);
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 3);
    QCOMPARE(core.buffer().lineAt(0).cellAt(0).text, emoji);
    QCOMPARE(core.buffer().lineAt(0).cellAt(0).width, 2);
    QVERIFY(core.buffer().lineAt(0).cellAt(1).continuation);
}

void QTermCoreTest::emitsBellWithoutChangingBuffer()
{
    QTermCore core;
    QSignalSpy bellSpy(&core, &QTermCore::bell);

    core.writePlainText("a\ab"_L1);

    QCOMPARE(bellSpy.size(), 1);
    QCOMPARE(core.debugPlainText(), "ab"_L1);
}

void QTermCoreTest::supports256ColorSgrAttributes()
{
    QTermCore core;

    core.writePlainText("\x1b[38;5;196;48;5;33mA"_L1);

    const QTermCell &cell = core.buffer().lineAt(0).cellAt(0);
    QCOMPARE(cell.text, "A"_L1);
    QCOMPARE(cell.attributes.foregroundIndex, 196);
    QCOMPARE(cell.attributes.foregroundRgb, -1);
    QCOMPARE(cell.attributes.backgroundIndex, 33);
    QCOMPARE(cell.attributes.backgroundRgb, -1);
}

void QTermCoreTest::supportsTrueColorSgrAttributes()
{
    QTermCore core;

    core.writePlainText("\x1b[38;2;12;34;56;48;2;200;210;220mA"_L1);

    const QTermCell &cell = core.buffer().lineAt(0).cellAt(0);
    QCOMPARE(cell.text, "A"_L1);
    QCOMPARE(cell.attributes.foregroundIndex, -1);
    QCOMPARE(cell.attributes.foregroundRgb, 0x0c2238);
    QCOMPARE(cell.attributes.backgroundIndex, -1);
    QCOMPARE(cell.attributes.backgroundRgb, 0xc8d2dc);
}

void QTermCoreTest::encodesCursorKeysByMode()
{
    QTermCore core;

    QCOMPARE(core.encodeKey(Qt::Key_Up), QByteArray("\x1b[A"));
    QCOMPARE(core.encodeKey(Qt::Key_Home), QByteArray("\x1b[H"));

    core.writePlainText("\x1b[?1h"_L1);

    QCOMPARE(core.encodeKey(Qt::Key_Up), QByteArray("\x1bOA"));
    QCOMPARE(core.encodeKey(Qt::Key_Home), QByteArray("\x1bOH"));
    QCOMPARE(core.encodeKey(Qt::Key_A, "a"_L1), QByteArray("a"));
    QCOMPARE(core.encodeKey(Qt::Key_Return), QByteArray("\r"));
}

void QTermCoreTest::encodesBracketedPasteByMode()
{
    QTermCore core;
    const QString pastedText = QString::fromUtf8(u8"中a\n");

    QCOMPARE(core.encodePaste(pastedText), pastedText.toUtf8());

    core.writePlainText("\x1b[?2004h"_L1);

    QCOMPARE(core.encodePaste(pastedText), QByteArray("\x1b[200~") + pastedText.toUtf8() + QByteArray("\x1b[201~"));
}

void QTermCoreTest::emitsOutboundDataForKeyAndPaste()
{
    QTermCore core;
    QSignalSpy spy(&core, &QTermCore::outboundData);

    core.writePlainText("\x1b[?1h\x1b[?2004h"_L1);
    core.sendKey(Qt::Key_Up);
    core.sendPaste("hi"_L1);

    QCOMPARE(spy.size(), 2);
    QCOMPARE(spy.at(0).at(0).toByteArray(), QByteArray("\x1bOA"));
    QCOMPARE(spy.at(1).at(0).toByteArray(), QByteArray("\x1b[200~hi\x1b[201~"));
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