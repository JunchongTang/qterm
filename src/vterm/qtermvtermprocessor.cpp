#include "qtermvtermprocessor.h"

#include <QDebug>
#include <QString>

// ---------------------------------------------------------------------------
// Screen callback table (static, shared by all instances)
// ---------------------------------------------------------------------------
static const VTermScreenCallbacks kScreenCallbacks = {
    QTermVTermProcessor::cb_damage,      // damage
    QTermVTermProcessor::cb_moverect,    // moverect
    QTermVTermProcessor::cb_movecursor,  // movecursor
    QTermVTermProcessor::cb_settermprop, // settermprop
    QTermVTermProcessor::cb_bell,        // bell
    QTermVTermProcessor::cb_resize,      // resize
    QTermVTermProcessor::cb_sb_pushline, // sb_pushline
    QTermVTermProcessor::cb_sb_popline,  // sb_popline
    QTermVTermProcessor::cb_sb_clear,    // sb_clear
    nullptr,                             // sb_pushline4 (ABI-compat, unused)
    nullptr,                             // sb_popline4  (ABI-compat, unused)
};

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

QTermVTermProcessor::QTermVTermProcessor(int rows, int cols, QObject *parent)
    : QObject(parent)
    , m_rows(rows)
    , m_cols(cols)
{
    m_vt = vterm_new(rows, cols);
    vterm_set_utf8(m_vt, 1);

    // Route libvterm output (keyboard/mouse responses, DA replies …) to us
    vterm_output_set_callback(m_vt, cb_output, this);

    m_screen = vterm_obtain_screen(m_vt);
    vterm_screen_set_callbacks(m_screen, &kScreenCallbacks, this);

    // Merge damage at row granularity – sufficient for a full repaint pass
    vterm_screen_set_damage_merge(m_screen, VTERM_DAMAGE_ROW);

    vterm_screen_enable_altscreen(m_screen, 1);
    // Reflow is intentionally disabled: on resize the PTY sends SIGWINCH and
    // the shell redraws its own content. Enabling reflow causes libvterm to
    // rewrap existing cells AND the shell to redraw simultaneously, producing
    // overlapping / garbled output.
    vterm_screen_enable_reflow(m_screen, false);
    vterm_screen_reset(m_screen, 1);

    // Default palette: near-white on dark background
    VTermColor fg, bg;
    vterm_color_rgb(&fg, 0xe0, 0xe0, 0xe0);
    vterm_color_rgb(&bg, 0x1e, 0x1e, 0x1e);
    vterm_screen_set_default_colors(m_screen, &fg, &bg);
}

QTermVTermProcessor::~QTermVTermProcessor()
{
    if (m_vt)
        vterm_free(m_vt);
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void QTermVTermProcessor::feedData(const QByteArray &data)
{
    vterm_input_write(m_vt, data.constData(), static_cast<size_t>(data.size()));
    vterm_screen_flush_damage(m_screen);
    flushDamage();
}

void QTermVTermProcessor::resize(int rows, int cols)
{
    m_rows = rows;
    m_cols = cols;
    vterm_set_size(m_vt, rows, cols);
    vterm_screen_flush_damage(m_screen);
    flushDamage();
}

void QTermVTermProcessor::sendKey(VTermKey key, VTermModifier mod)
{
    vterm_keyboard_key(m_vt, key, mod);
    // outputData is emitted synchronously from cb_output during the call above
}

void QTermVTermProcessor::sendChar(uint32_t unichar, VTermModifier mod)
{
    vterm_keyboard_unichar(m_vt, unichar, mod);
}

void QTermVTermProcessor::startPaste()
{
    vterm_keyboard_start_paste(m_vt);
}

void QTermVTermProcessor::endPaste()
{
    vterm_keyboard_end_paste(m_vt);
}

void QTermVTermProcessor::sendMouseMove(int row, int col, VTermModifier mod)
{
    vterm_mouse_move(m_vt, row, col, mod);
}

void QTermVTermProcessor::sendMouseButton(int button, bool pressed, VTermModifier mod)
{
    vterm_mouse_button(m_vt, button, pressed, mod);
}

bool QTermVTermProcessor::screenCellAt(int row, int col, VTermScreenCell *cell) const
{
    VTermPos pos{row, col};
    return vterm_screen_get_cell(m_screen, pos, cell) != 0;
}

const QVector<VTermScreenCell> &QTermVTermProcessor::scrollbackLine(int idx) const
{
    return m_scrollback.at(idx);
}

// ---------------------------------------------------------------------------
// libvterm screen callbacks
// ---------------------------------------------------------------------------

int QTermVTermProcessor::cb_damage(VTermRect rect, void *user)
{
    auto *self = static_cast<QTermVTermProcessor *>(user);

    if (self->m_damageStartRow < 0) {
        self->m_damageStartRow = rect.start_row;
        self->m_damageEndRow   = rect.end_row;
    } else {
        self->m_damageStartRow = qMin(self->m_damageStartRow, rect.start_row);
        self->m_damageEndRow   = qMax(self->m_damageEndRow,   rect.end_row);
    }
    return 1;
}

int QTermVTermProcessor::cb_moverect(VTermRect /*dest*/, VTermRect /*src*/, void * /*user*/)
{
    // Scrolling is handled by pushline/popline; nothing extra to do here.
    return 1;
}

int QTermVTermProcessor::cb_movecursor(VTermPos pos, VTermPos /*oldpos*/, int visible, void *user)
{
    auto *self = static_cast<QTermVTermProcessor *>(user);
    self->m_cursorPos     = pos;
    self->m_cursorVisible = (visible != 0);
    Q_EMIT self->cursorMoved(pos, visible != 0);
    return 1;
}

int QTermVTermProcessor::cb_settermprop(VTermProp prop, VTermValue *val, void *user)
{
    auto *self = static_cast<QTermVTermProcessor *>(user);

    switch (prop) {
    case VTERM_PROP_TITLE:
        // The title may arrive in multiple fragments; emit on final fragment.
        if (val->string.final) {
            Q_EMIT self->titleChanged(
                QString::fromUtf8(val->string.str, static_cast<int>(val->string.len)));
        }
        break;
    case VTERM_PROP_CURSORVISIBLE:
        self->m_cursorVisible = (val->boolean != 0);
        Q_EMIT self->cursorMoved(self->m_cursorPos, self->m_cursorVisible);
        break;
    default:
        break;
    }
    return 1;
}

int QTermVTermProcessor::cb_bell(void *user)
{
    Q_EMIT static_cast<QTermVTermProcessor *>(user)->bellRang();
    return 1;
}

int QTermVTermProcessor::cb_resize(int rows, int cols, void *user)
{
    auto *self = static_cast<QTermVTermProcessor *>(user);
    self->m_rows = rows;
    self->m_cols = cols;
    Q_EMIT self->resized(rows, cols);
    return 1;
}

int QTermVTermProcessor::cb_sb_pushline(int cols, const VTermScreenCell *cells, void *user)
{
    auto *self = static_cast<QTermVTermProcessor *>(user);

    QVector<VTermScreenCell> line(cols);
    for (int i = 0; i < cols; ++i)
        line[i] = cells[i];

    self->m_scrollback.prepend(std::move(line));

    // Enforce the scrollback limit
    while (self->m_scrollback.size() > kMaxScrollbackLines)
        self->m_scrollback.removeLast();

    return 1;
}

int QTermVTermProcessor::cb_sb_popline(int cols, VTermScreenCell *cells, void *user)
{
    auto *self = static_cast<QTermVTermProcessor *>(user);

    if (self->m_scrollback.isEmpty())
        return 0;

    const QVector<VTermScreenCell> &line = self->m_scrollback.first();
    int n = qMin(cols, static_cast<int>(line.size()));
    for (int i = 0; i < n; ++i)
        cells[i] = line[i];

    // Pad remaining columns with an empty cell (chars[0] == 0, not space)
    if (n < cols) {
        VTermScreenCell blank{};
        blank.chars[0] = 0;
        blank.width    = 1;
        for (int i = n; i < cols; ++i)
            cells[i] = blank;
    }

    self->m_scrollback.removeFirst();
    return 1;
}

int QTermVTermProcessor::cb_sb_clear(void *user)
{
    static_cast<QTermVTermProcessor *>(user)->m_scrollback.clear();
    return 1;
}

// ---------------------------------------------------------------------------
// libvterm output callback
// ---------------------------------------------------------------------------

void QTermVTermProcessor::cb_output(const char *s, size_t len, void *user)
{
    // Called synchronously from vterm_keyboard_*/vterm_mouse_* and when the
    // terminal generates a query response.  Emit so QTermSession can forward
    // the bytes to the PTY.
    Q_EMIT static_cast<QTermVTermProcessor *>(user)->outputData(
        QByteArray(s, static_cast<int>(len)));
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void QTermVTermProcessor::flushDamage()
{
    if (m_damageStartRow < 0)
        return;

    Q_EMIT screenUpdated(m_damageStartRow, m_damageEndRow);
    m_damageStartRow = -1;
    m_damageEndRow   = -1;
}
