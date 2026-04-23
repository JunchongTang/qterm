#ifndef QTERM_QTERMINPUTEXECUTOR_H
#define QTERM_QTERMINPUTEXECUTOR_H

#include <functional>
#include <QString>
#include <QVector>

#include "QTermModeState.h"
#include "QTermScreenState.h"

namespace QTerm {

class QTermInputExecutor
{
public:
    QTermInputExecutor(QTermScreenState &primaryScreen,
                       QTermScreenState &alternateScreen,
                       QTermModeState &modeState);

    QTermCursorState cursorState() const noexcept;
    const QTermModeState &modeState() const noexcept;
    QTermCellAttributes currentAttributes() const noexcept;
    std::optional<QTermSavedCursorState> savedCursorState() const;
    int scrollTop() const noexcept;
    int scrollBottom() const noexcept;
    int rows() const noexcept;
    bool isAlternateScreenActive() const noexcept;
    void setWindowTitleHandler(const std::function<void(const QString &)> &handler);

    void print(const QString &text);
    void lineFeed();
    void carriageReturn();
    void backspace();
    void horizontalTab();
    void cursorUp(int count);
    void cursorDown(int count);
    void cursorForward(int count);
    void cursorBackward(int count);
    void cursorPosition(int row, int column);
    void eraseInLine(int mode);
    void eraseInDisplay(int mode);
    void characterAttributes(const QVector<int> &parameters);
    void insertCharacters(int count);
    void deleteCharacters(int count);
    void insertLines(int count);
    void deleteLines(int count);
    void setScrollRegion(int top, int bottom);
    void saveCursor();
    void restoreCursor();
    void setPrivateModes(const QVector<int> &parameters, bool enabled);
    void setWindowTitle(const QString &title);

private:
    QTermScreenState &currentScreen() noexcept;
    const QTermScreenState &currentScreen() const noexcept;
    QTermScreenState &inactiveScreen() noexcept;
    void setCursorState(const QTermCursorState &cursorState);
    void wrapToNextLine();
    void advanceToNextRow();
    void enterAlternateScreen(bool saveCursor, bool clearScreen);
    void leaveAlternateScreen(bool restoreCursor);

    QTermScreenState &m_primaryScreen;
    QTermScreenState &m_alternateScreen;
    QTermModeState &m_modeState;
    std::function<void(const QString &)> m_windowTitleHandler;
};

} // namespace QTerm

#endif // QTERM_QTERMINPUTEXECUTOR_H