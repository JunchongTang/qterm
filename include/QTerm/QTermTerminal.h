#ifndef QTERM_QTERMTERMINAL_H
#define QTERM_QTERMTERMINAL_H

#include <QByteArray>
#include <memory>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>
#include <QStringDecoder>

#include <QtQml/qqmlregistration.h>

#include <QTerm/QTermSession.h>
#include <QTerm/QTermSurfaceModel.h>

namespace QTerm {

class QTermCore;
class QTermSelectionModel;

class QTermTerminal : public QObject
{
    Q_OBJECT
    QML_ELEMENT
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
    // In-buffer search status (for the host's ⌘F bar: "m / n", empty state).
    Q_PROPERTY(int searchMatchCount READ searchMatchCount NOTIFY searchChanged)
    Q_PROPERTY(int searchCurrentIndex READ searchCurrentIndex NOTIFY searchChanged) // 1-based; 0 = none

public:
    explicit QTermTerminal(QObject *parent = nullptr);
    ~QTermTerminal() override;

    int rows() const noexcept;
    int columns() const noexcept;
    int scrollOffset() const noexcept;
    // 当前视口顶在 projection 空间(含 scrollback)的绝对行号。视图层做选区拖拽时
    // 用它把视口内行换算成绝对行,使锚点不随滚动漂移。
    int viewportTopProjectionRow() const noexcept;
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
    QString dumpPlainText() const;
    // ANSI-encoded snapshot of the buffer (UTF-8 bytes with embedded SGR escapes).
    // Re-feeding via feedText() on a fresh terminal reproduces the same visual
    // state — used by host apps to restore scrollback after restart.
    Q_INVOKABLE QByteArray dumpAnsi(int maxLines = 5000) const;

    Q_INVOKABLE void clear();
    Q_INVOKABLE void feedText(const QString &text);
    Q_INVOKABLE void setTerminalSize(int columns, int rows);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void setSelectionRange(int startRow, int startColumn, int endRow, int endColumn);
    // 拖拽选区:锚点 + 拖拽点均以 **projection 绝对行**给出(视图层用
    // viewportTopProjectionRow() 换算),内部按 projection 端点 + logical 锚点建立
    // 选区(可跨 scrollback、自动滚动不丢起点),并对端点做字素对齐(宽字符/CJK
    // 整字纳入,高亮不再只覆盖半个字)。列为单元格列,半开区间由本方法内部处理。
    Q_INVOKABLE void setSelectionDrag(int anchorProjectionRow, int anchorColumn,
                                      int dragProjectionRow, int dragColumn);
    Q_INVOKABLE void selectWordAt(int row, int column);
    Q_INVOKABLE void selectLogicalLineAt(int row);
    Q_INVOKABLE void scrollByLines(int deltaRows);
    Q_INVOKABLE void scrollToBottom();

    // ── In-buffer search ───────────────────────────────────────────────────────
    // Scan the whole buffer (history + screen) for `query` and remember the
    // matches. Returns the match count; picks the match nearest the current
    // viewport as the current one and scrolls it into view. Matches are kept in
    // projection-row coordinates and re-projected to the viewport for rendering,
    // so they stay anchored to content while scrolling.
    Q_INVOKABLE int search(const QString &query, bool caseSensitive = false);
    Q_INVOKABLE void findNext();
    Q_INVOKABLE void findPrevious();
    Q_INVOKABLE void clearSearch();
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
    void searchChanged();
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

    int searchMatchCount() const noexcept;
    int searchCurrentIndex() const noexcept;
    // Project stored matches (projection-row coords) onto the current viewport
    // and push to the surface model; called on every viewport change + on search.
    void syncSurfaceSearch();
    // Scroll so the current match's projection row is visible.
    void scrollMatchIntoView();

    // A match in projection-row coordinates.
    struct SearchMatch {
        int projectionRow = 0;
        int startColumn = 0;
        int length = 0;
    };
    // Recompute matches against the (possibly mutated) buffer, preserving the
    // active query; clamps current index; no scroll. Called when buffer changes
    // while a search is active.
    void refreshSearch();

    QVector<SearchMatch> m_searchMatches;
    int m_searchCurrent = -1;          // index into m_searchMatches; -1 = none
    QString m_searchQuery;
    bool m_searchCaseSensitive = false;

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