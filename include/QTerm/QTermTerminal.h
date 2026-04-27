#ifndef QTERM_QTERMTERMINAL_H
#define QTERM_QTERMTERMINAL_H

#include <QByteArray>
#include <memory>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringDecoder>

#include <QTerm/QTermSession.h>
#include <QTerm/QTermSurfaceModel.h>

namespace QTerm {

class QTermCore;
class QTermSelectionModel;

class QTermTerminal final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int rows READ rows NOTIFY sizeChanged)
    Q_PROPERTY(int columns READ columns NOTIFY sizeChanged)
    Q_PROPERTY(int scrollOffset READ scrollOffset NOTIFY viewportChanged)
    Q_PROPERTY(int maxScrollOffset READ maxScrollOffset NOTIFY viewportChanged)
    Q_PROPERTY(QTerm::QTermSession *session READ session WRITE setSession NOTIFY sessionChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString currentDirectory READ currentDirectory NOTIFY currentDirectoryChanged)
    // OSC 133 shell integration
    Q_PROPERTY(int shellZone READ shellZone NOTIFY shellZoneChanged)
    Q_PROPERTY(int lastExitCode READ lastExitCode NOTIFY shellZoneChanged)
    Q_PROPERTY(QTerm::QTermSurfaceModel *surfaceModel READ surfaceModel CONSTANT)

public:
    explicit QTermTerminal(QObject *parent = nullptr);
    ~QTermTerminal() override;

    int rows() const noexcept;
    int columns() const noexcept;
    int scrollOffset() const noexcept;
    int maxScrollOffset() const noexcept;
    QTermSession *session() const noexcept;
    QString title() const;
    QString currentDirectory() const;
    int shellZone() const noexcept;
    int lastExitCode() const noexcept;
    QTermSurfaceModel *surfaceModel() noexcept;

    // Mouse protocol state (for view-layer event routing)
    bool isMouseProtocolActive() const noexcept;
    bool isHoverTrackingActive() const noexcept;
    bool isButtonTrackingActive() const noexcept;

    // OSC 8: resolve a hyperlink id (from a style run) to its URL
    Q_INVOKABLE QString hyperlinkUrl(int id) const;
    // Returns the full buffer content as plain text; intended for tests and debugging.
    QString debugPlainText() const;

    Q_INVOKABLE void clear();
    Q_INVOKABLE void feedText(const QString &text);
    Q_INVOKABLE void setTerminalSize(int columns, int rows);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void setSelectionRange(int startRow, int startColumn, int endRow, int endColumn);
    Q_INVOKABLE void selectWordAt(int row, int column);
    Q_INVOKABLE void selectLogicalLineAt(int row);
    Q_INVOKABLE void scrollByLines(int deltaRows);
    Q_INVOKABLE void scrollToBottom();
    Q_INVOKABLE void sendKey(int key, const QString &text = QString());
    Q_INVOKABLE void sendPaste(const QString &text);
    Q_INVOKABLE void sendMouse(int row, int column, int button, int modifiers, bool isPress, bool isMotion = false);

    void setSession(QTermSession *session);
    // Initial title can be set by embedding application; thereafter updated by OSC 0/2
    void setTitle(const QString &title);

private slots:
    void setCurrentDirectory(const QString &url);

signals:
    void bell();
    void sizeChanged();
    void viewportChanged();
    void sessionChanged();
    void titleChanged();
    void currentDirectoryChanged();
    void shellZoneChanged();
    void clipboardWriteRequested(const QString &text);
    void modeStateChanged();
    void outboundData(const QByteArray &data);

private:
    static void syncSurfaceCursor(QTermSurfaceModel &surfaceModel, QTermCore *core, int viewportTopProjectionRow);
    int maxViewportTopProjectionRow() const noexcept;
    void clampViewportToBuffer();
    void syncSurfaceViewport();
    void syncSurfaceSelection();

    QTermCore *m_core = nullptr;
    QPointer<QTermSession> m_session;
    QTermSurfaceModel m_surfaceModel;
    std::unique_ptr<QTermSelectionModel> m_selectionModel;
    QString m_title;
    QString m_currentDirectory;
    QMetaObject::Connection m_sessionDataConnection;
    QMetaObject::Connection m_sessionDestroyedConnection;
    QMetaObject::Connection m_coreOutboundConnection;
    QMetaObject::Connection m_sizeToSessionResizeConnection;
    QStringDecoder m_sessionUtf8Decoder = QStringDecoder(QStringDecoder::Utf8);
    int m_viewportTopProjectionRow = 0;
    int m_lastSyncedViewportTop = -1; // -1 forces full sync on first call
    bool m_viewportPinnedToBottom = true;
};

} // namespace QTerm

#endif // QTERM_QTERMTERMINAL_H