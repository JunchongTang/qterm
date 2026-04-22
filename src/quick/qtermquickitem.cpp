#include "qtermquickitem.h"

#include "session/qtermsession.h"

#include <QPainter>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFocusEvent>
#include <QDebug>

#include <vterm_keycodes.h>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

QTermQuickItem::QTermQuickItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    // Monospace font – Menlo is the macOS standard monospace face
    m_font.setFamily(QStringLiteral("Menlo"));
    m_font.setPointSize(13);
    m_font.setStyleHint(QFont::TypeWriter);

    updateCellMetrics();

    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(false);
    setFlag(ItemAcceptsInputMethod, true);
    setFlag(ItemIsFocusScope, true);
    setActiveFocusOnTab(true);
}

QTermQuickItem::~QTermQuickItem() = default;

// ---------------------------------------------------------------------------
// Session property
// ---------------------------------------------------------------------------

void QTermQuickItem::setSession(QTermSession *session)
{
    if (m_session == session)
        return;

    if (m_session)
        QObject::disconnect(m_session, nullptr, this, nullptr);

    m_session = session;

    if (m_session) {
        connect(m_session, &QTermSession::displayChanged,
                this, &QTermQuickItem::onDisplayChanged);
        connect(m_session, &QTermSession::scrollOffsetChanged,
                this, &QTermQuickItem::onSessionScrollOffsetChanged);

        // Sync the terminal size in case geometry was already set before the
        // session was assigned (e.g. session assigned after initial layout).
        updateTerminalSize();
    }

    Q_EMIT sessionChanged();
    update();
}

// ---------------------------------------------------------------------------
// Scroll offset property (delegates to session)
// ---------------------------------------------------------------------------

int QTermQuickItem::scrollOffset() const
{
    return m_session ? m_session->scrollOffset() : 0;
}

void QTermQuickItem::setScrollOffset(int offset)
{
    if (m_session)
        m_session->setScrollOffset(offset);
}

int QTermQuickItem::maxScrollOffset() const
{
    return m_session ? m_session->maxScrollOffset() : 0;
}

// ---------------------------------------------------------------------------
// paint()
// ---------------------------------------------------------------------------

void QTermQuickItem::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), m_defaultBg);

    if (!m_session)
        return;

    const int rows = m_session->terminalRows();
    const int cols = m_session->terminalCols();

    VTermScreenCell cell;

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            if (!m_session->cellAt(row, col, &cell))
                continue;

            // Skip the second (right) half of a wide character
            if (cell.width == 0)
                continue;

            const int cellW = static_cast<int>(m_cellWidth)  * cell.width;
            const int cellH = static_cast<int>(m_cellHeight);
            const int x     = static_cast<int>(col * m_cellWidth);
            const int y     = static_cast<int>(row * m_cellHeight);

            // --- Resolve colours ---
            QColor fg = vtermColorToQt(cell.fg, true);
            QColor bg = vtermColorToQt(cell.bg, false);

            if (cell.attrs.reverse)
                qSwap(fg, bg);

            // --- Background ---
            painter->fillRect(x, y, cellW, cellH, bg);

            // --- Text ---
            if (cell.chars[0] != 0 && cell.chars[0] != ' ') {
                QFont f = m_font;
                f.setBold(cell.attrs.bold);
                f.setItalic(cell.attrs.italic);
                if (cell.attrs.underline)
                    f.setUnderline(true);
                if (cell.attrs.strike)
                    f.setStrikeOut(true);

                painter->setFont(f);
                painter->setPen(fg);

                // Build a QString from the UCS-4 codepoints
                QString text;
                for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i]; ++i) {
                    const char32_t cp = static_cast<char32_t>(cell.chars[i]);
                    text += QString::fromUcs4(&cp, 1);
                }

                painter->drawText(x, static_cast<int>(y + m_cellAscent), text);
            }
        }
    }

    // --- Cursor ---
    if (m_hasFocus && m_session->isCursorVisible()) {
        const VTermPos cur = m_session->cursorPos();

        // Only draw cursor if it's in the live screen area (not scrollback)
        if (m_session->scrollOffset() == 0
            && cur.row >= 0 && cur.row < rows
            && cur.col >= 0 && cur.col < cols) {

            const int x = static_cast<int>(cur.col * m_cellWidth);
            const int y = static_cast<int>(cur.row * m_cellHeight);

            // Block cursor – draw as inverted rectangle
            painter->setCompositionMode(QPainter::RasterOp_SourceXorDestination);
            painter->fillRect(x, y,
                              static_cast<int>(m_cellWidth),
                              static_cast<int>(m_cellHeight),
                              Qt::white);
            painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
    }
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

void QTermQuickItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        updateTerminalSize();
}

// ---------------------------------------------------------------------------
// Key events
// ---------------------------------------------------------------------------

void QTermQuickItem::keyPressEvent(QKeyEvent *event)
{
    if (!m_session) {
        event->ignore();
        return;
    }

    event->accept();

    // Build modifier flags
    VTermModifier mod = VTERM_MOD_NONE;
    const Qt::KeyboardModifiers qmods = event->modifiers();
    if (qmods & Qt::ShiftModifier)   mod = static_cast<VTermModifier>(mod | VTERM_MOD_SHIFT);
    if (qmods & Qt::AltModifier)     mod = static_cast<VTermModifier>(mod | VTERM_MOD_ALT);
    if (qmods & Qt::ControlModifier) mod = static_cast<VTermModifier>(mod | VTERM_MOD_CTRL);

    // Try special key first
    VTermModifier vmod = mod;
    VTermKey vkey = qtKeyToVTerm(event->key(), qmods, &vmod);

    if (vkey != VTERM_KEY_NONE) {
        m_session->sendKey(vkey, vmod);
        return;
    }

    // Fall back to Unicode character
    const QString text = event->text();
    if (!text.isEmpty()) {
        const QList<uint> ucs4 = text.toUcs4();
        for (uint cp : ucs4)
            m_session->sendChar(cp, mod);
    }
}

// ---------------------------------------------------------------------------
// Mouse events
// ---------------------------------------------------------------------------

void QTermQuickItem::mousePressEvent(QMouseEvent *event)
{
    forceActiveFocus(Qt::MouseFocusReason);

    if (!m_session) {
        event->ignore();
        return;
    }

    event->accept();
    m_mousePressed = true;

    int row, col;
    pixelToCell(event->position(), &row, &col);

    VTermModifier mod = VTERM_MOD_NONE;
    if (event->modifiers() & Qt::ShiftModifier)   mod = static_cast<VTermModifier>(mod | VTERM_MOD_SHIFT);
    if (event->modifiers() & Qt::AltModifier)     mod = static_cast<VTermModifier>(mod | VTERM_MOD_ALT);
    if (event->modifiers() & Qt::ControlModifier) mod = static_cast<VTermModifier>(mod | VTERM_MOD_CTRL);

    // libvterm buttons: 1=left, 2=middle, 3=right
    int btn = 1;
    if (event->button() == Qt::MiddleButton) btn = 2;
    else if (event->button() == Qt::RightButton) btn = 3;

    m_session->sendMouseButton(btn, true, mod);
}

void QTermQuickItem::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_session) {
        event->ignore();
        return;
    }

    event->accept();

    int row, col;
    pixelToCell(event->position(), &row, &col);

    VTermModifier mod = VTERM_MOD_NONE;
    if (event->modifiers() & Qt::ShiftModifier)   mod = static_cast<VTermModifier>(mod | VTERM_MOD_SHIFT);
    if (event->modifiers() & Qt::AltModifier)     mod = static_cast<VTermModifier>(mod | VTERM_MOD_ALT);
    if (event->modifiers() & Qt::ControlModifier) mod = static_cast<VTermModifier>(mod | VTERM_MOD_CTRL);

    m_session->sendMouseMove(row, col, mod);
}

void QTermQuickItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_session) {
        event->ignore();
        return;
    }

    event->accept();
    m_mousePressed = false;

    int row, col;
    pixelToCell(event->position(), &row, &col);

    VTermModifier mod = VTERM_MOD_NONE;
    if (event->modifiers() & Qt::ShiftModifier)   mod = static_cast<VTermModifier>(mod | VTERM_MOD_SHIFT);
    if (event->modifiers() & Qt::AltModifier)     mod = static_cast<VTermModifier>(mod | VTERM_MOD_ALT);
    if (event->modifiers() & Qt::ControlModifier) mod = static_cast<VTermModifier>(mod | VTERM_MOD_CTRL);

    int btn = 1;
    if (event->button() == Qt::MiddleButton) btn = 2;
    else if (event->button() == Qt::RightButton) btn = 3;

    m_session->sendMouseButton(btn, false, mod);
}

void QTermQuickItem::wheelEvent(QWheelEvent *event)
{
    if (!m_session) {
        event->ignore();
        return;
    }

    event->accept();

    // Scroll 3 lines per notch (120 units = 1 standard notch)
    const int delta = event->angleDelta().y();
    const int lines = -(delta / 40); // positive delta = scroll up (into scrollback)

    if (lines != 0)
        m_session->scrollBy(lines);
}

// ---------------------------------------------------------------------------
// Focus
// ---------------------------------------------------------------------------

void QTermQuickItem::focusInEvent(QFocusEvent *event)
{
    QQuickPaintedItem::focusInEvent(event);
    m_hasFocus = true;
    update();
}

void QTermQuickItem::focusOutEvent(QFocusEvent *event)
{
    QQuickPaintedItem::focusOutEvent(event);
    m_hasFocus = false;
    update();
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void QTermQuickItem::onDisplayChanged(int /*startRow*/, int /*endRow*/)
{
    // Phase 1: repaint the whole item on any change.
    // Future: limit repaint to the damaged rect.
    update();
}

void QTermQuickItem::onSessionScrollOffsetChanged(int /*offset*/)
{
    Q_EMIT scrollOffsetChanged();
    Q_EMIT maxScrollOffsetChanged();
    update();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void QTermQuickItem::updateCellMetrics()
{
    QFontMetricsF fm(m_font);
    // Use the advance width of '0' (representative monospace character)
    m_cellWidth  = fm.horizontalAdvance(QLatin1Char('0'));
    m_cellHeight = fm.height();
    m_cellAscent = fm.ascent();
}

void QTermQuickItem::updateTerminalSize()
{
    if (!m_session || m_cellWidth <= 0 || m_cellHeight <= 0)
        return;

    const int cols = qMax(1, static_cast<int>(width()  / m_cellWidth));
    const int rows = qMax(1, static_cast<int>(height() / m_cellHeight));

    m_session->resizeTerminal(rows, cols);
}

void QTermQuickItem::pixelToCell(const QPointF &pos, int *row, int *col) const
{
    *row = qBound(0, static_cast<int>(pos.y() / m_cellHeight),
                  m_session ? m_session->terminalRows() - 1 : 0);
    *col = qBound(0, static_cast<int>(pos.x() / m_cellWidth),
                  m_session ? m_session->terminalCols() - 1 : 0);
}

QColor QTermQuickItem::vtermColorToQt(const VTermColor &color, bool isFg) const
{
    if (VTERM_COLOR_IS_DEFAULT_FG(&color))
        return m_defaultFg;
    if (VTERM_COLOR_IS_DEFAULT_BG(&color))
        return m_defaultBg;

    if (VTERM_COLOR_IS_RGB(&color))
        return QColor(color.rgb.red, color.rgb.green, color.rgb.blue);

    // Indexed colour – use the standard xterm 256-colour palette
    // For phase 1, resolve the 16 basic ANSI colours inline; the rest are RGB.
    static const QColor ansi16[16] = {
        {  0,   0,   0}, // 0  black
        {170,   0,   0}, // 1  red
        {  0, 170,   0}, // 2  green
        {170, 170,   0}, // 3  yellow
        {  0,   0, 170}, // 4  blue
        {170,   0, 170}, // 5  magenta
        {  0, 170, 170}, // 6  cyan
        {170, 170, 170}, // 7  white
        { 85,  85,  85}, // 8  bright black (grey)
        {255,  85,  85}, // 9  bright red
        { 85, 255,  85}, // 10 bright green
        {255, 255,  85}, // 11 bright yellow
        { 85,  85, 255}, // 12 bright blue
        {255,  85, 255}, // 13 bright magenta
        { 85, 255, 255}, // 14 bright cyan
        {255, 255, 255}, // 15 bright white
    };

    const int idx = color.indexed.idx;

    if (idx < 16)
        return ansi16[idx];

    if (idx < 232) {
        // 6×6×6 colour cube
        const int i = idx - 16;
        const int b = i % 6;
        const int g = (i / 6) % 6;
        const int r = i / 36;
        auto scale = [](int v) { return v == 0 ? 0 : 55 + v * 40; };
        return QColor(scale(r), scale(g), scale(b));
    }

    // Grayscale ramp 232–255
    const int grey = 8 + (idx - 232) * 10;
    return QColor(grey, grey, grey);
}

// ---------------------------------------------------------------------------
// Key translation
// ---------------------------------------------------------------------------

VTermKey QTermQuickItem::qtKeyToVTerm(int qtKey, Qt::KeyboardModifiers /*mods*/,
                                      VTermModifier * /*outMod*/)
{
    switch (qtKey) {
    case Qt::Key_Return:
    case Qt::Key_Enter:      return VTERM_KEY_ENTER;
    case Qt::Key_Tab:        return VTERM_KEY_TAB;
    case Qt::Key_Backspace:  return VTERM_KEY_BACKSPACE;
    case Qt::Key_Escape:     return VTERM_KEY_ESCAPE;
    case Qt::Key_Up:         return VTERM_KEY_UP;
    case Qt::Key_Down:       return VTERM_KEY_DOWN;
    case Qt::Key_Left:       return VTERM_KEY_LEFT;
    case Qt::Key_Right:      return VTERM_KEY_RIGHT;
    case Qt::Key_Home:       return VTERM_KEY_HOME;
    case Qt::Key_End:        return VTERM_KEY_END;
    case Qt::Key_PageUp:     return VTERM_KEY_PAGEUP;
    case Qt::Key_PageDown:   return VTERM_KEY_PAGEDOWN;
    case Qt::Key_Insert:     return VTERM_KEY_INS;
    case Qt::Key_Delete:     return VTERM_KEY_DEL;
    case Qt::Key_F1:         return static_cast<VTermKey>(VTERM_KEY_FUNCTION(1));
    case Qt::Key_F2:         return static_cast<VTermKey>(VTERM_KEY_FUNCTION(2));
    case Qt::Key_F3:         return static_cast<VTermKey>(VTERM_KEY_FUNCTION(3));
    case Qt::Key_F4:         return static_cast<VTermKey>(VTERM_KEY_FUNCTION(4));
    case Qt::Key_F5:         return static_cast<VTermKey>(VTERM_KEY_FUNCTION(5));
    case Qt::Key_F6:         return static_cast<VTermKey>(VTERM_KEY_FUNCTION(6));
    case Qt::Key_F7:         return static_cast<VTermKey>(VTERM_KEY_FUNCTION(7));
    case Qt::Key_F8:         return static_cast<VTermKey>(VTERM_KEY_FUNCTION(8));
    case Qt::Key_F9:         return static_cast<VTermKey>(VTERM_KEY_FUNCTION(9));
    case Qt::Key_F10:        return static_cast<VTermKey>(VTERM_KEY_FUNCTION(10));
    case Qt::Key_F11:        return static_cast<VTermKey>(VTERM_KEY_FUNCTION(11));
    case Qt::Key_F12:        return static_cast<VTermKey>(VTERM_KEY_FUNCTION(12));
    default:                 return VTERM_KEY_NONE;
    }
}
