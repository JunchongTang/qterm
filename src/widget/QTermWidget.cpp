#include <QTerm/QTermWidget.h>

#include <QTerm/QTermSurfaceModel.h>
#include <QTerm/QTermTerminal.h>

#include "../quick/QTermViewController.h"
#include "../QTermRenderUtils.h"

#include <QFocusEvent>
#include <QFont>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>
#include <QWheelEvent>

#include <limits>

namespace QTerm {

// ── Constructor ───────────────────────────────────────────────────────────────

QTermWidget::QTermWidget(QWidget *parent)
    : QWidget(parent)
    , m_controller(new QTermViewController(this))
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setAutoFillBackground(false);
    setMouseTracking(false); // updated dynamically by updateMouseAcceptance()

    m_repaintTimer = new QTimer(this);
    m_repaintTimer->setSingleShot(true);
    connect(m_repaintTimer, &QTimer::timeout, this, &QTermWidget::flushPendingUpdate);

    connect(m_controller, &QTermViewController::repaintNeeded, this, [this]() {
        m_dirtyRows.clear();
        scheduleUpdate();
    });
    connect(m_controller, &QTermViewController::contentRowsDirty, this, [this](QVector<int> rows) {
        const qreal cellH = m_controller->cellHeight();
        if (cellH <= 0.0) {
            m_dirtyRows.clear();
            scheduleUpdate();
            return;
        }
        for (int r : rows) {
            if (!m_dirtyRows.contains(r))
                m_dirtyRows.append(r);
        }
        qreal yMin = std::numeric_limits<qreal>::max();
        qreal yMax = 0.0;
        for (int r : std::as_const(m_dirtyRows)) {
            yMin = qMin(yMin, r * cellH);
            yMax = qMax(yMax, (r + 1) * cellH);
        }
        scheduleUpdate(QRect(0, int(yMin), width(), int(yMax - yMin)));
    });
    connect(m_controller, &QTermViewController::mouseAcceptanceChanged,
            this, &QTermWidget::updateMouseAcceptance);
    connect(m_controller, &QTermViewController::focusRequested, this, [this]() {
        setFocus(Qt::MouseFocusReason);
    });
    connect(m_controller, &QTermViewController::metricsChanged, this, [this]() {
        update();
        emit metricsChanged();
        updateGeometry(); // let parent layout re-query sizeHint
    });
    connect(m_controller, &QTermViewController::scrollChanged,
            this, &QTermWidget::scrollChanged);
    connect(m_controller, &QTermViewController::wheelScrolled,
            this, &QTermWidget::wheelScrolled);
    connect(m_controller, &QTermViewController::zoomRequested,
            this, &QTermWidget::zoomRequested);
    connect(m_controller, &QTermViewController::copyRequested,
            this, &QTermWidget::copyRequested);
    connect(m_controller, &QTermViewController::hyperlinkActivated,
            this, &QTermWidget::hyperlinkActivated);
    connect(m_controller, &QTermViewController::terminalChanged, this, [this]() {
        update();
        emit terminalChanged();
        emit scrollChanged();
    });
}

// ── Terminal binding ─────────────────────────────────────────────────────────

QTermTerminal *QTermWidget::terminal() const noexcept
{
    return m_controller->terminal();
}

void QTermWidget::setTerminal(QTermTerminal *terminal)
{
    m_controller->notifyGeometryChanged(width(), height());
    m_controller->setTerminal(terminal);
    update();
}

// ── Font ─────────────────────────────────────────────────────────────────────

QString QTermWidget::fontFamily() const
{
    return m_controller->fontFamily();
}

void QTermWidget::setFontFamily(const QString &family)
{
    if (m_controller->fontFamily() == family) return;
    m_controller->setFontFamily(family);
    update();
    emit fontChanged();
}

int QTermWidget::fontPixelSize() const noexcept
{
    return m_controller->fontPixelSize();
}

void QTermWidget::setFontPixelSize(int size)
{
    const int bounded = qMax(1, size);
    if (m_controller->fontPixelSize() == bounded) return;
    m_controller->setFontPixelSize(bounded);
    update();
    emit fontChanged();
}

qreal QTermWidget::cellWidth() const noexcept  { return m_controller->cellWidth(); }
qreal QTermWidget::cellHeight() const noexcept { return m_controller->cellHeight(); }

// ── Palette ──────────────────────────────────────────────────────────────────

QColor QTermWidget::foregroundColor() const { return m_foregroundColor; }
void QTermWidget::setForegroundColor(const QColor &c)
{
    if (m_foregroundColor == c) return;
    m_foregroundColor = c; update(); emit paletteChanged();
}

QColor QTermWidget::backgroundColor() const { return m_backgroundColor; }

QColor QTermWidget::inverseTextColor() const { return m_inverseTextColor; }

void QTermWidget::setInverseTextColor(const QColor &inverseTextColor)
{
    if (m_inverseTextColor == inverseTextColor) return;
    m_inverseTextColor = inverseTextColor;
    update();
    emit paletteChanged();
}
void QTermWidget::setBackgroundColor(const QColor &c)
{
    if (m_backgroundColor == c) return;
    m_backgroundColor = c; update(); emit paletteChanged();
}

QColor QTermWidget::selectionColor() const { return m_selectionColor; }
void QTermWidget::setSelectionColor(const QColor &c)
{
    if (m_selectionColor == c) return;
    m_selectionColor = c; update(); emit paletteChanged();
}

QColor QTermWidget::searchHighlightColor() const { return m_searchHighlightColor; }
void QTermWidget::setSearchHighlightColor(const QColor &c)
{
    if (m_searchHighlightColor == c) return;
    m_searchHighlightColor = c; update(); emit paletteChanged();
}

QColor QTermWidget::searchCurrentColor() const { return m_searchCurrentColor; }
void QTermWidget::setSearchCurrentColor(const QColor &c)
{
    if (m_searchCurrentColor == c) return;
    m_searchCurrentColor = c; update(); emit paletteChanged();
}

QColor QTermWidget::cursorColor() const { return m_cursorColor; }
void QTermWidget::setCursorColor(const QColor &c)
{
    if (m_cursorColor == c) return;
    m_cursorColor = c; update(); emit paletteChanged();
}

qreal QTermWidget::cursorOpacity() const noexcept { return m_cursorOpacity; }
void QTermWidget::setCursorOpacity(qreal opacity)
{
    const qreal bounded = qBound(0.0, opacity, 1.0);
    if (qFuzzyCompare(m_cursorOpacity, bounded)) return;
    m_cursorOpacity = bounded; update(); emit cursorOpacityChanged();
}

QTermWidget::CursorStyle QTermWidget::cursorStyle() const noexcept { return m_cursorStyle; }
void QTermWidget::setCursorStyle(CursorStyle style)
{
    if (m_cursorStyle == style) return;
    m_cursorStyle = style; update(); emit cursorStyleChanged();
}

// ── Scrolling ────────────────────────────────────────────────────────────────

qreal QTermWidget::scrollPosition() const noexcept { return m_controller->scrollPosition(); }
qreal QTermWidget::scrollSize() const noexcept     { return m_controller->scrollSize(); }
void QTermWidget::setScrollPosition(qreal pos)     { m_controller->setScrollPosition(pos); }

// ── Coordinate helpers ───────────────────────────────────────────────────────

int QTermWidget::rowAtPosition(qreal y) const    { return m_controller->rowAtPosition(y); }
int QTermWidget::columnAtPosition(qreal x) const { return m_controller->columnAtPosition(x); }

// ── IME ───────────────────────────────────────────────────────────────────────

QVariant QTermWidget::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImEnabled:
        return true;
    case Qt::ImCursorRectangle:
        return m_controller->cursorRect().toRect();
    default:
        return QWidget::inputMethodQuery(query);
    }
}

// ── Theme ─────────────────────────────────────────────────────────────────────

QTermTheme QTermWidget::theme() const { return m_theme; }

void QTermWidget::setTheme(const QTermTheme &t)
{
    m_theme           = t;
    m_foregroundColor = t.foreground();
    m_backgroundColor = t.background();
    m_selectionColor  = t.selection();
    m_cursorColor     = t.cursor();
    update();
    emit themeChanged();
    emit paletteChanged();
}

// ── sizeHint ──────────────────────────────────────────────────────────────────

QSize QTermWidget::sizeHint() const
{
    // Default hint: 80×24 terminal
    return QSize(static_cast<int>(80 * m_controller->cellWidth()),
                 static_cast<int>(24 * m_controller->cellHeight()));
}

// ── Repaint coalescing ────────────────────────────────────────────────────────

int QTermWidget::frameIntervalMs() const
{
    // Following the display means a fast panel still gets its extra frames and
    // a slow one is not asked for frames it cannot show.
    const qreal refreshRate = screen() ? screen()->refreshRate() : 60.0;
    const int interval = refreshRate > 1.0 ? int(1000.0 / refreshRate) : 16;
    return qBound(4, interval, 32);
}

void QTermWidget::scheduleUpdate(const QRect &rect)
{
    if (rect.isNull())
        m_pendingFull = true;
    else if (!m_pendingFull)
        m_pendingRect = m_pendingRect.united(rect);

    // Already waiting on the frame deadline: the region above is enough, the
    // timer will pick it up.
    if (m_repaintTimer->isActive())
        return;

    const int interval = frameIntervalMs();
    const qint64 sinceLastPaint = m_sinceLastPaint.isValid() ? m_sinceLastPaint.elapsed()
                                                             : interval;
    if (sinceLastPaint >= interval)
        flushPendingUpdate();          // idle until now: paint straight away
    else
        m_repaintTimer->start(int(interval - sinceLastPaint));
}

void QTermWidget::flushPendingUpdate()
{
    if (m_pendingFull)
        update();
    else if (!m_pendingRect.isNull())
        update(m_pendingRect);

    m_pendingFull = false;
    m_pendingRect = QRect();
}

// ── paintEvent ────────────────────────────────────────────────────────────────

void QTermWidget::paintEvent(QPaintEvent *event)
{
    QTermTerminal *terminal = m_controller->terminal();
    QTermSurfaceModel *surfaceModel = terminal ? terminal->surfaceModel() : nullptr;

    QFont baseFont(m_controller->fontFamily());
    baseFont.setPixelSize(m_controller->fontPixelSize());

    const QRectF fullBounds(rect());
    const QRectF eventRect(event->rect());
    const bool isPartial = eventRect.isValid() && eventRect != fullBounds;

    QTermPaintRequest req;
    req.bounds        = fullBounds;
    req.clipBounds    = isPartial ? eventRect : QRectF();
    req.surfaceModel  = surfaceModel;
    req.cellWidth     = m_controller->cellWidth();
    req.cellHeight    = m_controller->cellHeight();
    req.baseFont      = baseFont;
    req.foreground    = m_foregroundColor;
    req.background    = m_backgroundColor;
    req.inverseText   = m_inverseTextColor;
    req.selection     = m_selectionColor;
    req.searchHighlight = m_searchHighlightColor;
    req.searchCurrent = m_searchCurrentColor;
    req.cursor        = m_cursorColor;
    req.cursorOpacity = m_cursorOpacity;
    req.cursorStyle   = static_cast<int>(m_cursorStyle);
    req.showCursor    = hasFocus();
    req.hyperlinkTint = m_theme.hyperlinkTint();
    req.palette16     = m_theme.palette16();

    QPainter painter(this);
    qtermPaintTerminal(&painter, req);

    m_dirtyRows.clear();
    // Anchors the next frame deadline; scheduleUpdate() measures from here.
    m_sinceLastPaint.restart();
}

// ── resizeEvent ───────────────────────────────────────────────────────────────

void QTermWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_controller->notifyGeometryChanged(event->size().width(), event->size().height());
}

// ── Event forwarding ──────────────────────────────────────────────────────────

void QTermWidget::keyPressEvent(QKeyEvent *event)
{
    if (!m_controller->handleKeyPress(event))
        QWidget::keyPressEvent(event);
}

void QTermWidget::inputMethodEvent(QInputMethodEvent *event)
{
    if (!m_controller->handleInputMethod(event))
        QWidget::inputMethodEvent(event);
}

void QTermWidget::mousePressEvent(QMouseEvent *event)
{
    if (!m_controller->handleMousePress(event))
        QWidget::mousePressEvent(event);
}

void QTermWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_controller->handleMouseDoubleClick(event))
        QWidget::mouseDoubleClickEvent(event);
}

void QTermWidget::mouseMoveEvent(QMouseEvent *event)
{
    // In QWidget, no-button mouse moves (mouse tracking) come here too.
    // handleMouseMove already handles the AnyEvent protocol path for no-button moves.
    if (!m_controller->handleMouseMove(event))
        QWidget::mouseMoveEvent(event);
}

void QTermWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_controller->handleMouseRelease(event))
        QWidget::mouseReleaseEvent(event);
}

void QTermWidget::wheelEvent(QWheelEvent *event)
{
    if (!m_controller->handleWheel(event))
        QWidget::wheelEvent(event);
}

void QTermWidget::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    update(); // redraw cursor
}

void QTermWidget::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    update(); // hide cursor
}

// ── updateMouseAcceptance ─────────────────────────────────────────────────────

void QTermWidget::updateMouseAcceptance()
{
    // In QWidget, all mouse buttons are always delivered; no per-button filter.
    // Only mouse tracking (no-button moves) needs to be toggled.
    setMouseTracking(m_controller->hoverEventsNeeded());
}

} // namespace QTerm
