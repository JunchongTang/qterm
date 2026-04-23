#include "QTermInputExecutor.h"

#include <QChar>
#include <QtGlobal>

namespace {

constexpr int kTabWidth = 8;

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

void QTermInputExecutor::print(const QString &text)
{
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
        wrapToNextLine();
    }

    if (width > 1 && currentScreen().cursorState.column == currentScreen().buffer.columns() - 1) {
        currentScreen().wrapPending = true;
        wrapToNextLine();
    }

    currentScreen().buffer.lineAt(currentScreen().cursorState.row).setCharacter(
        currentScreen().cursorState.column,
        text,
        width,
        currentScreen().currentAttributes);

    if (currentScreen().cursorState.column + width >= currentScreen().buffer.columns()) {
        currentScreen().wrapPending = true;
        setCursorState(QTermCursorState{currentScreen().cursorState.row, currentScreen().buffer.columns() - 1});
        return;
    }

    setCursorState(QTermCursorState{currentScreen().cursorState.row, currentScreen().cursorState.column + width});
}

void QTermInputExecutor::lineFeed()
{
    currentScreen().wrapPending = false;
    currentScreen().buffer.lineAt(currentScreen().cursorState.row).setWrappedToNextLine(false);
    advanceToNextRow();
}

void QTermInputExecutor::carriageReturn()
{
    currentScreen().wrapPending = false;
    currentScreen().buffer.lineAt(currentScreen().cursorState.row).setWrappedToNextLine(false);
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

    setCursorState(QTermCursorState{currentScreen().scrollTop, 0});
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

    if (currentScreen().cursorState.row == currentScreen().buffer.rows() - 1) {
        currentScreen().buffer.scrollUp();
        return;
    }

    setCursorState(QTermCursorState{currentScreen().cursorState.row + 1, currentScreen().cursorState.column});
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

} // namespace QTerm