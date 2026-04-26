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
    void setBellHandler(const std::function<void()> &handler);
    void setWindowTitleHandler(const std::function<void(const QString &)> &handler);
    void setOutboundHandler(const std::function<void(const QByteArray &)> &handler);
    void setRegisterHyperlinkHandler(const std::function<int(const QString &)> &handler);

    void print(const QString &text);
    void lineFeed();
    void carriageReturn();
    void backspace();
    void horizontalTab();
    void cursorUp(int count);
    void cursorDown(int count);
    void cursorForward(int count);
    void cursorBackward(int count);
    void cursorHorizontalAbsolute(int column);
    void cursorPosition(int row, int column);
    void eraseInLine(int mode);
    void eraseInDisplay(int mode);
    void characterAttributes(const QVector<int> &parameters);
    void insertCharacters(int count);
    void deleteCharacters(int count);
    void insertLines(int count);
    void deleteLines(int count);
    void scrollUpRegion(int count);
    void scrollDownRegion(int count);
    void eraseCharacters(int count);
    void linePositionAbsolute(int row);
    void setScrollRegion(int top, int bottom);
    void saveCursor();
    void restoreCursor();
    void bell();
    void reset();
    void setPrivateModes(const QVector<int> &parameters, bool enabled);
    void setWindowTitle(const QString &title);
    void deviceStatusReport();
    void deviceAttributes();
    void secondaryDeviceAttributes();
    void reverseIndex();
    void cursorNextLine(int count);
    void cursorPreviousLine(int count);
    void designateCharacterSet(QChar intermediate, QChar final);
    void setKeypadMode(bool application);
    // OSC 8: url=empty to close the link
    void setHyperlink(const QString &url);
    // DECSCUSR: CSI n SP q  (n=0..6)
    void setCursorShape(int parameter);

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
    std::function<void()> m_bellHandler;
    std::function<void(const QString &)> m_windowTitleHandler;
    std::function<void(const QByteArray &)> m_outboundHandler;
    std::function<int(const QString &)> m_registerHyperlinkHandler;
};

} // namespace QTerm

#endif // QTERM_QTERMINPUTEXECUTOR_H