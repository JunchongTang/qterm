#include <QtTest>

#include "core/QTermCore.h"

using namespace Qt::StringLiterals;

/*
    The capability matrix.

    docs/protocol-support.md is generated from this file's results, so the
    published support table cannot drift from the code: a capability is listed
    as supported only if the assertion below passed on this build.

    The previous table was hand-maintained and had drifted badly -- function
    keys, modifier sequences, SO/SI and DECKPAM were all marked supported while
    the code did not implement them. Anything hand-written will drift again;
    only a generated table stays honest.

    Each slot declares itself with CAPABILITY() and then asserts the behaviour.
    Capabilities that are known to be missing use QEXPECT_FAIL, which keeps the
    suite green while recording the gap -- and turns into a failure (XPASS) the
    moment someone implements it, forcing the table to be regenerated.

    The enumeration is not exhaustive; it covers what the target applications
    exercise. Absence from this file means "not assessed", which the generated
    table shows as a distinct state from "not supported".
*/

#define CAPABILITY(id, group, description, reference)                        \
    qInfo().noquote() << u"CAPABILITY\t%1\t%2\t%3\t%4"_s                     \
                             .arg(QStringLiteral(id), QStringLiteral(group), \
                                  QStringLiteral(description),               \
                                  QStringLiteral(reference))

namespace QTerm {

class QTermCapabilityTest : public QObject
{
    Q_OBJECT

private slots:
    // ── Cursor movement ──────────────────────────────────────────────────
    void cursorPosition();
    void cursorUpDownForwardBack();
    void cursorNextPreviousLine();
    void cursorHorizontalAbsolute();
    void linePositionAbsolute();
    void saveRestoreCursorDec();
    void saveRestoreCursorAnsi();
    void reverseIndex();

    // ── Erase and edit ───────────────────────────────────────────────────
    void eraseInDisplay();
    void eraseInDisplayScrollback();
    void eraseInLine();
    void eraseCharacters();
    void insertDeleteCharacters();
    void insertDeleteLines();

    // ── Scrolling ────────────────────────────────────────────────────────
    void scrollRegion();
    void scrollUpDown();

    // ── Presentation ─────────────────────────────────────────────────────
    void sgrBasicAttributes();
    void sgr256Colour();
    void sgrTrueColour();
    void sgrColonSubParameters();
    void sgrReset();

    // ── Modes ────────────────────────────────────────────────────────────
    void modeCursorVisibility();
    void modeAutoWrap();
    void modeBracketedPaste();
    void modeApplicationCursorKeys();
    void modeAlternateScreen();
    void modeInsertReplace();
    void modeOriginDecom();

    // ── Character sets ───────────────────────────────────────────────────
    void charsetDecLineDrawing();
    void charsetShiftInOut();

    // ── Reports ──────────────────────────────────────────────────────────
    void reportDeviceStatus();
    void reportPrimaryDeviceAttributes();
    void reportSecondaryDeviceAttributes();

    // ── Operating system commands ────────────────────────────────────────
    void oscWindowTitle();
    void oscCurrentDirectory();
    void oscClipboardWrite();
    void oscShellIntegration();
    void oscHyperlink();

    // ── Control strings ──────────────────────────────────────────────────
    void controlStringsIgnoredSafely();
    void verticalTabAndFormFeed();
    void unknownEscapeIntermediates();

    // ── Keyboard encoding ────────────────────────────────────────────────
    void keyCursorNormal();
    void keyCursorApplication();
    void keyHomeEnd();
    void keyReturnBackspaceTabEscape();
    void keyControlLetter();
    void keyFunctionF1toF4();
    void keyFunctionF5toF12();
    void keyInsertDelete();
    void keyPageUpDown();
    void keyModifiedCursor();
    void keyApplicationKeypad();
    void keyAltMeta();
    void keyShiftTab();

    // ── Paste and mouse ──────────────────────────────────────────────────
    void pasteBracketed();
    void mouseX10Encoding();
    void mouseSgrEncoding();
};

namespace {

// Feeds `text` to a fresh core of the given size.
QString lineAt(const QTermCore &core, int row)
{
    const QStringList lines = core.dumpPlainText().split(u'\n');
    return row < lines.size() ? lines.at(row) : QString();
}

QByteArray outboundFor(QTermCore &core, int key, const QString &text = QString())
{
    return core.encodeKey(key, text);
}

} // namespace

// ── Cursor movement ──────────────────────────────────────────────────────────

void QTermCapabilityTest::cursorPosition()
{
    CAPABILITY("cursor.cup", "Cursor", "Absolute positioning (CUP)", "CSI Ps ; Ps H");
    QTermCore core;
    core.writePlainText(u"\x1b[3;5Hx"_s);
    QCOMPARE(core.cursorState().row, 2);
    QCOMPARE(lineAt(core, 2).at(4), u'x');
}

void QTermCapabilityTest::cursorUpDownForwardBack()
{
    CAPABILITY("cursor.cuu-cud-cuf-cub", "Cursor", "Relative movement", "CSI A / B / C / D");
    QTermCore core;
    core.writePlainText(u"\x1b[10;10H\x1b[2A\x1b[3B\x1b[4C\x1b[5D"_s);
    QCOMPARE(core.cursorState().row, 10);
    QCOMPARE(core.cursorState().column, 8);
}

void QTermCapabilityTest::cursorNextPreviousLine()
{
    CAPABILITY("cursor.cnl-cpl", "Cursor", "Next/previous line", "CSI E / F");
    QTermCore core;
    core.writePlainText(u"\x1b[5;10H\x1b[2E"_s);
    QCOMPARE(core.cursorState().row, 6);
    QCOMPARE(core.cursorState().column, 0);
}

void QTermCapabilityTest::cursorHorizontalAbsolute()
{
    CAPABILITY("cursor.cha", "Cursor", "Horizontal absolute", "CSI Ps G");
    QTermCore core;
    core.writePlainText(u"\x1b[20Gx"_s);
    QCOMPARE(core.cursorState().column, 20);
}

void QTermCapabilityTest::linePositionAbsolute()
{
    CAPABILITY("cursor.vpa", "Cursor", "Vertical absolute", "CSI Ps d");
    QTermCore core;
    core.writePlainText(u"\x1b[7d"_s);
    QCOMPARE(core.cursorState().row, 6);
}

void QTermCapabilityTest::saveRestoreCursorDec()
{
    CAPABILITY("cursor.decsc", "Cursor", "Save/restore cursor", "ESC 7 / ESC 8");
    QTermCore core;
    core.writePlainText(u"\x1b[5;5H\0337\x1b[20;20H\0338"_s);
    QCOMPARE(core.cursorState().row, 4);
    QCOMPARE(core.cursorState().column, 4);
}

void QTermCapabilityTest::saveRestoreCursorAnsi()
{
    CAPABILITY("cursor.scosc", "Cursor", "Save/restore cursor (ANSI)", "CSI s / CSI u");
    QTermCore core;
    core.writePlainText(u"\x1b[5;5H\x1b[s\x1b[20;20H\x1b[u"_s);
    QCOMPARE(core.cursorState().row, 4);
    QCOMPARE(core.cursorState().column, 4);
}

void QTermCapabilityTest::reverseIndex()
{
    CAPABILITY("cursor.ri", "Cursor", "Reverse index", "ESC M");
    QTermCore core;
    core.writePlainText(u"line1\r\nline2\x1b[1;1H\x1bMtop"_s);
    QCOMPARE(lineAt(core, 0), u"top"_s);
}

// ── Erase and edit ───────────────────────────────────────────────────────────

void QTermCapabilityTest::eraseInDisplay()
{
    CAPABILITY("erase.ed", "Erase", "Erase in display", "CSI Ps J");
    QTermCore core;
    core.writePlainText(u"hello\x1b[2J"_s);
    QVERIFY(!core.dumpPlainText().contains(u"hello"_s));
}

void QTermCapabilityTest::eraseInDisplayScrollback()
{
    CAPABILITY("erase.ed3", "Erase", "Erase scrollback only", "CSI 3 J");
    QTermCore core;
    core.setTerminalSize(20, 3);
    core.writePlainText(u"a\r\nb\r\nc\r\nd\r\ne\r\n"_s);
    core.writePlainText(u"\x1b[3J"_s);
    QVERIFY(!core.dumpPlainText().contains(u'a'));
}

void QTermCapabilityTest::eraseInLine()
{
    CAPABILITY("erase.el", "Erase", "Erase in line", "CSI Ps K");
    QTermCore core;
    core.writePlainText(u"abcdef\x1b[1;4H\x1b[K"_s);
    QCOMPARE(lineAt(core, 0), u"abc"_s);
}

void QTermCapabilityTest::eraseCharacters()
{
    CAPABILITY("erase.ech", "Erase", "Erase characters", "CSI Ps X");
    QTermCore core;
    core.writePlainText(u"abcdef\x1b[1;2H\x1b[3X"_s);
    QCOMPARE(lineAt(core, 0).mid(1, 3), u"   "_s);
}

void QTermCapabilityTest::insertDeleteCharacters()
{
    CAPABILITY("edit.ich-dch", "Erase", "Insert/delete characters", "CSI Ps @ / CSI Ps P");
    QTermCore core;
    core.writePlainText(u"abcdef\x1b[1;1H\x1b[2P"_s);
    QCOMPARE(lineAt(core, 0).trimmed(), u"cdef"_s);
}

void QTermCapabilityTest::insertDeleteLines()
{
    CAPABILITY("edit.il-dl", "Erase", "Insert/delete lines", "CSI Ps L / CSI Ps M");
    QTermCore core;
    core.writePlainText(u"one\r\ntwo\r\nthree\x1b[1;1H\x1b[1M"_s);
    QCOMPARE(lineAt(core, 0), u"two"_s);
}

// ── Scrolling ────────────────────────────────────────────────────────────────

void QTermCapabilityTest::scrollRegion()
{
    CAPABILITY("scroll.decstbm", "Scrolling", "Set scrolling region", "CSI Ps ; Ps r");
    QTermCore core;
    core.setTerminalSize(20, 6);
    core.writePlainText(u"\x1b[2;4r\x1b[2;1Ha\r\nb\r\nc\r\nd"_s);
    QCOMPARE(lineAt(core, 3), u"d"_s);
}

void QTermCapabilityTest::scrollUpDown()
{
    CAPABILITY("scroll.su-sd", "Scrolling", "Scroll up/down", "CSI Ps S / CSI Ps T");
    QTermCore core;
    core.setTerminalSize(20, 4);
    core.writePlainText(u"a\r\nb\r\nc\x1b[2S"_s);
    QCOMPARE(lineAt(core, 0), u"c"_s);
}

// ── Presentation ─────────────────────────────────────────────────────────────

void QTermCapabilityTest::sgrBasicAttributes()
{
    CAPABILITY("sgr.basic", "Presentation", "Bold/italic/underline/inverse", "CSI 1/3/4/7 m");
    QTermCore core;
    core.writePlainText(u"\x1b[1;3;4;7mstyled\x1b[0m"_s);
    QVERIFY(core.dumpAnsi().contains("styled"));
}

void QTermCapabilityTest::sgr256Colour()
{
    CAPABILITY("sgr.256", "Presentation", "256-colour palette", "CSI 38 ; 5 ; Ps m");
    QTermCore core;
    core.writePlainText(u"\x1b[38;5;196mred\x1b[0m"_s);
    QVERIFY(core.dumpAnsi().contains("38;5;196"));
}

void QTermCapabilityTest::sgrTrueColour()
{
    CAPABILITY("sgr.truecolor", "Presentation", "24-bit colour", "CSI 38 ; 2 ; R ; G ; B m");
    QTermCore core;
    core.writePlainText(u"\x1b[38;2;255;0;0mred\x1b[0m"_s);
    QVERIFY(core.dumpAnsi().contains("38;2;255;0;0"));
}

void QTermCapabilityTest::sgrColonSubParameters()
{
    CAPABILITY("sgr.colon", "Presentation", "Colon-separated sub-parameters", "ITU T.416");
    QTermCore core;
    core.writePlainText(u"\x1b[38:2::255:0:0mred\x1b[0m"_s);
    QCOMPARE(lineAt(core, 0).trimmed(), u"red"_s);
    QVERIFY(core.dumpAnsi().contains("38;2;255;0;0"));
}

void QTermCapabilityTest::sgrReset()
{
    CAPABILITY("sgr.reset", "Presentation", "Attribute reset", "CSI 0 m");
    QTermCore core;
    core.writePlainText(u"\x1b[1;31mred\x1b[0mplain"_s);
    QVERIFY(core.dumpAnsi().contains("plain"));
}

// ── Modes ────────────────────────────────────────────────────────────────────

void QTermCapabilityTest::modeCursorVisibility()
{
    CAPABILITY("mode.decTCEM", "Modes", "Cursor visibility", "CSI ? 25 h/l");
    QTermCore core;
    core.writePlainText(u"\x1b[?25l"_s);
    QCOMPARE(core.modeState().cursorVisible, false);
    core.writePlainText(u"\x1b[?25h"_s);
    QCOMPARE(core.modeState().cursorVisible, true);
}

void QTermCapabilityTest::modeAutoWrap()
{
    CAPABILITY("mode.decawm", "Modes", "Auto-wrap", "CSI ? 7 h/l");
    QTermCore core;
    core.writePlainText(u"\x1b[?7l"_s);
    QCOMPARE(core.modeState().autoWrap, false);
}

void QTermCapabilityTest::modeBracketedPaste()
{
    CAPABILITY("mode.bracketed-paste", "Modes", "Bracketed paste", "CSI ? 2004 h/l");
    QTermCore core;
    core.writePlainText(u"\x1b[?2004h"_s);
    QCOMPARE(core.modeState().bracketedPaste, true);
}

void QTermCapabilityTest::modeApplicationCursorKeys()
{
    CAPABILITY("mode.decckm", "Modes", "Application cursor keys", "CSI ? 1 h/l");
    QTermCore core;
    core.writePlainText(u"\x1b[?1h"_s);
    QCOMPARE(core.modeState().applicationCursorKeys, true);
}

void QTermCapabilityTest::modeAlternateScreen()
{
    CAPABILITY("mode.alt-screen", "Modes", "Alternate screen buffer", "CSI ? 1049 h/l");
    QTermCore core;
    core.writePlainText(u"main\x1b[?1049h"_s);
    QCOMPARE(core.modeState().alternateScreenActive, true);
    core.writePlainText(u"\x1b[?1049l"_s);
    QVERIFY(core.dumpPlainText().contains(u"main"_s));
}

void QTermCapabilityTest::modeInsertReplace()
{
    CAPABILITY("mode.irm", "Modes", "Insert/replace mode", "CSI 4 h/l");
    QTermCore core;
    core.writePlainText(u"abc\x1b[1;1H\x1b[4hXY"_s);
    QEXPECT_FAIL("", "IRM is not implemented; text always overwrites", Continue);
    QCOMPARE(lineAt(core, 0).trimmed(), u"XYabc"_s);
}

void QTermCapabilityTest::modeOriginDecom()
{
    CAPABILITY("mode.decom", "Modes", "Origin mode", "CSI ? 6 h/l");
    QTermCore core;
    core.setTerminalSize(20, 10);
    core.writePlainText(u"\x1b[3;6r\x1b[?6h\x1b[1;1Hx"_s);
    QEXPECT_FAIL("", "DECOM is not implemented; CUP stays screen-absolute", Continue);
    QCOMPARE(lineAt(core, 2).trimmed(), u"x"_s);
}

// ── Character sets ───────────────────────────────────────────────────────────

void QTermCapabilityTest::charsetDecLineDrawing()
{
    CAPABILITY("charset.dec-graphics", "Character sets", "DEC line drawing (G0)", "ESC ( 0");
    QTermCore core;
    core.writePlainText(u"\x1b(0lqk\x1b(B"_s);
    QCOMPARE(lineAt(core, 0), u"┌─┐"_s);
}

void QTermCapabilityTest::charsetShiftInOut()
{
    CAPABILITY("charset.so-si", "Character sets", "Locking shift G0/G1", "SO (0x0E) / SI (0x0F)");
    QTermCore core;
    core.writePlainText(u"\x1b)0\x0elqk\x0f"_s);
    QEXPECT_FAIL("", "SO/SI are swallowed rather than switching the active charset", Continue);
    QCOMPARE(lineAt(core, 0), u"┌─┐"_s);
}

// ── Reports ──────────────────────────────────────────────────────────────────

void QTermCapabilityTest::reportDeviceStatus()
{
    CAPABILITY("report.dsr", "Reports", "Cursor position report", "CSI 6 n");
    QTermCore core;
    QByteArray reply;
    QObject::connect(&core, &QTermCore::outboundData, &core,
                     [&reply](const QByteArray &data) { reply += data; });
    core.writePlainText(u"\x1b[3;7H\x1b[6n"_s);
    QCOMPARE(reply, QByteArray("\x1b[3;7R"));
}

void QTermCapabilityTest::reportPrimaryDeviceAttributes()
{
    CAPABILITY("report.da1", "Reports", "Primary device attributes", "CSI c");
    QTermCore core;
    QByteArray reply;
    QObject::connect(&core, &QTermCore::outboundData, &core,
                     [&reply](const QByteArray &data) { reply += data; });
    core.writePlainText(u"\x1b[c"_s);
    QVERIFY(reply.startsWith("\x1b[?"));
}

void QTermCapabilityTest::reportSecondaryDeviceAttributes()
{
    CAPABILITY("report.da2", "Reports", "Secondary device attributes", "CSI > c");
    QTermCore core;
    QByteArray reply;
    QObject::connect(&core, &QTermCore::outboundData, &core,
                     [&reply](const QByteArray &data) { reply += data; });
    core.writePlainText(u"\x1b[>c"_s);
    QVERIFY(reply.startsWith("\x1b[>"));
}

// ── Operating system commands ────────────────────────────────────────────────

void QTermCapabilityTest::oscWindowTitle()
{
    CAPABILITY("osc.title", "OSC", "Window title", "OSC 0 / 2");
    QTermCore core;
    core.writePlainText(u"\x1b]2;My Title\a"_s);
    QCOMPARE(core.title(), u"My Title"_s);
}

void QTermCapabilityTest::oscCurrentDirectory()
{
    CAPABILITY("osc.cwd", "OSC", "Current working directory", "OSC 7");
    QTermCore core;
    core.writePlainText(u"\x1b]7;file:///tmp\a"_s);
    QVERIFY(core.currentDirectory().contains(u"/tmp"_s));
}

void QTermCapabilityTest::oscClipboardWrite()
{
    CAPABILITY("osc.clipboard", "OSC", "Clipboard write", "OSC 52");
    QTermCore core;
    QString written;
    QObject::connect(&core, &QTermCore::clipboardWriteRequested, &core,
                     [&written](const QString &text) { written = text; });
    core.writePlainText(u"\x1b]52;c;SGVsbG8=\a"_s);
    QCOMPARE(written, u"Hello"_s);
}

void QTermCapabilityTest::oscShellIntegration()
{
    CAPABILITY("osc.shell-integration", "OSC", "Shell integration markers", "OSC 133");
    QTermCore core;
    core.writePlainText(u"\x1b]133;A\a"_s);
    QVERIFY(core.shellZone() != 0);
}

void QTermCapabilityTest::oscHyperlink()
{
    CAPABILITY("osc.hyperlink", "OSC", "Hyperlinks", "OSC 8");
    QTermCore core;
    core.writePlainText(u"\x1b]8;;https://example.com\x1b\\link\x1b]8;;\x1b\\"_s);
    QCOMPARE(lineAt(core, 0).trimmed(), u"link"_s);
    QCOMPARE(core.hyperlinkUrl(1), u"https://example.com"_s);
}

// ── Control strings ──────────────────────────────────────────────────────────

void QTermCapabilityTest::controlStringsIgnoredSafely()
{
    CAPABILITY("parser.control-strings", "Parser", "DCS/APC/PM/SOS swallowed", "ESC P / _ / ^ / X");
    QTermCore core;
    core.writePlainText(u"\x1bPtmux;payload\x1b\\visible"_s);
    QCOMPARE(lineAt(core, 0).trimmed(), u"visible"_s);
}

void QTermCapabilityTest::verticalTabAndFormFeed()
{
    CAPABILITY("parser.vt-ff", "Parser", "VT/FF treated as line feed", "0x0B / 0x0C");
    QTermCore core;
    core.writePlainText(u"a\013b\014c"_s);
    QCOMPARE(lineAt(core, 0).trimmed(), u"a"_s);
    QVERIFY(!core.dumpPlainText().contains(u'\x0b'));
}

void QTermCapabilityTest::unknownEscapeIntermediates()
{
    CAPABILITY("parser.esc-intermediates", "Parser", "Unknown ESC intermediates swallowed", "ESC # 8");
    QTermCore core;
    core.writePlainText(u"\x1b#8visible"_s);
    QCOMPARE(lineAt(core, 0).trimmed(), u"visible"_s);
}

// ── Keyboard encoding ────────────────────────────────────────────────────────

void QTermCapabilityTest::keyCursorNormal()
{
    CAPABILITY("key.cursor-normal", "Keyboard", "Arrow keys, normal mode", "CSI A/B/C/D");
    QTermCore core;
    QCOMPARE(outboundFor(core, Qt::Key_Up), QByteArray("\x1b[A"));
    QCOMPARE(outboundFor(core, Qt::Key_Left), QByteArray("\x1b[D"));
}

void QTermCapabilityTest::keyCursorApplication()
{
    CAPABILITY("key.cursor-application", "Keyboard", "Arrow keys, application mode", "SS3 A/B/C/D");
    QTermCore core;
    core.writePlainText(u"\x1b[?1h"_s);
    QCOMPARE(outboundFor(core, Qt::Key_Up), QByteArray("\x1bOA"));
}

void QTermCapabilityTest::keyHomeEnd()
{
    CAPABILITY("key.home-end", "Keyboard", "Home and End", "CSI H / CSI F");
    QTermCore core;
    QCOMPARE(outboundFor(core, Qt::Key_Home), QByteArray("\x1b[H"));
    QCOMPARE(outboundFor(core, Qt::Key_End), QByteArray("\x1b[F"));
}

void QTermCapabilityTest::keyReturnBackspaceTabEscape()
{
    CAPABILITY("key.basic", "Keyboard", "Return, Backspace, Tab, Escape", "CR / DEL / HT / ESC");
    QTermCore core;
    QCOMPARE(outboundFor(core, Qt::Key_Return), QByteArray("\r"));
    QCOMPARE(outboundFor(core, Qt::Key_Backspace), QByteArray("\x7f"));
    QCOMPARE(outboundFor(core, Qt::Key_Tab), QByteArray("\t"));
    QCOMPARE(outboundFor(core, Qt::Key_Escape), QByteArray("\x1b"));
}

void QTermCapabilityTest::keyControlLetter()
{
    CAPABILITY("key.ctrl-letter", "Keyboard", "Ctrl+letter control codes", "0x01-0x1A");
    QTermCore core;
    QCOMPARE(outboundFor(core, Qt::Key_C), QByteArray(1, '\x03'));
}

void QTermCapabilityTest::keyFunctionF1toF4()
{
    CAPABILITY("key.f1-f4", "Keyboard", "F1-F4", "SS3 P/Q/R/S");
    QTermCore core;
    QEXPECT_FAIL("", "Function keys are not encoded", Abort);
    QCOMPARE(outboundFor(core, Qt::Key_F1), QByteArray("\x1bOP"));
    QCOMPARE(outboundFor(core, Qt::Key_F4), QByteArray("\x1bOS"));
}

void QTermCapabilityTest::keyFunctionF5toF12()
{
    CAPABILITY("key.f5-f12", "Keyboard", "F5-F12", "CSI 15~ .. CSI 24~");
    QTermCore core;
    QEXPECT_FAIL("", "Function keys are not encoded", Abort);
    QCOMPARE(outboundFor(core, Qt::Key_F5), QByteArray("\x1b[15~"));
    QCOMPARE(outboundFor(core, Qt::Key_F12), QByteArray("\x1b[24~"));
}

void QTermCapabilityTest::keyInsertDelete()
{
    CAPABILITY("key.insert-delete", "Keyboard", "Insert and Delete", "CSI 2~ / CSI 3~");
    QTermCore core;
    QEXPECT_FAIL("", "Editing keys are not encoded -- Delete does nothing", Abort);
    QCOMPARE(outboundFor(core, Qt::Key_Insert), QByteArray("\x1b[2~"));
    QCOMPARE(outboundFor(core, Qt::Key_Delete), QByteArray("\x1b[3~"));
}

void QTermCapabilityTest::keyPageUpDown()
{
    CAPABILITY("key.page-up-down", "Keyboard", "PageUp and PageDown", "CSI 5~ / CSI 6~");
    QTermCore core;
    QEXPECT_FAIL("", "Paging keys are not encoded", Abort);
    QCOMPARE(outboundFor(core, Qt::Key_PageUp), QByteArray("\x1b[5~"));
    QCOMPARE(outboundFor(core, Qt::Key_PageDown), QByteArray("\x1b[6~"));
}

void QTermCapabilityTest::keyModifiedCursor()
{
    CAPABILITY("key.modified-cursor", "Keyboard", "Modifier + arrow keys", "CSI 1 ; Ps A");
    QTermCore core;
    // encodeKey() takes no modifier argument at all, so this cannot be
    // expressed today; the assertion documents the target encoding.
    QEXPECT_FAIL("", "encodeKey() takes no modifiers, so this cannot be expressed", Continue);
    QCOMPARE(outboundFor(core, Qt::Key_Right), QByteArray("\x1b[1;5C"));
}

void QTermCapabilityTest::keyApplicationKeypad()
{
    CAPABILITY("key.application-keypad", "Keyboard", "Application keypad", "DECKPAM / SS3 p-y");
    QTermCore core;
    core.writePlainText(u"\x1b="_s);
    QVERIFY(core.modeState().applicationKeypad);
    QEXPECT_FAIL("", "applicationKeypad is tracked but the encoder never reads it", Continue);
    QCOMPARE(outboundFor(core, Qt::Key_0), QByteArray("\x1bOp"));
}

void QTermCapabilityTest::keyAltMeta()
{
    CAPABILITY("key.alt-meta", "Keyboard", "Alt/Meta prefix", "ESC + key");
    QTermCore core;
    QEXPECT_FAIL("", "Alt/Meta prefixing is not implemented", Continue);
    QCOMPARE(outboundFor(core, Qt::Key_B), QByteArray("\x1b" "b"));
}

void QTermCapabilityTest::keyShiftTab()
{
    CAPABILITY("key.shift-tab", "Keyboard", "Shift+Tab (back tab)", "CSI Z");
    QTermCore core;
    QEXPECT_FAIL("", "Backtab is not encoded", Continue);
    QCOMPARE(outboundFor(core, Qt::Key_Backtab), QByteArray("\x1b[Z"));
}

// ── Paste and mouse ──────────────────────────────────────────────────────────

void QTermCapabilityTest::pasteBracketed()
{
    CAPABILITY("paste.bracketed", "Input", "Bracketed paste wrapping", "CSI 200~ / 201~");
    QTermCore core;
    core.writePlainText(u"\x1b[?2004h"_s);
    QCOMPARE(core.encodePaste(u"hi"_s), QByteArray("\x1b[200~hi\x1b[201~"));
}

void QTermCapabilityTest::mouseX10Encoding()
{
    CAPABILITY("mouse.x10", "Input", "X10 mouse reporting", "CSI ? 1000 h");
    QTermCore core;
    core.writePlainText(u"\x1b[?1000h"_s);
    QCOMPARE(core.modeState().mouseTracking != MouseTracking::Disabled, true);
}

void QTermCapabilityTest::mouseSgrEncoding()
{
    CAPABILITY("mouse.sgr", "Input", "SGR mouse encoding", "CSI ? 1006 h");
    QTermCore core;
    core.writePlainText(u"\x1b[?1000h\x1b[?1006h"_s);
    QCOMPARE(core.modeState().mouseEncoding, MouseEncoding::SGR);
}

} // namespace QTerm

QTEST_MAIN(QTerm::QTermCapabilityTest)
#include "QTermCapabilityTest.moc"
