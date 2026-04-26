#include <QtTest>

#include "core/QTermCore.h"
#include "core/QTermInputEncoder.h"

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
    void ignoresUnsupportedEscapeFinals();
    void supportsScrollRegionDuringLineFeed();
    void preservesScrollbackAcrossFullScreenLineFeed();
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
    void reflowsVisibleContentWhenNarrowing();
    void reflowsBackWhenWidening();
    void reflowsCombiningCharactersAcrossResize();
    void preservesWrappedBoundarySemanticsAcrossReflow();
    void reflowsWideCharactersAcrossResize();
    void preservesWideWrappedBoundarySemanticsAcrossReflow();
    void preservesComplexPromptAcrossRepeatedResizeCycles();
    void preservesTreeListingAcrossRepeatedResizeCycles();
    void preservesMultiplePromptLinesAcrossExtremeResizeCycles();
    void preservesContentAcrossRepeatedNarrowWithRedraw();
    void preservesContentAcrossOscillatingResize();
    void preservesContentWhenResizeInterleavesMidRedraw();
    void clearsState();
    void togglesMouseModeX10();
    void togglesMouseModeSGR();
    void encodesMouseEventX10();
    void encodesMouseEventSGR();
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

void QTermCoreTest::ignoresUnsupportedEscapeFinals()
{
    QTermCore core;

    core.writePlainText("\x1b=\x1b>prompt"_L1);

    QCOMPARE(core.debugPlainText(), "prompt"_L1);
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 6);
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

void QTermCoreTest::preservesScrollbackAcrossFullScreenLineFeed()
{
    QTermCore core;

    core.setTerminalSize(5, 2);
    core.writePlainText("alpha\r\nbeta\r\ngamma"_L1);

    QCOMPARE(core.debugPlainText(), "alpha\nbeta\ngamma"_L1);
    QCOMPARE(core.buffer().lineAt(0).plainText(), "beta"_L1);
    QCOMPARE(core.buffer().lineAt(1).plainText(), "gamma"_L1);
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

void QTermCoreTest::reflowsVisibleContentWhenNarrowing()
{
    QTermCore core;

    core.setTerminalSize(10, 4);
    core.writePlainText("alpha beta"_L1);
    core.setTerminalSize(5, 4);

    QCOMPARE(core.debugPlainText(), "alpha beta"_L1);
    QCOMPARE(core.buffer().lineAt(0).plainText(), "alpha"_L1);
    QCOMPARE(core.buffer().lineAt(1).plainText(), " beta"_L1);
    QVERIFY(core.buffer().lineAt(0).wrappedToNextLine());
}

void QTermCoreTest::reflowsBackWhenWidening()
{
    QTermCore core;

    core.setTerminalSize(10, 4);
    core.writePlainText("abc"_L1);
    core.setTerminalSize(2, 4);
    QCOMPARE(core.cursorState().row, 1);
    QCOMPARE(core.cursorState().column, 1);

    core.setTerminalSize(5, 4);

    QCOMPARE(core.debugPlainText(), "abc"_L1);
    QCOMPARE(core.buffer().lineAt(0).plainText(), "abc"_L1);
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 3);
}

void QTermCoreTest::reflowsCombiningCharactersAcrossResize()
{
    QTermCore core;
    const QString text = QStringLiteral("e\u0301abc");

    core.setTerminalSize(4, 4);
    core.writePlainText(text);
    core.setTerminalSize(2, 4);

    QCOMPARE(core.debugPlainText(), text);
    QCOMPARE(core.buffer().lineAt(0).plainText(), QStringLiteral("e\u0301a"));
    QCOMPARE(core.buffer().lineAt(1).plainText(), "bc"_L1);

    core.setTerminalSize(4, 4);
    core.writePlainText("X"_L1);

    QCOMPARE(core.debugPlainText(), QStringLiteral("e\u0301abcX"));
    QCOMPARE(core.buffer().lineAt(0).plainText(), text);
    QCOMPARE(core.buffer().lineAt(1).plainText(), "X"_L1);
    QVERIFY(core.buffer().lineAt(0).wrappedToNextLine());
}

void QTermCoreTest::preservesWrappedBoundarySemanticsAcrossReflow()
{
    QTermCore core;

    core.setTerminalSize(5, 4);
    core.writePlainText("abcde"_L1);

    core.setTerminalSize(2, 4);
    QCOMPARE(core.debugPlainText(), "abcde"_L1);

    core.setTerminalSize(5, 4);
    core.writePlainText("X"_L1);

    QCOMPARE(core.debugPlainText(), "abcdeX"_L1);
    QCOMPARE(core.buffer().lineAt(0).plainText(), "abcde"_L1);
    QCOMPARE(core.buffer().lineAt(1).plainText(), "X"_L1);
    QVERIFY(core.buffer().lineAt(0).wrappedToNextLine());
}

void QTermCoreTest::reflowsWideCharactersAcrossResize()
{
    QTermCore core;
    const QString text = QString::fromUtf8(u8"中ab");

    core.setTerminalSize(4, 4);
    core.writePlainText(text);
    core.setTerminalSize(2, 4);

    QCOMPARE(core.debugPlainText(), text);
    QCOMPARE(core.buffer().lineAt(0).plainText(), QString::fromUtf8(u8"中"));
    QCOMPARE(core.buffer().lineAt(1).plainText(), "ab"_L1);

    core.setTerminalSize(4, 4);

    QCOMPARE(core.debugPlainText(), text);
    QCOMPARE(core.buffer().lineAt(0).plainText(), text);
}

void QTermCoreTest::preservesWideWrappedBoundarySemanticsAcrossReflow()
{
    QTermCore core;
    const QString text = QString::fromUtf8(u8"中ab");

    core.setTerminalSize(4, 4);
    core.writePlainText(text);
    core.setTerminalSize(2, 4);
    QCOMPARE(core.debugPlainText(), text);

    core.setTerminalSize(4, 4);
    core.writePlainText("X"_L1);

    QCOMPARE(core.debugPlainText(), QString::fromUtf8(u8"中abX"));
    QCOMPARE(core.buffer().lineAt(0).plainText(), text);
    QCOMPARE(core.buffer().lineAt(1).plainText(), "X"_L1);
    QVERIFY(core.buffer().lineAt(0).wrappedToNextLine());
}

void QTermCoreTest::preservesComplexPromptAcrossRepeatedResizeCycles()
{
    QTermCore core;
    const QString prompt = QStringLiteral("user@host:~/src$ echo alpha-beta/gamma?x=1&y=2");

    core.setTerminalSize(64, 6);
    core.writePlainText(prompt);

    core.setTerminalSize(11, 6);
    QCOMPARE(core.debugPlainText(), prompt);

    core.setTerminalSize(19, 6);
    QCOMPARE(core.debugPlainText(), prompt);

    core.setTerminalSize(7, 6);
    QCOMPARE(core.debugPlainText(), prompt);

    core.setTerminalSize(64, 6);

    QCOMPARE(core.debugPlainText(), prompt);
    QCOMPARE(core.buffer().lineAt(0).plainText(), prompt);
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, prompt.size());
}

void QTermCoreTest::preservesTreeListingAcrossRepeatedResizeCycles()
{
    QTermCore core;
    const QStringList lines = {
        QString::fromUtf8(u8"."),
        QString::fromUtf8(u8"├── cmake_install.cmake"),
        QString::fromUtf8(u8"├── CMakeFiles"),
        QString::fromUtf8(u8"│   ├── qterm_quick_demo_autogen.dir"),
        QString::fromUtf8(u8"│   │   ├── AutogenInfo.json"),
        QString::fromUtf8(u8"│   │   ├── AutogenUsed.txt"),
        QString::fromUtf8(u8"│   │   └── ParseCache.txt"),
        QString::fromUtf8(u8"│   └── qterm_quick_demo.dir"),
        QString::fromUtf8(u8"│       ├── main.cpp.o"),
        QString::fromUtf8(u8"│       ├── qterm_quick_demo_autogen"),
        QString::fromUtf8(u8"│       │   └── mocs_compilation.cpp.o"),
        QString::fromUtf8(u8"│       └── qterm_quick_demo_qmltyperegistrations.cpp.o"),
        QString::fromUtf8(u8"├── CTestTestfile.cmake"),
        QString::fromUtf8(u8"├── meta_types"),
        QString::fromUtf8(u8"│   ├── qt6qterm_quick_demo_debug_metatypes.json"),
        QString::fromUtf8(u8"│   ├── qt6qterm_quick_demo_debug_metatypes.json.gen"),
        QString::fromUtf8(u8"│   ├── qterm_quick_demo_json_file_list.txt"),
        QString::fromUtf8(u8"│   └── qterm_quick_demo_json_file_list.txt.timestamp"),
        QString::fromUtf8(u8"├── qmltypes"),
        QString::fromUtf8(u8"│   └── qterm_quick_demo_foreign_types.txt"),
        QString::fromUtf8(u8"├── qterm_quick_demo"),
        QString::fromUtf8(u8"├── qterm_quick_demo_autogen"),
        QString::fromUtf8(u8"│   ├── deps"),
        QString::fromUtf8(u8"│   ├── include"),
        QString::fromUtf8(u8"│   │   ├── main.moc"),
        QString::fromUtf8(u8"│   │   ├── main.moc.d"),
        QString::fromUtf8(u8"│   │   └── main.moc.json"),
        QString::fromUtf8(u8"│   ├── moc_predefs.h"),
        QString::fromUtf8(u8"│   ├── mocs_compilation.cpp"),
        QString::fromUtf8(u8"│   └── timestamp"),
        QString::fromUtf8(u8"├── qterm_quick_demo_qmltyperegistrations.cpp"),
        QString::fromUtf8(u8"└── QTermDemo"),
        QString::fromUtf8(u8"    ├── DebugStatusBar.qml"),
        QString::fromUtf8(u8"    ├── Main.qml"),
        QString::fromUtf8(u8"    ├── qmldir"),
        QString::fromUtf8(u8"    ├── qterm_quick_demo_qml_module_dir_map.qrc"),
        QString::fromUtf8(u8"    ├── qterm_quick_demo.qmltypes"),
        QString::fromUtf8(u8"    └── StatusBar.qml"),
        QString(),
        QString::fromUtf8(u8"10 directories, 28 files"),
        QString::fromUtf8(u8"➜  tangjc@MBP /Users/tangjc/1-proj/2-mygithub/qterm/build/examples/quick-demo")
    };
    const QString inputTranscript = lines.join(QStringLiteral("\r\n"));
    const QString projectedTranscript = lines.join(QStringLiteral("\n"));

    core.setTerminalSize(92, 14);
    core.writePlainText(inputTranscript);

    core.setTerminalSize(58, 14);
    QCOMPARE(core.debugPlainText(), projectedTranscript);

    core.setTerminalSize(44, 14);
    QCOMPARE(core.debugPlainText(), projectedTranscript);

    core.setTerminalSize(73, 14);
    QCOMPARE(core.debugPlainText(), projectedTranscript);

    core.setTerminalSize(39, 14);
    QCOMPARE(core.debugPlainText(), projectedTranscript);

    core.setTerminalSize(88, 14);
    QCOMPARE(core.debugPlainText(), projectedTranscript);
}

void QTermCoreTest::preservesMultiplePromptLinesAcrossExtremeResizeCycles()
{
    // Reproduces the resize-then-shell-redraw corruption:
    // 1. Write N prompt lines (simulating N Enter presses).
    // 2. Resize narrower → buffer reflows, wrappedToNextLine flags set.
    // 3. Shell receives SIGWINCH and redraws the current prompt using ESC[K
    //    (erase to end of line) at the cursor position. This ESC[K clears
    //    wrappedToNextLine on the cursor's physical row, which was set during
    //    reflow. Subsequent resize wider loses content that was "below" that
    //    broken wrap chain.
    QTermCore core;
    core.setTerminalSize(40, 8);

    // Write 4 prompt lines followed by CRLF, simulating 4 Enter presses.
    const QString prompt = u"user@host:~/projects/qterm $"_s;  // 28 chars, fits in 40 cols
    QString transcript;
    for (int i = 0; i < 4; ++i) {
        transcript += prompt + u"\r\n"_s;
    }
    // 5th prompt (current, cursor at end, no trailing newline)
    transcript += prompt;
    core.writePlainText(transcript);

    // Verify initial state: 5 prompt lines, cursor at end of last.
    const QString expected = (QStringList(5, prompt)).join(u'\n');
    QCOMPARE(core.debugPlainText(), expected);
    QCOMPARE(core.cursorState().row, 4);
    QCOMPARE(core.cursorState().column, prompt.size());

    // --- Simulate narrow resize: window dragged to 10 columns ---
    core.setTerminalSize(10, 8);
    // Content reflows: each 28-char prompt wraps into 3 physical rows of 10.
    // Total 5*3=15 physical rows; with rows=8, 7 pushed to history.
    // wrappedToNextLine flags are set correctly by reflow.
    QCOMPARE(core.debugPlainText(), expected);

    // --- Shell receives SIGWINCH and redraws prompt at cursor position ---
    // Real zsh does: move cursor to start of prompt line, ESC[K, reprint prompt.
    // We simulate the ESC[K (erase to end of line) at current cursor position,
    // followed by rewriting the prompt text. This is what breaks wrap flags.
    core.writePlainText(u"\r\x1b[K"_s + prompt);  // CR + EL + prompt reprint

    // Content must still be correct at this point (just prompt redrawn).
    QCOMPARE(core.debugPlainText(), expected);

    // --- Widen back: window dragged back to 40 columns ---
    core.setTerminalSize(40, 8);

    // All 5 prompt lines must survive.
    QCOMPARE(core.debugPlainText(), expected);
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

void QTermCoreTest::preservesContentAcrossRepeatedNarrowWithRedraw()
{
    // Simulates: user drags window narrower in stages, shell redraws at each stage.
    // Tests that multiple narrow+redraw cycles don't corrupt the scrollback.
    QTermCore core;
    core.setTerminalSize(40, 8);

    const QString prompt = u"user@host:~/projects/qterm $"_s;  // 28 chars
    QString transcript;
    for (int i = 0; i < 4; ++i) {
        transcript += prompt + u"\r\n"_s;
    }
    transcript += prompt;
    core.writePlainText(transcript);

    const QString expected = QStringList(5, prompt).join(u'\n');
    QCOMPARE(core.debugPlainText(), expected);

    // Cycle 1: narrow to 20, shell redraws
    core.setTerminalSize(20, 8);
    QCOMPARE(core.debugPlainText(), expected);
    core.writePlainText(u"\r\x1b[K"_s + prompt);
    QCOMPARE(core.debugPlainText(), expected);

    // Cycle 2: narrow to 15, shell redraws
    core.setTerminalSize(15, 8);
    QCOMPARE(core.debugPlainText(), expected);
    core.writePlainText(u"\r\x1b[K"_s + prompt);
    QCOMPARE(core.debugPlainText(), expected);

    // Cycle 3: narrow to 10, shell redraws
    core.setTerminalSize(10, 8);
    QCOMPARE(core.debugPlainText(), expected);
    core.writePlainText(u"\r\x1b[K"_s + prompt);
    QCOMPARE(core.debugPlainText(), expected);

    // Widen back all the way
    core.setTerminalSize(40, 8);
    QCOMPARE(core.debugPlainText(), expected);
}

void QTermCoreTest::preservesContentAcrossOscillatingResize()
{
    // Simulates: user drags window back and forth (narrow→wide→narrow→wide)
    // with shell redraws at each stable size.
    QTermCore core;
    core.setTerminalSize(40, 8);

    const QString prompt = u"user@host:~/projects/qterm $"_s;  // 28 chars
    QString transcript;
    for (int i = 0; i < 4; ++i) {
        transcript += prompt + u"\r\n"_s;
    }
    transcript += prompt;
    core.writePlainText(transcript);

    const QString expected = QStringList(5, prompt).join(u'\n');
    QCOMPARE(core.debugPlainText(), expected);

    // Oscillate multiple times: narrow with redraw, wide with redraw
    for (int cycle = 0; cycle < 3; ++cycle) {
        core.setTerminalSize(20, 8);
        core.writePlainText(u"\r\x1b[K"_s + prompt);
        QCOMPARE(core.debugPlainText(), expected);

        core.setTerminalSize(40, 8);
        // Shell redraws after widening: prompt fits in 1 row, no predecessor chain
        core.writePlainText(u"\r\x1b[K"_s + prompt);
        QCOMPARE(core.debugPlainText(), expected);
    }
}

void QTermCoreTest::preservesContentWhenResizeInterleavesMidRedraw()
{
    // Reproduces the race between rapid user dragging and asynchronous shell redraws.
    //
    // Sequence:
    //   1. Write 5 prompts at wide width.
    //   2. User drags narrow (resize A). Shell gets SIGWINCH and starts sending
    //      \r (carriageReturn). This positions the cursor at the LAST physical row
    //      of the wrapped prompt at width A.
    //   3. BEFORE the rest of the shell's output (\x1b[K + prompt) arrives, the
    //      user continues dragging to width B. The buffer is reflowed again.
    //      After this reflow the cursor may end up at a NON-LAST physical row
    //      (because cursorDisplayOffset / B doesn't land on the last row).
    //   4. \x1b[K arrives. clearToEnd() sets wrappedToNextLine=false on the
    //      now-middle row, severing the prompt's wrap chain in the buffer.
    //   5. Shell finishes writing the new prompt.
    //   6. User drags back to the original wide width.
    //   7. All 5 prompts must still be present.
    QTermCore core;
    core.setTerminalSize(40, 8);

    const QString prompt = u"user@host:~/projects/qterm $"_s;  // 28 chars
    QString transcript;
    for (int i = 0; i < 4; ++i) {
        transcript += prompt + u"\r\n"_s;
    }
    transcript += prompt;
    core.writePlainText(transcript);

    const QString expected = QStringList(5, prompt).join(u'\n');
    QCOMPARE(core.debugPlainText(), expected);

    // Step 2: user drags to 10 cols. Each 28-char prompt -> 3 rows.
    // Cursor ends up at last physical row (row index 2 of 3), col 8.
    core.setTerminalSize(10, 8);
    QCOMPARE(core.debugPlainText(), expected);

    // Step 3: shell sends \r (carriageReturn at 10 cols).
    // Cursor moves to col 0 of that last physical row.
    core.writePlainText(u"\r"_s);
    // cursorDisplayOffset for the 28-char logical line = 2*10 = 20

    // Step 4: user drags further to 7 cols before \x1b[K arrives.
    // At 7 cols, 28 chars -> 4 rows. logicalCursorRow = 20/7 = 2 (NOT the last row, 3).
    // So the cursor lands at row 2 of 4, column 6.
    core.setTerminalSize(7, 8);
    QCOMPARE(core.debugPlainText(), expected);

    // Step 5: \x1b[K + prompt arrive at 7 cols.
    // \x1b[K clears from cursor col (6) to end of row 2 AND sets wrappedToNextLine=false,
    // severing row 2 from row 3. Without a fix this leaves row 3 as a spurious
    // logical line and the rewrite only covers rows up to cursor+1.
    core.writePlainText(u"\x1b[K"_s + prompt);
    QCOMPARE(core.debugPlainText(), expected);

    // Step 6: user drags back to 40 cols.
    core.setTerminalSize(40, 8);
    QCOMPARE(core.debugPlainText(), expected);
}

void QTermCoreTest::clearsState(){
    QTermCore core;
    core.writePlainText("bootstrap"_L1);

    core.clear();

    QCOMPARE(core.debugPlainText(), QString());
    QCOMPARE(core.cursorState().row, 0);
    QCOMPARE(core.cursorState().column, 0);
}

void QTermCoreTest::togglesMouseModeX10()
{
    QTermCore core;
    QCOMPARE(core.modeState().mouseMode, MouseMode::Disabled);

    // 启用 X10 鼠标模式：ESC[?1000h
    core.writePlainText("\x1b[?1000h"_L1);
    QCOMPARE(core.modeState().mouseMode, MouseMode::X10);

    // 禁用鼠标模式：ESC[?1000l
    core.writePlainText("\x1b[?1000l"_L1);
    QCOMPARE(core.modeState().mouseMode, MouseMode::Disabled);
}

void QTermCoreTest::togglesMouseModeSGR()
{
    QTermCore core;
    QCOMPARE(core.modeState().mouseMode, MouseMode::Disabled);

    // 启用 SGR 鼠标模式：ESC[?1006h
    core.writePlainText("\x1b[?1006h"_L1);
    QCOMPARE(core.modeState().mouseMode, MouseMode::SGR);

    // 禁用鼠标模式：ESC[?1006l
    core.writePlainText("\x1b[?1006l"_L1);
    QCOMPARE(core.modeState().mouseMode, MouseMode::Disabled);
}

void QTermCoreTest::encodesMouseEventX10()
{
    QTermCore core;
    core.writePlainText("\x1b[?1000h"_L1);  // 启用 X10 模式

    // 编码鼠标按下事件：左键按下在 (5, 10)
    const QByteArray encoded = QTermInputEncoder::encodeMouse(
        10, 5, Qt::LeftButton, Qt::NoModifier, true, core.modeState());
    
    // X10 格式：ESC[M<button><x><y>
    // button = 0 + 0x20 = 0x20 (' ')
    // x = 5 + 33 = 38 = 0x26 ('&')
    // y = 10 + 33 = 43 = 0x2B ('+')
    const QByteArray expected = QByteArray("\x1b[M ") + QByteArray(reinterpret_cast<const char *>("\x26"), 1) +
                                QByteArray(reinterpret_cast<const char *>("\x2B"), 1);
    QCOMPARE(encoded, expected);
}

void QTermCoreTest::encodesMouseEventSGR()
{
    QTermCore core;
    core.writePlainText("\x1b[?1006h"_L1);  // 启用 SGR 模式

    // 编码鼠标按下事件：右键按下在 (5, 10)，带 Shift 修饰符
    const QByteArray encoded = QTermInputEncoder::encodeMouse(
        10, 5, Qt::RightButton, Qt::ShiftModifier, true, core.modeState());
    
    // SGR 格式：ESC[<button>;<x>;<y>M
    // button = 2 (右键) + 4 (Shift) = 6
    // x = 5 + 1 = 6
    // y = 10 + 1 = 11
    const QByteArray expected = QByteArray("\x1b[6;6;11M");
    QCOMPARE(encoded, expected);
}

} // namespace QTerm

QTEST_MAIN(QTerm::QTermCoreTest)

#include "QTermCoreTest.moc"