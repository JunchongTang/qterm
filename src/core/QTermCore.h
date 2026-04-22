#ifndef QTERM_QTERMCORE_H
#define QTERM_QTERMCORE_H

#include <QObject>
#include <QString>
#include <optional>

#include "QTermBuffer.h"
#include "QTermCursorState.h"
#include "QTermInputExecutor.h"
#include "QTermTextParser.h"

namespace QTerm {

class QTermCore final : public QObject
{
    Q_OBJECT

public:
    explicit QTermCore(QObject *parent = nullptr);

    int rows() const noexcept;
    int columns() const noexcept;
    QString debugPlainText() const;
    QTermCursorState cursorState() const noexcept;
    const QTermBuffer &buffer() const noexcept;

    void clear();
    void writePlainText(const QString &text);
    void setTerminalSize(int columns, int rows);

signals:
    void sizeChanged();
    void debugPlainTextChanged();
    void cursorStateChanged();

private:
    void setCursorState(const QTermCursorState &cursorState);

    QTermBuffer m_buffer;
    QTermCursorState m_cursorState;
    bool m_wrapPending = false;
    QTermTextParser m_textParser;
    QTermCellAttributes m_currentAttributes;
    std::optional<QTermSavedCursorState> m_savedCursorState;
    int m_scrollTop = 0;
    int m_scrollBottom = 23;
};

} // namespace QTerm

#endif // QTERM_QTERMCORE_H