#ifndef QTERM_QTERMCORE_H
#define QTERM_QTERMCORE_H

#include <QObject>
#include <QString>

#include "QTermModeState.h"
#include "QTermScreenState.h"
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
    const QTermModeState &modeState() const noexcept;

    void clear();
    void writePlainText(const QString &text);
    void setTerminalSize(int columns, int rows);

signals:
    void sizeChanged();
    void debugPlainTextChanged();
    void cursorStateChanged();

private:
    const QTermScreenState &activeScreen() const noexcept;

    QTermScreenState m_primaryScreen;
    QTermScreenState m_alternateScreen;
    QTermModeState m_modeState;
    QTermTextParser m_textParser;
};

} // namespace QTerm

#endif // QTERM_QTERMCORE_H