#ifndef QTERM_QTERMINPUTEXECUTOR_H
#define QTERM_QTERMINPUTEXECUTOR_H

#include <optional>
#include <QString>
#include <QVector>

#include "QTermBuffer.h"
#include "QTermCursorState.h"

namespace QTerm {

struct QTermSavedCursorState
{
    QTermCursorState cursorState;
    QTermCellAttributes attributes;
};

class QTermInputExecutor
{
public:
    QTermInputExecutor(QTermBuffer &buffer,
                       const QTermCursorState &cursorState,
                       bool wrapPending,
                       const QTermCellAttributes &currentAttributes,
                       const std::optional<QTermSavedCursorState> &savedCursorState,
                       int scrollTop,
                       int scrollBottom);

    QTermCursorState cursorState() const noexcept;
    bool wrapPending() const noexcept;
    QTermCellAttributes currentAttributes() const noexcept;
    std::optional<QTermSavedCursorState> savedCursorState() const;
    int scrollTop() const noexcept;
    int scrollBottom() const noexcept;
    int rows() const noexcept;

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

private:
    void setCursorState(const QTermCursorState &cursorState);
    void wrapToNextLine();
    void advanceToNextRow();

    QTermBuffer &m_buffer;
    QTermCursorState m_cursorState;
    bool m_wrapPending = false;
    QTermCellAttributes m_currentAttributes;
    std::optional<QTermSavedCursorState> m_savedCursorState;
    int m_scrollTop = 0;
    int m_scrollBottom = 0;
};

} // namespace QTerm

#endif // QTERM_QTERMINPUTEXECUTOR_H