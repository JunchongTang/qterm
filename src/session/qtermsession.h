#ifndef QTERMSESSION_H
#define QTERMSESSION_H

#include <QObject>
#include <QByteArray>

#include <vterm.h>

class QTermSessionBackend;
class QTermVTermProcessor;

/**
 * @brief Combines a session backend (PTY) and the vterm processor.
 *
 * QTermSession is the main entry point used by QTermQuickItem.
 * It manages:
 *  - Starting and stopping the backend.
 *  - Routing PTY output → vterm and vterm output → PTY.
 *  - Scroll offset: 0 = bottom (live view), positive = scrolled into scrollback.
 *  - Providing a unified cell-query API that transparently spans scrollback and
 *    the live screen.
 *  - Forwarding terminal resize to both backend and vterm.
 */
class QTermSession : public QObject
{
    Q_OBJECT

public:
    explicit QTermSession(QObject *parent = nullptr);
    ~QTermSession() override;

    /**
     * @brief Start the session (opens PTY, forks shell).
     *        Must be called once after construction.
     */
    void start();

    // --- Size ---

    /** Notify the session that the view size changed. */
    void resizeTerminal(int rows, int cols);

    int terminalRows() const;
    int terminalCols() const;

    // --- Input from the view ---

    void sendKey(VTermKey key, VTermModifier mod);
    void sendChar(uint32_t unichar, VTermModifier mod);
    void sendData(const QByteArray &data); // raw bytes (paste etc.)
    void startPaste();
    void endPaste();

    // --- Mouse from the view ---

    void sendMouseMove(int row, int col, VTermModifier mod);
    void sendMouseButton(int button, bool pressed, VTermModifier mod);

    // --- Scrollback ---

    /** Lines scrolled up from the bottom (0 = live view). */
    int scrollOffset() const { return m_scrollOffset; }

    /** Maximum allowed scroll offset. */
    int maxScrollOffset() const;

    /** Change the scroll offset; clamped automatically. */
    void setScrollOffset(int offset);

    /** Scroll by @p delta lines (positive = scroll up into scrollback). */
    void scrollBy(int delta);

    // --- Screen query ---

    /**
     * @brief Get the cell at logical display row @p row, column @p col.
     *
     * Row 0 is the top of the current viewport:
     *  - When scrollOffset == 0: row 0 maps to the top of the live screen.
     *  - When scrollOffset > 0: the top rows come from scrollback.
     *
     * @return true if the cell was filled successfully.
     */
    bool cellAt(int row, int col, VTermScreenCell *cell) const;

    // --- Cursor ---
    VTermPos cursorPos() const;
    bool isCursorVisible() const;

Q_SIGNALS:
    /** The visible area needs repainting (at least rows [startRow, endRow)). */
    void displayChanged(int startRow, int endRow);

    /** The scroll offset changed (e.g. new scrollback line was pushed). */
    void scrollOffsetChanged(int offset);

    /** The terminal title changed. */
    void titleChanged(const QString &title);

    /** Terminal bell. */
    void bellRang();

    /** The terminal process has finished. */
    void finished(int exitCode);

private Q_SLOTS:
    void onDataReceived(const QByteArray &data);
    void onScreenUpdated(int startRow, int endRow);
    void onScrollbackChanged();

private:
    QTermSessionBackend   *m_backend   = nullptr;
    QTermVTermProcessor   *m_processor = nullptr;

    int m_scrollOffset = 0; // lines scrolled up into scrollback (0 = bottom)
};

#endif // QTERMSESSION_H
