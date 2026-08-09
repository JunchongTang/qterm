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

/*!
    \class QTermTerminal
    \inmodule QTerm
    \brief Core terminal model and controller for buffer, selection and viewport state.

    QTermTerminal owns the terminal buffer, selection state and the connection to
    a session backend. It is the main object consumed by the widget and QML layers.
*/
class QTermTerminal : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int rows READ rows NOTIFY sizeChanged)
    Q_PROPERTY(int columns READ columns NOTIFY sizeChanged)
    Q_PROPERTY(int maximumScrollbackLines READ maximumScrollbackLines WRITE setMaximumScrollbackLines NOTIFY maximumScrollbackLinesChanged)
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

    /*!
        \brief Returns the number of rows currently visible in the viewport.
    */
    int rows() const noexcept;

    /*!
        \brief Returns the number of columns currently visible in the viewport.
    */
    int columns() const noexcept;

    /*!
        \brief Returns the maximum number of scrollback lines retained in the buffer.
    */
    int maximumScrollbackLines() const noexcept;

    /*!
        \brief Returns the current scroll offset from the bottom of the buffer.
    */
    int scrollOffset() const noexcept;
    /*!
        \brief Returns the absolute row index, in projection space, of the viewport top.

        Projection space includes scrollback. The view layer uses this to map
        viewport-relative rows to absolute rows so a selection anchor does not
        drift while scrolling.
    */
    int viewportTopProjectionRow() const noexcept;

    /*!
        \brief Returns the maximum scroll offset available for the current buffer.
    */
    int maxScrollOffset() const noexcept;

    /*!
        \brief Returns the session currently attached to this terminal.
    */
    QTermSession *session() const noexcept;

    /*!
        \brief Returns the terminal title advertised by the shell or application.
    */
    QString title() const;

    /*!
        \brief Returns the current working directory reported by the shell integration layer.
    */
    QString currentDirectory() const;

    /*!
        \brief Returns the current shell integration zone index.
    */
    int shellZone() const noexcept;

    /*!
        \brief Returns the last shell exit code reported by shell integration.
    */
    int lastExitCode() const noexcept;

    /*!
        \brief Returns the surface model that exposes the viewport and selection state.
    */
    QTermSurfaceModel *surfaceModel() noexcept;

    // Mouse protocol state (for view-layer event routing)
    bool isMouseProtocolActive() const noexcept;
    bool isHoverTrackingActive() const noexcept;
    bool isButtonTrackingActive() const noexcept;

    /*!
        \brief Resolves an OSC 8 hyperlink identifier to its target URL.
        \param id The hyperlink identifier carried by a style run.
        \return The resolved URL, or an empty string if no hyperlink matches.
    */
    Q_INVOKABLE QString hyperlinkUrl(int id) const;
    /*!
        \brief Returns the full buffer content as plain text.
        \return The complete terminal content, intended for tests and debugging.
    */
    QString dumpPlainText() const;
    /*!
        \brief Returns an ANSI-encoded snapshot of the buffer.
        \return UTF-8 bytes with embedded SGR escapes.

        Re-feeding the result via feedText() on a fresh terminal reproduces the
        same visual state, which host applications use to restore scrollback
        after a restart.
    */
    Q_INVOKABLE QByteArray dumpAnsi(int maxLines = 5000) const;

    /*!
        \brief Clears the terminal buffer and resets the viewport state.
    */
    Q_INVOKABLE void clear();

    /*!
        \brief Feeds text into the terminal as if it had been typed by the user.
        \param text The text to feed to the terminal parser.
    */
    Q_INVOKABLE void feedText(const QString &text);

    /*!
        \brief Sets the maximum number of scrollback lines retained by the terminal.
        \param maximumScrollbackLines The maximum scrollback size.
    */
    Q_INVOKABLE void setMaximumScrollbackLines(int maximumScrollbackLines);

    /*!
        \brief Sets the visible viewport size in columns and rows.
        \param columns The number of columns.
        \param rows The number of rows.
    */
    Q_INVOKABLE void setTerminalSize(int columns, int rows);

    /*!
        \brief Clears the current selection from the terminal buffer.
    */
    Q_INVOKABLE void clearSelection();

    /*!
        \brief Sets a rectangular selection range in the terminal buffer.
    */
    Q_INVOKABLE void setSelectionRange(int startRow, int startColumn, int endRow, int endColumn);
    /*!
        \brief Updates a drag selection from absolute projection rows.

        Both endpoints are absolute projection rows; the view maps them via
        viewportTopProjectionRow(). The selection is built from projection
        endpoints plus logical anchors, so it spans scrollback and never loses
        its start during auto-scroll, and its endpoints snap to grapheme
        boundaries so a wide or CJK character is always selected whole. Columns
        are cell columns; the half-open interval is handled internally.
    */
    Q_INVOKABLE void setSelectionDrag(int anchorProjectionRow, int anchorColumn,
                                      int dragProjectionRow, int dragColumn);

    /*!
        \brief Selects the whole buffer, scrollback included.

        Equivalent to a drag from the first cell of the oldest projection row to
        the end of the last row that has content, so it goes through the same
        logical-anchor path as a real drag selection (and therefore survives
        auto-scroll and reflow). Blank rows past the end of the output are left
        out, so Select All followed by a copy does not yield trailing newlines.
        Clears the selection when the buffer holds nothing but blanks.
    */
    Q_INVOKABLE void selectAll();

    /*!
        \brief Selects the word under the given cursor position.
    */
    Q_INVOKABLE void selectWordAt(int row, int column);

    /*!
        \brief Selects the full logical line at the specified row.
    */
    Q_INVOKABLE void selectLogicalLineAt(int row);

    /*!
        \brief Scrolls the viewport by a relative number of lines.
        \param deltaRows The number of lines to move the viewport.
    */
    Q_INVOKABLE void scrollByLines(int deltaRows);

    /*!
        \brief Scrolls the viewport to the bottom of the buffer.
    */
    Q_INVOKABLE void scrollToBottom();

    // ── In-buffer search ───────────────────────────────────────────────────────
    /*!
        \brief Searches the whole buffer, history included, for \a query.
        \return The number of matches.

        Picks the match nearest the current viewport as the current one and
        scrolls it into view. Matches are kept in projection-row coordinates and
        re-projected for rendering, so they stay anchored to content while
        scrolling.
    */
    Q_INVOKABLE int search(const QString &query, bool caseSensitive = false);
    /*!
        \brief Moves to the next search match.
    */
    Q_INVOKABLE void findNext();
    /*!
        \brief Moves to the previous search match.
    */
    Q_INVOKABLE void findPrevious();
    /*!
        \brief Clears the current search and its highlights.
    */
    Q_INVOKABLE void clearSearch();
    /*!
        \brief Returns the number of matches found by the last search().
    */
    int searchMatchCount() const noexcept;
    /*!
        \brief Returns the 1-based position of the current match, or 0 if none.
    */
    int searchCurrentIndex() const noexcept;

    /*!
        \brief Sends a key event to the attached session.
        \param key The key code.
        \param text Optional text payload for printable keys.
    */
    Q_INVOKABLE void sendKey(int key, const QString &text = QString(),
                             Qt::KeyboardModifiers modifiers = Qt::NoModifier);

    /*!
        \brief Sends pasted text to the attached session.
        \param text The text to paste.
    */
    Q_INVOKABLE void sendPaste(const QString &text);

    /*!
        \brief Sends a mouse event to the attached session.
    */
    Q_INVOKABLE void sendMouse(int row, int column, int button, int modifiers, bool isPress, bool isMotion = false);

    /*!
        \brief Attaches a session backend to the terminal.
        \param session The session to use for I/O and state changes.
    */
    void setSession(QTermSession *session);

    /*!
        \brief Sets the terminal title.
        \param title The title to expose to the UI or shell integration layer.
    */
    void setTitle(const QString &title);

private slots:
    void setCurrentDirectory(const QString &url);

signals:
    void bell();
    void sizeChanged();
    void maximumScrollbackLinesChanged();
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