#ifndef QTERM_QTERMTERMINAL_H
#define QTERM_QTERMTERMINAL_H

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringDecoder>

#include <QTerm/QTermSession.h>
#include <QTerm/QTermSurfaceModel.h>

namespace QTerm {

class QTermCore;

class QTermTerminal final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int rows READ rows NOTIFY sizeChanged)
    Q_PROPERTY(int columns READ columns NOTIFY sizeChanged)
    Q_PROPERTY(QTerm::QTermSession *session READ session WRITE setSession NOTIFY sessionChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QTerm::QTermSurfaceModel *surfaceModel READ surfaceModel CONSTANT)

public:
    explicit QTermTerminal(QObject *parent = nullptr);

    int rows() const noexcept;
    int columns() const noexcept;
    QTermSession *session() const noexcept;
    QString title() const;
    QTermSurfaceModel *surfaceModel() noexcept;

    Q_INVOKABLE void clear();
    Q_INVOKABLE void feedText(const QString &text);
    Q_INVOKABLE void setTerminalSize(int columns, int rows);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void setSelectionRange(int startRow, int startColumn, int endRow, int endColumn);
    Q_INVOKABLE void selectWordAt(int row, int column);
    Q_INVOKABLE void sendKey(int key, const QString &text = QString());
    Q_INVOKABLE void sendPaste(const QString &text);

    void setSession(QTermSession *session);

public slots:
    void setTitle(const QString &title);

signals:
    void bell();
    void sizeChanged();
    void sessionChanged();
    void titleChanged();
    void outboundData(const QByteArray &data);

private:
    void syncSurfaceSelection();

    QTermCore *m_core = nullptr;
    QPointer<QTermSession> m_session;
    QTermSurfaceModel m_surfaceModel;
    bool m_hasSelection = false;
    int m_selectionStartRow = 0;
    int m_selectionStartColumn = 0;
    int m_selectionEndRow = 0;
    int m_selectionEndColumn = 0;
    QString m_title;
    QMetaObject::Connection m_sessionDataConnection;
    QMetaObject::Connection m_sessionDestroyedConnection;
    QMetaObject::Connection m_coreOutboundConnection;
    QMetaObject::Connection m_sizeToSessionResizeConnection;
    QStringDecoder m_sessionUtf8Decoder = QStringDecoder(QStringDecoder::Utf8);
};

} // namespace QTerm

#endif // QTERM_QTERMTERMINAL_H