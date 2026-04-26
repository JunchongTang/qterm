#include "QTermInputExecutor.h"

#include <QChar>
#include <QtGlobal>

namespace {

constexpr int kTabWidth = 8;

// DEC Special Character and Line Drawing Set (activated by ESC ( 0, deactivated by ESC ( B).
// Maps ASCII 0x60-0x7e to Unicode equivalents used for box-drawing and symbols.
static QChar applyLineDrawing(QChar ch)
{
    // clang-format off
    static const char16_t kMap[] = {
        u'\u25c6', // 0x60 ` → ◆
        u'\u2592', // 0x61 a → ▒
        u'\u2409', // 0x62 b → ␉
        u'\u240c', // 0x63 c → ␌
        u'\u240d', // 0x64 d → ␍
        u'\u240a', // 0x65 e → ␊
        u'\u00b0', // 0x66 f → °
        u'\u00b1', // 0x67 g → ±
        u'\u2424', // 0x68 h → ␤
        u'\u240b', // 0x69 i → ␋
        u'\u2518', // 0x6a j → ┘
        u'\u2510', // 0x6b k → ┐
        u'\u250c', // 0x6c l → ┌
        u'\u2514', // 0x6d m → └
        u'\u253c', // 0x6e n → ┼
        u'\u23ba', // 0x6f o → ⎺
        u'\u23bb', // 0x70 p → ⎻
        u'\u2500', // 0x71 q → ─
        u'\u23bc', // 0x72 r → ⎼
        u'\u23bd', // 0x73 s → ⎽
        u'\u251c', // 0x74 t → ├
        u'\u2524', // 0x75 u → ┤
        u'\u2534', // 0x76 v → ┴
        u'\u252c', // 0x77 w → ┬
        u'\u2502', // 0x78 x → │
        u'\u2264', // 0x79 y → ≤
        u'\u2265', // 0x7a z → ≥
        u'\u03c0', // 0x7b { → π
        u'\u2260', // 0x7c | → ≠
        u'\u00a3', // 0x7d } → £
        u'\u00b7', // 0x7e ~ → ·
    };
    // clang-format on
    const int index = ch.unicode() - 0x60;
    if (index >= 0 && index < static_cast<int>(std::size(kMap))) {
        return QChar(kMap[index]);
    }
    return ch;
}

int clampColorComponent(int value)
{
    return qBound(0, value, 255);
}

int packRgb(int red, int green, int blue)
{
    return (clampColorComponent(red) << 16) |
           (clampColorComponent(green) << 8) |
           clampColorComponent(blue);
}

bool isCombiningMark(const QString &text)
{
    const QList<uint> codePoints = text.toUcs4();
    if (codePoints.size() != 1) {
        return false;
    }

    switch (QChar::category(codePoints.front())) {
    case QChar::Mark_NonSpacing:
    case QChar::Mark_SpacingCombining:
    case QChar::Mark_Enclosing:
        return true;
    default:
        return false;
    }
}

bool isWideCodePoint(uint codePoint)
{
    return (codePoint >= 0x1100 && codePoint <= 0x115f) ||
           codePoint == 0x2329 ||
           codePoint == 0x232a ||
           (codePoint >= 0x2e80 && codePoint <= 0xa4cf) ||
           (codePoint >= 0xac00 && codePoint <= 0xd7a3) ||
           (codePoint >= 0xf900 && codePoint <= 0xfaff) ||
           (codePoint >= 0xfe10 && codePoint <= 0xfe19) ||
           (codePoint >= 0xfe30 && codePoint <= 0xfe6f) ||
           (codePoint >= 0xff00 && codePoint <= 0xff60) ||
           (codePoint >= 0xffe0 && codePoint <= 0xffe6) ||
           (codePoint >= 0x1f300 && codePoint <= 0x1faff) ||
           (codePoint >= 0x20000 && codePoint <= 0x3fffd);
}

int displayWidth(const QString &text)
{
    const QList<uint> codePoints = text.toUcs4();
    if (codePoints.isEmpty()) {
        return 0;
    }

    if (isCombiningMark(text)) {
        return 0;
    }

    return isWideCodePoint(codePoints.front()) ? 2 : 1;
}

int consumeExtendedColor(const QVector<int> &parameters, int parameterIndex, bool foreground, QTerm::QTermCellAttributes &attributes)
{
    if (parameterIndex + 1 >= parameters.size()) {
        return 1;
    }

    const int colorMode = parameters.at(parameterIndex + 1);
    if (colorMode == 5 && parameterIndex + 2 < parameters.size()) {
        if (foreground) {
            attributes.foregroundIndex = qBound(0, parameters.at(parameterIndex + 2), 255);
            attributes.foregroundRgb = -1;
        } else {
            attributes.backgroundIndex = qBound(0, parameters.at(parameterIndex + 2), 255);
            attributes.backgroundRgb = -1;
        }
        return 3;
    }

    if (colorMode == 2 && parameterIndex + 4 < parameters.size()) {
        const int rgb = packRgb(parameters.at(parameterIndex + 2),
                                parameters.at(parameterIndex + 3),
                                parameters.at(parameterIndex + 4));
        if (foreground) {
            attributes.foregroundIndex = -1;
            attributes.foregroundRgb = rgb;
        } else {
            attributes.backgroundIndex = -1;
            attributes.backgroundRgb = rgb;
        }
        return 5;
    }

    return 1;
}

} // namespace

namespace QTerm {

QTermInputExecutor::QTermInputExecutor(QTermScreenState &primaryScreen,
                                       QTermScreenState &alternateScreen,
                                       QTermModeState &modeState)
    : m_primaryScreen(primaryScreen)
    , m_alternateScreen(alternateScreen)
    , m_modeState(modeState)
{
}

QTermCursorState QTermInputExecutor::cursorState() const noexcept
{
    return currentScreen().cursorState;
}

const QTermModeState &QTermInputExecutor::modeState() const noexcept
{
    return m_modeState;
}

QTermCellAttributes QTermInputExecutor::currentAttributes() const noexcept
{
    return currentScreen().currentAttributes;
}

std::optional<QTermSavedCursorState> QTermInputExecutor::savedCursorState() const
{
    return currentScreen().savedCursorState;
}

int QTermInputExecutor::scrollTop() const noexcept
{
    return currentScreen().scrollTop;
}

int QTermInputExecutor::scrollBottom() const noexcept
{
    return currentScreen().scrollBottom;
}

int QTermInputExecutor::rows() const noexcept
{
    return currentScreen().buffer.rows();
}

bool QTermInputExecutor::isAlternateScreenActive() const noexcept
{
    return m_modeState.alternateScreenActive;
}

void QTermInputExecutor::setBellHandler(const std::function<void()> &handler)
{
    m_bellHandler = handler;
}

void QTermInputExecutor::setWindowTitleHandler(const std::function<void(const QString &)> &handler)
{
    m_windowTitleHandler = handler;
}

void QTermInputExecutor::setOutboundHandler(const std::function<void(const QByteArray &)> &handler)
{
    m_outboundHandler = handler;
}

void QTermInputExecutor::print(const QString &text)
{
    // DEC line drawing translation: if active and the character is in the
    // special range 0x60-0x7e, remap to the corresponding Unicode symbol.
    if (currentScreen().lineDrawingMode && text.size() == 1) {
        const ushort code = text.front().unicode();
        if (code >= 0x60 && code <= 0x7e) {
            const QChar mapped = applyLineDrawing(text.front());
            if (mapped != text.front()) {
                print(QString(mapped));
                return;
            }
        }
    }

    const int width = displayWidth(text);

    if (width == 0) {
        int targetColumn = currentScreen().wrapPending
            ? currentScreen().cursorState.column
            : currentScreen().cursorState.column - 1;

        if (targetColumn >= 0) {
            currentScreen().buffer.lineAt(currentScreen().cursorState.row).appendCombiningMark(targetColumn, text);
        }
        return;
    }

    if (currentScreen().wrapPending) {
        if (m_modeState.autoWrap) {
            wrapToNextLine();
        } else {
            // Auto-wrap disabled: overwrite at last column, reset pending state.
            currentScreen().wrapPending = false;
            setCursorState(QTermCursorState{currentScreen().cursorState.row,
                                           currentScreen().buffer.columns() - 1});
        }
    }

    if (width > 1 && currentScreen().cursorState.column == currentScreen().buffer.columns() - 1) {
        if (m_modeState.autoWrap) {
            currentScreen().wrapPending = true;
            wrapToNextLine();
        } else {
            // Cannot fit a 2-wide character at the last column without wrapping; skip it.
            return;
        }
    }

    // If a carriageReturn() preceded this write without an intervening lineFeed,
    // sever any predecessor wrap chain before committing the character. This
    // handles shell prompt redraw after resize: the shell issues CR then
    // overwrites the current physical row with new content. Without severing,
    // the predecessor row's wrappedToNextLine flag causes reflow to merge its
    // content with the newly written text, corrupting logical line boundaries.
    if (currentScreen().breakPredecessorWrapOnWrite) {
        currentScreen().breakPredecessorWrapOnWrite = false;
        // Sever and clear the entire predecessor wrap chain in the visible region,
        // then reposition the cursor to where the chain started. This handles the
        // shell prompt-redraw pattern after a resize: the shell issues CR to move
        // to the physical row start and then overwrites with new content. Because
        // reflow may have split the logical line into multiple physical rows, the
        // earlier rows (now orphaned reflow fragments) must be cleared so they
        // don't appear as stale content or blank lines in the next reflow.
        const int chainStart = currentScreen().buffer.severPredecessorWrapChain(
            currentScreen().cursorState.row);
        if (chainStart < currentScreen().cursorState.row) {
            setCursorState(QTermCursorState{chainStart, 0});
        }
    }

    currentScreen().buffer.lineAt(currentScreen().cursorState.row).setCharacter(
        currentScreen().cursorState.column,
        text,
        width,
        currentScreen().currentAttributes);

    if (currentScreen().cursorState.column + width >= currentScreen().buffer.columns()) {
        if (m_modeState.autoWrap) {
            currentScreen().wrapPending = true;
        }
        setCursorState(QTermCursorState{currentScreen().cursorState.row, currentScreen().buffer.columns() - 1});
        return;
    }

    setCursorState(QTermCursorState{currentScreen().cursorState.row, currentScreen().cursorState.column + width});
}

void QTermInputExecutor::lineFeed()
{
    currentScreen().wrapPending = false;
    currentScreen().breakPredecessorWrapOnWrite = false;
    currentScreen().buffer.lineAt(currentScreen().cursorState.row).setWrappedToNextLine(false);
    advanceToNextRow();
}

void QTermInputExecutor::carriageReturn()
{
    currentScreen().wrapPending = false;
    currentScreen().buffer.lineAt(currentScreen().cursorState.row).setWrappedToNextLine(false);
    // Flag that the next character write should sever any predecessor wrap chain.
    // This is needed when a shell redraws the prompt after resize by issuing
    // CR followed by new content on the same physical row.
    currentScreen().breakPredecessorWrapOnWrite = true;
    setCursorState(QTermCursorState{currentScreen().cursorState.row, 0});
}

void QTermInputExecutor::backspace()
{
    if (currentScreen().wrapPending) {
        currentScreen().wrapPending = false;
        return;
    }

    setCursorState(QTermCursorState{currentScreen().cursorState.row, qMax(0, currentScreen().cursorState.column - 1)});
}

void QTermInputExecutor::horizontalTab()
{
    const int nextTabStop = ((currentScreen().cursorState.column / kTabWidth) + 1) * kTabWidth;
    const int targetColumn = qMax(nextTabStop, currentScreen().cursorState.column + 1);

    for (int column = currentScreen().cursorState.column; column < targetColumn; ++column) {
        if (currentScreen().wrapPending) {
            wrapToNextLine();
        } else if (currentScreen().cursorState.column == currentScreen().buffer.columns() - 1) {
            currentScreen().wrapPending = true;
        } else {
            setCursorState(QTermCursorState{currentScreen().cursorState.row, currentScreen().cursorState.column + 1});
        }
    }
}

void QTermInputExecutor::cursorUp(int count)
{
    currentScreen().wrapPending = false;
    setCursorState(QTermCursorState{qMax(0, currentScreen().cursorState.row - qMax(1, count)), currentScreen().cursorState.column});
}

void QTermInputExecutor::cursorDown(int count)
{
    currentScreen().wrapPending = false;
    setCursorState(QTermCursorState{qMin(currentScreen().buffer.rows() - 1, currentScreen().cursorState.row + qMax(1, count)), currentScreen().cursorState.column});
}

void QTermInputExecutor::cursorForward(int count)
{
    currentScreen().wrapPending = false;
    setCursorState(QTermCursorState{currentScreen().cursorState.row, qMin(currentScreen().buffer.columns() - 1, currentScreen().cursorState.column + qMax(1, count))});
}

void QTermInputExecutor::cursorBackward(int count)
{
    currentScreen().wrapPending = false;
    setCursorState(QTermCursorState{currentScreen().cursorState.row, qMax(0, currentScreen().cursorState.column - qMax(1, count))});
}

void QTermInputExecutor::cursorHorizontalAbsolute(int column)
{
    currentScreen().wrapPending = false;
    const int targetColumn = qBound(0, column, currentScreen().buffer.columns() - 1);
    // Moving to column 0 is semantically equivalent to CR: any subsequent write
    // should sever the predecessor wrap chain so that shell prompt redraws
    // (which use ESC[1G instead of \r) work correctly after a resize.
    if (targetColumn == 0) {
        currentScreen().buffer.lineAt(currentScreen().cursorState.row).setWrappedToNextLine(false);
        currentScreen().breakPredecessorWrapOnWrite = true;
    }
    setCursorState(QTermCursorState{currentScreen().cursorState.row, targetColumn});
}

void QTermInputExecutor::cursorPosition(int row, int column)
{
    currentScreen().wrapPending = false;
    setCursorState(QTermCursorState{
        qBound(0, row, currentScreen().buffer.rows() - 1),
        qBound(0, column, currentScreen().buffer.columns() - 1)
    });
}

void QTermInputExecutor::eraseInLine(int mode)
{
    currentScreen().wrapPending = false;

    switch (mode) {
    case 1:
        currentScreen().buffer.lineAt(currentScreen().cursorState.row).clearToColumn(currentScreen().cursorState.column);
        break;
    case 2:
        currentScreen().buffer.lineAt(currentScreen().cursorState.row).clear();
        break;
    case 0:
    default:
        currentScreen().buffer.lineAt(currentScreen().cursorState.row).clearToEnd(currentScreen().cursorState.column);
        break;
    }
}

void QTermInputExecutor::eraseInDisplay(int mode)
{
    currentScreen().wrapPending = false;

    switch (mode) {
    case 1:
        currentScreen().buffer.clearVisibleTo(currentScreen().cursorState.row, currentScreen().cursorState.column);
        break;
    case 2:
        currentScreen().buffer.clearVisible();
        break;
    case 3:
        // Erase scrollback (history) only; visible screen is untouched.
        currentScreen().buffer.clearHistory();
        break;
    case 0:
    default:
        currentScreen().buffer.clearVisibleFrom(currentScreen().cursorState.row, currentScreen().cursorState.column);
        break;
    }
}

void QTermInputExecutor::characterAttributes(const QVector<int> &parameters)
{
    const QVector<int> normalized = parameters.isEmpty() ? QVector<int>{0} : parameters;

    for (int parameterIndex = 0; parameterIndex < normalized.size();) {
        const int parameter = normalized.at(parameterIndex);
        switch (parameter) {
        case 0:
            currentScreen().currentAttributes = QTermCellAttributes();
            ++parameterIndex;
            break;
        case 1:
            currentScreen().currentAttributes.bold = true;
            ++parameterIndex;
            break;
        case 2:
            currentScreen().currentAttributes.dim = true;
            ++parameterIndex;
            break;
        case 3:
            currentScreen().currentAttributes.italic = true;
            ++parameterIndex;
            break;
        case 4:
            currentScreen().currentAttributes.underline = true;
            ++parameterIndex;
            break;
        case 7:
            currentScreen().currentAttributes.inverse = true;
            ++parameterIndex;
            break;
        case 9:
            currentScreen().currentAttributes.strikethrough = true;
            ++parameterIndex;
            break;
        case 22:
            currentScreen().currentAttributes.bold = false;
            currentScreen().currentAttributes.dim = false;
            ++parameterIndex;
            break;
        case 23:
            currentScreen().currentAttributes.italic = false;
            ++parameterIndex;
            break;
        case 24:
            currentScreen().currentAttributes.underline = false;
            ++parameterIndex;
            break;
        case 27:
            currentScreen().currentAttributes.inverse = false;
            ++parameterIndex;
            break;
        case 29:
            currentScreen().currentAttributes.strikethrough = false;
            ++parameterIndex;
            break;
        case 39:
            currentScreen().currentAttributes.foregroundIndex = -1;
            currentScreen().currentAttributes.foregroundRgb = -1;
            ++parameterIndex;
            break;
        case 49:
            currentScreen().currentAttributes.backgroundIndex = -1;
            currentScreen().currentAttributes.backgroundRgb = -1;
            ++parameterIndex;
            break;
        default:
            if (parameter >= 30 && parameter <= 37) {
                currentScreen().currentAttributes.foregroundIndex = parameter - 30;
                currentScreen().currentAttributes.foregroundRgb = -1;
            } else if (parameter >= 40 && parameter <= 47) {
                currentScreen().currentAttributes.backgroundIndex = parameter - 40;
                currentScreen().currentAttributes.backgroundRgb = -1;
            } else if (parameter >= 90 && parameter <= 97) {
                currentScreen().currentAttributes.foregroundIndex = 8 + (parameter - 90);
                currentScreen().currentAttributes.foregroundRgb = -1;
            } else if (parameter >= 100 && parameter <= 107) {
                currentScreen().currentAttributes.backgroundIndex = 8 + (parameter - 100);
                currentScreen().currentAttributes.backgroundRgb = -1;
            } else if (parameter == 38) {
                parameterIndex += consumeExtendedColor(normalized, parameterIndex, true, currentScreen().currentAttributes);
                continue;
            } else if (parameter == 48) {
                parameterIndex += consumeExtendedColor(normalized, parameterIndex, false, currentScreen().currentAttributes);
                continue;
            }
            ++parameterIndex;
            break;
        }
    }
}

void QTermInputExecutor::insertCharacters(int count)
{
    currentScreen().wrapPending = false;
    currentScreen().buffer.lineAt(currentScreen().cursorState.row).insertCells(currentScreen().cursorState.column, qMax(1, count));
}

void QTermInputExecutor::deleteCharacters(int count)
{
    currentScreen().wrapPending = false;
    currentScreen().buffer.lineAt(currentScreen().cursorState.row).deleteCells(currentScreen().cursorState.column, qMax(1, count));
}

void QTermInputExecutor::insertLines(int count)
{
    currentScreen().wrapPending = false;
    currentScreen().buffer.insertLines(currentScreen().cursorState.row,
                                       qMax(1, count),
                                       currentScreen().scrollTop,
                                       currentScreen().scrollBottom);
}

void QTermInputExecutor::deleteLines(int count)
{
    currentScreen().wrapPending = false;
    currentScreen().buffer.deleteLines(currentScreen().cursorState.row,
                                       qMax(1, count),
                                       currentScreen().scrollTop,
                                       currentScreen().scrollBottom);
}

void QTermInputExecutor::scrollUpRegion(int count)
{
    // CSI S — scroll up: delete lines at the top of the scroll region,
    // inserting blank lines at the bottom. Cursor does not move.
    currentScreen().wrapPending = false;
    currentScreen().buffer.deleteLines(currentScreen().scrollTop,
                                       qMax(1, count),
                                       currentScreen().scrollTop,
                                       currentScreen().scrollBottom);
}

void QTermInputExecutor::scrollDownRegion(int count)
{
    // CSI T — scroll down: insert blank lines at the top of the scroll region,
    // pushing existing lines down and discarding lines at the bottom.
    // Cursor does not move.
    currentScreen().wrapPending = false;
    currentScreen().buffer.insertLines(currentScreen().scrollTop,
                                       qMax(1, count),
                                       currentScreen().scrollTop,
                                       currentScreen().scrollBottom);
}

void QTermInputExecutor::eraseCharacters(int count)
{
    // CSI X — erase characters at the cursor position without moving the
    // cursor. Unlike delete characters (CSI P), this does not shift content.
    currentScreen().wrapPending = false;
    const int col = currentScreen().cursorState.column;
    const int cols = currentScreen().buffer.columns();
    const int bounded = qMin(qMax(1, count), cols - col);
    for (int i = 0; i < bounded; ++i) {
        currentScreen().buffer.lineAt(currentScreen().cursorState.row).clearCharacterAt(col + i);
    }
}

void QTermInputExecutor::linePositionAbsolute(int row)
{
    // CSI d — move cursor to the given row (1-based, already converted to
    // 0-based by the caller), keeping the current column.
    currentScreen().wrapPending = false;
    const int bounded = qBound(0, row, currentScreen().buffer.rows() - 1);
    setCursorState(QTermCursorState{bounded, currentScreen().cursorState.column});
}

void QTermInputExecutor::setScrollRegion(int top, int bottom)
{
    const int normalizedTop = qBound(0, top, currentScreen().buffer.rows() - 1);
    const int normalizedBottom = qBound(0, bottom, currentScreen().buffer.rows() - 1);
    if (normalizedTop >= normalizedBottom) {
        currentScreen().scrollTop = 0;
        currentScreen().scrollBottom = currentScreen().buffer.rows() - 1;
    } else {
        currentScreen().scrollTop = normalizedTop;
        currentScreen().scrollBottom = normalizedBottom;
    }

    // DECSTBM always moves the cursor to the home position (0, 0) and clears
    // any pending wrap state, regardless of where the scroll region starts.
    currentScreen().wrapPending = false;
    currentScreen().breakPredecessorWrapOnWrite = false;
    setCursorState(QTermCursorState{0, 0});
}

void QTermInputExecutor::saveCursor()
{
    currentScreen().savedCursorState = QTermSavedCursorState{currentScreen().cursorState, currentScreen().currentAttributes};
}

void QTermInputExecutor::restoreCursor()
{
    if (!currentScreen().savedCursorState.has_value()) {
        return;
    }

    currentScreen().currentAttributes = currentScreen().savedCursorState->attributes;
    setCursorState(currentScreen().savedCursorState->cursorState);
    currentScreen().wrapPending = false;
}

void QTermInputExecutor::bell()
{
    if (m_bellHandler) {
        m_bellHandler();
    }
}

void QTermInputExecutor::setPrivateModes(const QVector<int> &parameters, bool enabled)
{
    for (int parameter : parameters) {
        switch (parameter) {
        case 1:
            m_modeState.applicationCursorKeys = enabled;
            break;
        case 7:
            // DECAWM — Auto-Wrap Mode
            m_modeState.autoWrap = enabled;
            break;
        case 25:
            m_modeState.cursorVisible = enabled;
            break;
        case 47:
        case 1047:
            if (enabled) {
                enterAlternateScreen(false, true);
            } else {
                leaveAlternateScreen(false);
            }
            break;
        case 1049:
            if (enabled) {
                enterAlternateScreen(true, true);
            } else {
                leaveAlternateScreen(true);
            }
            break;
        case 2004:
            m_modeState.bracketedPaste = enabled;
            break;
        // 鼠标事件类型：?1000, ?1002, ?1003
        case 1000:
            m_modeState.mouseTracking = enabled ? MouseTracking::X10 : MouseTracking::Disabled;
            break;
        case 1002:
            m_modeState.mouseTracking = enabled ? MouseTracking::Button : MouseTracking::Disabled;
            break;
        case 1003:
            m_modeState.mouseTracking = enabled ? MouseTracking::AnyEvent : MouseTracking::Disabled;
            break;
        // 鼠标编码格式：?1005（UTF-8，忽略），?1006（SGR），?1015（URXVT）
        case 1005:
            break;  // UTF-8 编码忽略，不影响事件类型
        case 1006:
            m_modeState.mouseEncoding = enabled ? MouseEncoding::SGR : MouseEncoding::Default;
            break;
        case 1015:
            m_modeState.mouseEncoding = enabled ? MouseEncoding::URXVT : MouseEncoding::Default;
            break;
        default:
            break;
        }
    }
}

void QTermInputExecutor::setWindowTitle(const QString &title)
{
    if (m_windowTitleHandler) {
        m_windowTitleHandler(title);
    }
}

QTermScreenState &QTermInputExecutor::currentScreen() noexcept
{
    return m_modeState.alternateScreenActive ? m_alternateScreen : m_primaryScreen;
}

const QTermScreenState &QTermInputExecutor::currentScreen() const noexcept
{
    return m_modeState.alternateScreenActive ? m_alternateScreen : m_primaryScreen;
}

QTermScreenState &QTermInputExecutor::inactiveScreen() noexcept
{
    return m_modeState.alternateScreenActive ? m_primaryScreen : m_alternateScreen;
}

void QTermInputExecutor::setCursorState(const QTermCursorState &cursorState)
{
    currentScreen().cursorState = cursorState;
}

void QTermInputExecutor::wrapToNextLine()
{
    currentScreen().buffer.lineAt(currentScreen().cursorState.row).setWrappedToNextLine(true);
    currentScreen().wrapPending = false;
    advanceToNextRow();
    setCursorState(QTermCursorState{currentScreen().cursorState.row, 0});
}

void QTermInputExecutor::advanceToNextRow()
{
    if (currentScreen().cursorState.row == currentScreen().scrollBottom) {
        if (currentScreen().scrollTop == 0 && currentScreen().scrollBottom == currentScreen().buffer.rows() - 1) {
            currentScreen().buffer.scrollUp();
            return;
        }

        currentScreen().buffer.deleteLines(currentScreen().scrollTop,
                                           1,
                                           currentScreen().scrollTop,
                                           currentScreen().scrollBottom);
        return;
    }

    // Cursor is not at the scroll-region bottom.  Advance one row if we are
    // not already at the last row of the screen; otherwise stay put.
    // (A cursor below the scroll region must NOT trigger a scroll.)
    if (currentScreen().cursorState.row < currentScreen().buffer.rows() - 1) {
        setCursorState(QTermCursorState{currentScreen().cursorState.row + 1, currentScreen().cursorState.column});
    }
}

void QTermInputExecutor::enterAlternateScreen(bool saveCursor, bool clearScreen)
{
    if (saveCursor) {
        m_primaryScreen.savedCursorState = QTermSavedCursorState{m_primaryScreen.cursorState, m_primaryScreen.currentAttributes};
    }

    if (clearScreen) {
        m_alternateScreen.clear();
    }

    m_modeState.alternateScreenActive = true;
}

void QTermInputExecutor::leaveAlternateScreen(bool restoreCursor)
{
    m_modeState.alternateScreenActive = false;
    if (restoreCursor && m_primaryScreen.savedCursorState.has_value()) {
        m_primaryScreen.currentAttributes = m_primaryScreen.savedCursorState->attributes;
        m_primaryScreen.cursorState = m_primaryScreen.savedCursorState->cursorState;
        m_primaryScreen.wrapPending = false;
    }
}

void QTermInputExecutor::deviceStatusReport()
{
    // CSI 6n — report current cursor position as ESC[row;colR (1-based).
    if (!m_outboundHandler) {
        return;
    }
    const int row = currentScreen().cursorState.row + 1;
    const int col = currentScreen().cursorState.column + 1;
    m_outboundHandler(QByteArray("\x1b[") + QByteArray::number(row) + ';' + QByteArray::number(col) + 'R');
}

void QTermInputExecutor::deviceAttributes()
{
    // CSI c (or CSI 0c) — primary device attributes.
    // Report as a VT100 with no options: ESC[?1;0c
    if (!m_outboundHandler) {
        return;
    }
    m_outboundHandler(QByteArray("\x1b[?1;0c"));
}

void QTermInputExecutor::secondaryDeviceAttributes()
{
    // CSI >c (or CSI >0c) — secondary device attributes.
    // Report as VT100 firmware version 0: ESC[>0;0;0c
    if (!m_outboundHandler) {
        return;
    }
    m_outboundHandler(QByteArray("\x1b[>0;0;0c"));
}

void QTermInputExecutor::reverseIndex()
{
    // ESC M — Reverse Index: move cursor up one row. If the cursor is already
    // at the top of the scroll region, insert a blank line there instead.
    currentScreen().wrapPending = false;
    if (currentScreen().cursorState.row == currentScreen().scrollTop) {
        currentScreen().buffer.insertLines(currentScreen().scrollTop, 1,
                                           currentScreen().scrollTop,
                                           currentScreen().scrollBottom);
    } else {
        setCursorState(QTermCursorState{currentScreen().cursorState.row - 1,
                                       currentScreen().cursorState.column});
    }
}

void QTermInputExecutor::designateCharacterSet(QChar intermediate, QChar final)
{
    // Only G0 designation (ESC ( x) affects the current screen.
    // ESC ( 0 → DEC line drawing; ESC ( B → ASCII (normal).
    if (intermediate == u'(') {
        currentScreen().lineDrawingMode = (final == u'0');
    }
    // G1 (ESC ) x) and others are accepted but currently ignored.
}

void QTermInputExecutor::reset()
{
    // ESC c — RIS (Reset to Initial State): full terminal reset.
    // Clears both screens, resets all modes, and positions the cursor at home.
    m_primaryScreen.clear();
    m_alternateScreen.clear();
    m_modeState = QTermModeState();
}

void QTermInputExecutor::cursorNextLine(int count)
{
    // CNL: move cursor down n rows and to column 0.
    currentScreen().wrapPending = false;
    const int newRow = qMin(currentScreen().buffer.rows() - 1,
                            currentScreen().cursorState.row + qMax(1, count));
    currentScreen().buffer.lineAt(currentScreen().cursorState.row).setWrappedToNextLine(false);
    currentScreen().breakPredecessorWrapOnWrite = true;
    setCursorState(QTermCursorState{newRow, 0});
}

void QTermInputExecutor::cursorPreviousLine(int count)
{
    // CPL: move cursor up n rows and to column 0.
    currentScreen().wrapPending = false;
    const int newRow = qMax(0, currentScreen().cursorState.row - qMax(1, count));
    currentScreen().breakPredecessorWrapOnWrite = true;
    setCursorState(QTermCursorState{newRow, 0});
}

void QTermInputExecutor::setKeypadMode(bool application)
{
    m_modeState.applicationKeypad = application;
}

} // namespace QTerm