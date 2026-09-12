// SPDX-License-Identifier: MIT
// Shared input/geometry/scroll controller – shared by all rendering backends.
// Paint-related logic (colours, QPainter, QSGNode) stays in the backend widget.

#pragma once

#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QString>
#include <QTimer>

// Forward-declare Qt event types so the header stays lightweight.
class QHoverEvent;
class QInputMethodEvent;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

namespace QTerm {

class QTermSurfaceModel;
class QTermTerminal;

class QTermViewController : public QObject
{
    Q_OBJECT
public:
    explicit QTermViewController(QObject *parent = nullptr);

    // ── Terminal binding ───────────────────────────────────────────────────
    QTermTerminal *terminal() const noexcept;
    void setTerminal(QTermTerminal *terminal);

    // ── Font and cell metrics ──────────────────────────────────────────────
    QString fontFamily() const;
    void setFontFamily(const QString &family);

    int fontPixelSize() const noexcept;
    void setFontPixelSize(int size);

    qreal cellWidth() const noexcept;
    qreal cellHeight() const noexcept;

    // ── Geometry, reported from geometryChange() / resizeEvent() ───────────
    void notifyGeometryChanged(qreal w, qreal h);

    // ── Scroll position, matching QML ScrollBar's position / size ──────────
    qreal scrollPosition() const noexcept;
    void setScrollPosition(qreal position);
    qreal scrollSize() const noexcept;

    // ── Coordinate helpers ─────────────────────────────────────────────────
    int rowAtPosition(qreal y) const;
    int columnAtPosition(qreal x) const;
    int hyperlinkIdAtPosition(int row, int col) const;

    // ── Mouse protocol state, driving the view's accepted buttons and hovering ──
    bool mouseProtocolEnabled() const;
    bool hoverEventsNeeded() const;

    // ── IME cursor rectangle, answering Qt::ImCursorRectangle ──────────────
    QRectF cursorRect() const;

    // ── Input dispatch; true means handled, so the view accepts the event ───
    bool handleKeyPress(QKeyEvent *event);
    bool handleInputMethod(QInputMethodEvent *event);
    bool handleMousePress(QMouseEvent *event);
    bool handleMouseDoubleClick(QMouseEvent *event);
    bool handleMouseMove(QMouseEvent *event);
    bool handleMouseRelease(QMouseEvent *event);
    bool handleHoverMove(QHoverEvent *event);
    bool handleWheel(QWheelEvent *event);

signals:
    void terminalChanged();
    void metricsChanged();          // cell metrics changed: repaint and resize
    void scrollChanged();
    void wheelScrolled(int scrollOffset);
    // Ctrl (⌘ on macOS) + wheel "zoom intent": steps>0 zoom in, <0 zoom out
    // (already accumulated per detent and de-bounced). QTerm itself does not change
    // the font — it only reports; the host (app) decides how to zoom (e.g. font size).
    void zoomRequested(int steps);
    void copyRequested(const QString &text);
    void hyperlinkActivated(const QString &url);
    // Right-click on the terminal. Carries everything a host needs to raise a menu
    // without laying its own MouseArea over the view: where to pop it up, which
    // cell it was over, and the OSC 8 link under it (0 when there is none).
    void contextMenuRequested(const QPointF &position, int row, int column,
                              int hyperlinkId);
    void mouseAcceptanceChanged();  // the view should call updateMouseAcceptance()
    void focusRequested();          // the view should call forceActiveFocus()
    void repaintNeeded();           // surface content or scroll changed: update()
    void cursorUpdateNeeded();      // cursor moved or changed visibility
    void selectionChanged();        // selection changed; the SG path redraws only that
    void contentRowsDirty(QVector<int> rows); // only these rows changed

private:
    void reconnectSurfaceModel();
    void disconnectSurfaceModel();
    void updateMetrics();
    void scheduleTerminalSizeSync();
    void syncTerminalSize();
    void updateSelectionFromDrag(qreal x, qreal y);

    // ── Signal connection handles ──────────────────────────────────────────
    QMetaObject::Connection m_viewportConnection;
    QMetaObject::Connection m_modeStateConnection;
    QMetaObject::Connection m_surfaceSizeConnection;
    QMetaObject::Connection m_surfaceCursorConnection;
    QMetaObject::Connection m_surfaceSelectionConnection;
    QMetaObject::Connection m_surfaceSearchConnection;
    QMetaObject::Connection m_surfaceVisibleRunsConnection;
    QMetaObject::Connection m_surfacePartialRunsConnection;
    QMetaObject::Connection m_surfaceDestroyedConnection;

    // ── State ──────────────────────────────────────────────────────────────
    QPointer<QTermTerminal> m_terminal;

    QTimer *m_resizeDebounceTimer    = nullptr;
    QTimer *m_selectionAutoScrollTimer = nullptr;
    QTimer *m_clickResetTimer        = nullptr;

#if defined(Q_OS_WIN)
    QString m_fontFamily    = QStringLiteral("Consolas");
#elif defined(Q_OS_MACOS)
    QString m_fontFamily    = QStringLiteral("Menlo");
#else
    QString m_fontFamily    = QStringLiteral("Monospace");
#endif
    int     m_fontPixelSize = 18;
    qreal   m_cellWidth     = 1.0;
    qreal   m_cellHeight    = 1.0;
    qreal   m_viewWidth     = 0.0;
    qreal   m_viewHeight    = 0.0;

    // ── Mouse and selection internals ──────────────────────────────────────
    int   m_clickStreak            = 0;
    int   m_lastClickRow           = -1;
    int   m_lastClickColumn        = -1;
    int   m_selectionAnchorRow     = -1;
    int   m_selectionAnchorColumn  = -1;
    // The selection anchor's **absolute projection row** (including scrollback);
    // pinned to the content during a drag, not drifting with the viewport — so the
    // start isn't lost when auto-scrolling across many rows. -1 = no anchor.
    int   m_selectionAnchorProjectionRow = -1;
    bool  m_suppressSelectionRelease = false;
    qreal m_dragX                  = 0.0;
    qreal m_dragY                  = 0.0;
    int   m_autoScrollDirection    = 0; // +1 = up, -1 = down
    // The wheel scrolls by "rows", but trackpads/Magic Mouse send high-resolution
    // pixel deltas (and fire high-frequency events during momentum). Pixels are
    // converted to fractional rows and accumulated here, scrolling only once a whole
    // row is reached — a sub-row delta does not force a 1-row scroll, otherwise the
    // momentum tail piles up into dozens of rows (the macOS over-scroll root cause).
    qreal m_wheelRowAccumulator    = 0.0;
    // Fractional step accumulator for Ctrl+wheel zoom (same de-bounce: a trackpad's
    // high-frequency pixel deltas must fill one step before a step is emitted).
    qreal m_zoomStepAccumulator    = 0.0;
};

} // namespace QTerm
