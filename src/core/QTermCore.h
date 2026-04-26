#ifndef QTERM_QTERMCORE_H
#define QTERM_QTERMCORE_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QHash>

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
    QString title() const;
    QString debugPlainText() const;
    QTermCursorState cursorState() const noexcept;
    const QTermBuffer &buffer() const noexcept;
    const QTermModeState &modeState() const noexcept;
    QByteArray encodeKey(int key, const QString &text = QString()) const;
    QByteArray encodePaste(const QString &text) const;

    // OSC 8: returns the URL registered for the given id, or empty string
    QString hyperlinkUrl(int id) const;
    // OSC 8: registers (or updates) a URL and returns its id; empty URL deactivates
    int registerHyperlinkUrl(const QString &url);

    void clear();
    void writePlainText(const QString &text);
    void setTerminalSize(int columns, int rows);
    void sendKey(int key, const QString &text = QString());
    void sendPaste(const QString &text);

signals:
    void bell();
    void sizeChanged();
    void titleChanged(const QString &title);
    void debugPlainTextChanged();
    void cursorStateChanged();
    void modeStateChanged();
    void outboundData(const QByteArray &data);

private:
    const QTermScreenState &activeScreen() const noexcept;

    QTermScreenState m_primaryScreen;
    QTermScreenState m_alternateScreen;
    QTermModeState m_modeState;
    QTermTextParser m_textParser;
    QString m_title;
    // OSC 8 hyperlink registry: id -> URL (id 0 is reserved for "no link")
    QHash<int, QString> m_hyperlinkUrls;
    int m_nextHyperlinkId = 1;
};

} // namespace QTerm

#endif // QTERM_QTERMCORE_H