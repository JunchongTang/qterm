#include <QTerm/QTermQuickPaintedItem.h>

#include <QTerm/QTermSurfaceModel.h>
#include <QTerm/QTermTerminal.h>

#include "QTermViewController.h"
#include "../QTermRenderUtils.h"

#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTimer>
#include <QWheelEvent>

namespace QTerm {

// ── Constructor ───────────────────────────────────────────────────────────────

QTermQuickPaintedItem::QTermQuickPaintedItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
    , m_controller(new QTermViewController(this))
{
    setOpaquePainting(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setFlag(QQuickItem::ItemAcceptsInputMethod, true);
    setAcceptHoverEvents(false);

    connect(this, &QQuickItem::activeFocusChanged, this, [this]() {
        update();
        updateCursorDelegateGeometry();
    });

    connect(m_controller, &QTermViewController::repaintNeeded, this, [this]() {
        update();
    });
    connect(m_controller, &QTermViewController::cursorUpdateNeeded, this, [this]() {
        updateCursorDelegateGeometry();
    });
    connect(m_controller, &QTermViewController::mouseAcceptanceChanged,
            this, &QTermQuickPaintedItem::updateMouseAcceptance);
    connect(m_controller, &QTermViewController::focusRequested, this, [this]() {
        forceActiveFocus(Qt::MouseFocusReason);
    });
    connect(m_controller, &QTermViewController::metricsChanged, this, [this]() {
        update();
        emit metricsChanged();
    });
    connect(m_controller, &QTermViewController::scrollChanged,
            this, &QTermQuickPaintedItem::scrollChanged);
    connect(m_controller, &QTermViewController::wheelScrolled,
            this, &QTermQuickPaintedItem::wheelScrolled);
    connect(m_controller, &QTermViewController::copyRequested,
            this, &QTermQuickPaintedItem::copyRequested);
    connect(m_controller, &QTermViewController::hyperlinkActivated,
            this, &QTermQuickPaintedItem::hyperlinkActivated);
    connect(m_controller, &QTermViewController::terminalChanged, this, [this]() {
        update();
        emit terminalChanged();
        emit scrollChanged();
    });
}

// ── Terminal 绑定 ─────────────────────────────────────────────────────────────

QTermTerminal *QTermQuickPaintedItem::terminal() const noexcept
{
    return m_controller->terminal();
}

void QTermQuickPaintedItem::setTerminal(QTermTerminal *terminal)
{
    // Geometry is already stored in controller; pass current size so
    // syncTerminalSize() runs correctly right after setTerminal.
    m_controller->notifyGeometryChanged(width(), height());
    m_controller->setTerminal(terminal);
    update();
}

// ── 字体 ──────────────────────────────────────────────────────────────────────

QString QTermQuickPaintedItem::fontFamily() const
{
    return m_controller->fontFamily();
}

void QTermQuickPaintedItem::setFontFamily(const QString &fontFamily)
{
    if (m_controller->fontFamily() == fontFamily)
        return;
    m_controller->setFontFamily(fontFamily);
    update();
    emit fontChanged();
}

int QTermQuickPaintedItem::fontPixelSize() const noexcept
{
    return m_controller->fontPixelSize();
}

void QTermQuickPaintedItem::setFontPixelSize(int fontPixelSize)
{
    const int bounded = qMax(1, fontPixelSize);
    if (m_controller->fontPixelSize() == bounded)
        return;
    m_controller->setFontPixelSize(bounded);
    update();
    emit fontChanged();
}

qreal QTermQuickPaintedItem::cellWidth() const noexcept
{
    return m_controller->cellWidth();
}

qreal QTermQuickPaintedItem::cellHeight() const noexcept
{
    return m_controller->cellHeight();
}

// ── 调色板 ────────────────────────────────────────────────────────────────────

QColor QTermQuickPaintedItem::foregroundColor() const { return m_foregroundColor; }

void QTermQuickPaintedItem::setForegroundColor(const QColor &foregroundColor)
{
    if (m_foregroundColor == foregroundColor) return;
    m_foregroundColor = foregroundColor;
    update();
    emit paletteChanged();
}

QColor QTermQuickPaintedItem::backgroundColor() const { return m_backgroundColor; }

void QTermQuickPaintedItem::setBackgroundColor(const QColor &backgroundColor)
{
    if (m_backgroundColor == backgroundColor) return;
    m_backgroundColor = backgroundColor;
    update();
    emit paletteChanged();
}

QColor QTermQuickPaintedItem::selectionColor() const { return m_selectionColor; }

void QTermQuickPaintedItem::setSelectionColor(const QColor &selectionColor)
{
    if (m_selectionColor == selectionColor) return;
    m_selectionColor = selectionColor;
    update();
    emit paletteChanged();
}

QColor QTermQuickPaintedItem::cursorColor() const { return m_cursorColor; }

void QTermQuickPaintedItem::setCursorColor(const QColor &cursorColor)
{
    if (m_cursorColor == cursorColor) return;
    m_cursorColor = cursorColor;
    update();
    emit paletteChanged();
}

qreal QTermQuickPaintedItem::cursorOpacity() const noexcept { return m_cursorOpacity; }

void QTermQuickPaintedItem::setCursorOpacity(qreal cursorOpacity)
{
    const qreal bounded = qBound(0.0, cursorOpacity, 1.0);
    if (qFuzzyCompare(m_cursorOpacity, bounded)) return;
    m_cursorOpacity = bounded;
    update();
    updateCursorDelegateGeometry();
    emit cursorOpacityChanged();
}

// ── 坐标辅助 ──────────────────────────────────────────────────────────────────

int QTermQuickPaintedItem::rowAtPosition(qreal y) const
{
    return m_controller->rowAtPosition(y);
}

int QTermQuickPaintedItem::columnAtPosition(qreal x) const
{
    return m_controller->columnAtPosition(x);
}

// ── 滚动 ──────────────────────────────────────────────────────────────────────

qreal QTermQuickPaintedItem::scrollSize() const noexcept
{
    return m_controller->scrollSize();
}

qreal QTermQuickPaintedItem::scrollPosition() const noexcept
{
    return m_controller->scrollPosition();
}

void QTermQuickPaintedItem::setScrollPosition(qreal position)
{
    m_controller->setScrollPosition(position);
}

// ── IME ───────────────────────────────────────────────────────────────────────

QVariant QTermQuickPaintedItem::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImEnabled:
        return true;
    case Qt::ImCursorRectangle:
        return m_controller->cursorRect();
    default:
        return QQuickPaintedItem::inputMethodQuery(query);
    }
}

// ── geometryChange ────────────────────────────────────────────────────────────

void QTermQuickPaintedItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        m_controller->notifyGeometryChanged(newGeometry.width(), newGeometry.height());
}

// ── Event forwarding ──────────────────────────────────────────────────────────

void QTermQuickPaintedItem::keyPressEvent(QKeyEvent *event)
{
    if (m_controller->handleKeyPress(event)) event->accept();
    else QQuickPaintedItem::keyPressEvent(event);
}

void QTermQuickPaintedItem::inputMethodEvent(QInputMethodEvent *event)
{
    if (m_controller->handleInputMethod(event)) event->accept();
    else QQuickPaintedItem::inputMethodEvent(event);
}

void QTermQuickPaintedItem::mousePressEvent(QMouseEvent *event)
{
    if (m_controller->handleMousePress(event)) event->accept();
    else QQuickPaintedItem::mousePressEvent(event);
}

void QTermQuickPaintedItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_controller->handleMouseDoubleClick(event)) event->accept();
    else QQuickPaintedItem::mouseDoubleClickEvent(event);
}

void QTermQuickPaintedItem::mouseMoveEvent(QMouseEvent *event)
{
    if (m_controller->handleMouseMove(event)) event->accept();
    else QQuickPaintedItem::mouseMoveEvent(event);
}

void QTermQuickPaintedItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_controller->handleMouseRelease(event)) event->accept();
    else QQuickPaintedItem::mouseReleaseEvent(event);
}

void QTermQuickPaintedItem::hoverMoveEvent(QHoverEvent *event)
{
    if (m_controller->handleHoverMove(event)) event->accept();
    else QQuickPaintedItem::hoverMoveEvent(event);
}

void QTermQuickPaintedItem::wheelEvent(QWheelEvent *event)
{
    if (m_controller->handleWheel(event)) event->accept();
    else QQuickPaintedItem::wheelEvent(event);
}

// ── updateMouseAcceptance ─────────────────────────────────────────────────────

void QTermQuickPaintedItem::updateMouseAcceptance()
{
    if (m_controller->mouseProtocolEnabled()) {
        setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton | Qt::MiddleButton);
        setAcceptHoverEvents(m_controller->hoverEventsNeeded());
    } else {
        setAcceptedMouseButtons(Qt::LeftButton);
        setAcceptHoverEvents(false);
    }
}

// ── paint() ───────────────────────────────────────────────────────────────────

void QTermQuickPaintedItem::paint(QPainter *painter)
{
    QTermTerminal *terminal = m_controller->terminal();
    QTermSurfaceModel *surfaceModel = terminal ? terminal->surfaceModel() : nullptr;

    QFont baseFont(m_controller->fontFamily());
    baseFont.setPixelSize(m_controller->fontPixelSize());

    QTermPaintRequest req;
    req.bounds        = boundingRect();
    req.surfaceModel  = surfaceModel;
    req.cellWidth     = m_controller->cellWidth();
    req.cellHeight    = m_controller->cellHeight();
    req.baseFont      = baseFont;
    req.foreground    = m_foregroundColor;
    req.background    = m_backgroundColor;
    req.selection     = m_selectionColor;
    req.cursor        = m_cursorColor;
    req.cursorOpacity = m_cursorOpacity;
    // Cursor shape comes from the terminal's surface model (DECSCUSR) when available,
    // falling back to the QML-set cursorStyle property.
    req.cursorStyle   = surfaceModel
        ? surfaceModel->cursorShape()
        : static_cast<int>(m_cursorStyle);
    req.showCursor    = !m_cursorDelegateItem && hasActiveFocus();
    req.hyperlinkTint = m_theme.hyperlinkTint();
    req.palette16     = m_theme.palette16();

    qtermPaintTerminal(painter, req);
}

// ── componentComplete ─────────────────────────────────────────────────────────

void QTermQuickPaintedItem::componentComplete()
{
    QQuickPaintedItem::componentComplete();
    if (m_cursorDelegate && !m_cursorDelegateItem)
        recreateCursorDelegateItem();
}

// ── CursorStyle ───────────────────────────────────────────────────────────────

QTermQuickPaintedItem::CursorStyle QTermQuickPaintedItem::cursorStyle() const noexcept
{
    return m_cursorStyle;
}

void QTermQuickPaintedItem::setCursorStyle(CursorStyle style)
{
    if (m_cursorStyle == style) return;
    m_cursorStyle = style;
    update();
    emit cursorStyleChanged();
}

// ── Theme ─────────────────────────────────────────────────────────────────────

QTermTheme QTermQuickPaintedItem::theme() const { return m_theme; }

void QTermQuickPaintedItem::setTheme(const QTermTheme &t)
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

// ── CursorDelegate ────────────────────────────────────────────────────────────

QQmlComponent *QTermQuickPaintedItem::cursorDelegate() const noexcept
{
    return m_cursorDelegate;
}

void QTermQuickPaintedItem::setCursorDelegate(QQmlComponent *delegate)
{
    if (m_cursorDelegate == delegate) return;

    m_cursorDelegate = delegate;

    if (m_cursorDelegateItem) {
        m_cursorDelegateItem->setParentItem(nullptr);
        m_cursorDelegateItem->deleteLater();
        m_cursorDelegateItem = nullptr;
    }

    if (m_cursorDelegate && isComponentComplete())
        recreateCursorDelegateItem();

    update();
    emit cursorDelegateChanged();
}

void QTermQuickPaintedItem::recreateCursorDelegateItem()
{
    if (m_cursorDelegateItem) {
        m_cursorDelegateItem->setParentItem(nullptr);
        m_cursorDelegateItem->deleteLater();
        m_cursorDelegateItem = nullptr;
    }

    if (!m_cursorDelegate) return;

    QQmlContext *ctx = QQmlEngine::contextForObject(this);
    if (!ctx) return;

    QObject *obj = m_cursorDelegate->beginCreate(ctx);
    m_cursorDelegateItem = qobject_cast<QQuickItem *>(obj);
    if (!m_cursorDelegateItem) {
        delete obj;
        return;
    }

    m_cursorDelegateItem->setParentItem(this);
    m_cursorDelegate->completeCreate();
    updateCursorDelegateGeometry();
}

void QTermQuickPaintedItem::updateCursorDelegateGeometry()
{
    if (!m_cursorDelegateItem) return;

    QTermTerminal *terminal = m_controller->terminal();
    if (!terminal) {
        m_cursorDelegateItem->setVisible(false);
        return;
    }

    QTermSurfaceModel *sm = terminal->surfaceModel();
    const bool visible = sm && sm->cursorVisible() && hasActiveFocus() && m_cursorOpacity > 0.0;
    m_cursorDelegateItem->setVisible(visible);

    if (visible) {
        const qreal cellW = m_controller->cellWidth();
        const qreal cellH = m_controller->cellHeight();
        m_cursorDelegateItem->setX(sm->cursorColumn() * cellW);
        m_cursorDelegateItem->setY(sm->cursorRow()    * cellH);
        m_cursorDelegateItem->setWidth(cellW);
        m_cursorDelegateItem->setHeight(cellH);
        m_cursorDelegateItem->setOpacity(m_cursorOpacity);
    }
}

} // namespace QTerm
