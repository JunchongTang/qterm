#ifndef QTERMQUICKITEM_H
#define QTERMQUICKITEM_H

#include <QQuickPaintedItem>
#include <QFont>
#include <QColor>

#include <vterm.h>

#include "session/qtermsession.h"

/**
 * @brief Qt Quick terminal widget.
 *
 * Registered as QML type "TerminalItem" in the "QTerm" module.
 *
 * Rendering strategy (Phase 1):
 *   QQuickPaintedItem with software rasterization (QPainter).
 *   Each cell is drawn as:
 *     1. Background fill rect.
 *     2. Text character.
 *   The cursor is rendered as an inverted (XOR) rectangle on top.
 *
 * Input:
 *   - Key events are translated to libvterm key codes and forwarded to
 *     QTermSession.
 *   - Mouse press/move/release events are forwarded as terminal mouse
 *     events (row/col in cell coordinates).
 *   - Wheel events scroll the scrollback buffer (unless the terminal app
 *     has requested mouse tracking, in which case wheel events are forwarded
 *     as button 4/5 presses).
 */
class QTermQuickItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QTermSession *session READ session WRITE setSession NOTIFY sessionChanged)
    Q_PROPERTY(int scrollOffset READ scrollOffset WRITE setScrollOffset NOTIFY scrollOffsetChanged)
    Q_PROPERTY(int maxScrollOffset READ maxScrollOffset NOTIFY maxScrollOffsetChanged)

public:
    explicit QTermQuickItem(QQuickItem *parent = nullptr);
    ~QTermQuickItem() override;

    // --- Properties ---
    QTermSession *session() const { return m_session; }
    void setSession(QTermSession *session);

    int scrollOffset() const;
    void setScrollOffset(int offset);
    int maxScrollOffset() const;

    // --- QQuickPaintedItem ---
    void paint(QPainter *painter) override;

Q_SIGNALS:
    void sessionChanged();
    void scrollOffsetChanged();
    void maxScrollOffsetChanged();

protected:
    // Geometry
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

    // Input
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    // Focus
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private Q_SLOTS:
    void onDisplayChanged(int startRow, int endRow);
    void onSessionScrollOffsetChanged(int offset);

private:
    /** Compute cell dimensions from the current font. */
    void updateCellMetrics();

    /** Recalculate rows/cols and notify the session. */
    void updateTerminalSize();

    /** Convert pixel position to (row, col) in cell coordinates. */
    void pixelToCell(const QPointF &pos, int *row, int *col) const;

    /** Build a QColor from a VTermColor, resolving indexed colours. */
    QColor vtermColorToQt(const VTermColor &color, bool isFg) const;

    /** Translate a Qt key event to a VTermKey + VTermModifier pair. */
    static VTermKey qtKeyToVTerm(int qtKey, Qt::KeyboardModifiers mods,
                                  VTermModifier *outMod);

    QTermSession *m_session    = nullptr;

    QFont  m_font;
    qreal  m_cellWidth  = 8.0;
    qreal  m_cellHeight = 16.0;
    qreal  m_cellAscent = 13.0; // baseline offset from top of cell

    bool   m_hasFocus   = false;
    bool   m_cursorBlink = false; // reserved for future blink animation

    // Default colours (used when VTermColor is DEFAULT_FG/BG)
    QColor m_defaultFg{0xe0, 0xe0, 0xe0};
    QColor m_defaultBg{0x1e, 0x1e, 0x1e};

    // Tracks whether the last mouse press was within the item
    bool   m_mousePressed = false;
};

#endif // QTERMQUICKITEM_H
