#include <QTerm/QTermQuickPaintedItem.h>

#include <QTerm/QTermSurfaceModel.h>
#include <QTerm/QTermTerminal.h>

#include "QTermViewController.h"

#include <QFont>
#include <QFontMetricsF>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTimer>
#include <QWheelEvent>
#include <QVariantList>
#include <QVariantMap>

#include <cmath>

namespace QTerm {

namespace {

QColor colorFromRgb(int rgb)
{
    if (rgb < 0)
        return QColor();
    return QColor((rgb >> 16) & 0xff,
                  (rgb >>  8) & 0xff,
                   rgb        & 0xff);
}

QColor colorFromPaletteIndex(int index)
{
    static const QColor palette[] = {
        QColor(QStringLiteral("#10151c")), QColor(QStringLiteral("#ff5f56")),
        QColor(QStringLiteral("#27c93f")), QColor(QStringLiteral("#ffbd2e")),
        QColor(QStringLiteral("#4f8cff")), QColor(QStringLiteral("#c678dd")),
        QColor(QStringLiteral("#56b6c2")), QColor(QStringLiteral("#dce7f3")),
        QColor(QStringLiteral("#5b6574")), QColor(QStringLiteral("#ff8f88")),
        QColor(QStringLiteral("#58d26a")), QColor(QStringLiteral("#ffd866")),
        QColor(QStringLiteral("#7aa2ff")), QColor(QStringLiteral("#d7a6ff")),
        QColor(QStringLiteral("#7dd3d8")), QColor(QStringLiteral("#f5fbff"))
    };

    if (index < 0)
        return QColor();

    if (index < static_cast<int>(std::size(palette)))
        return palette[index];

    if (index >= 232) {
        const int level = 8 + (index - 232) * 10;
        return QColor(level, level, level);
    }

    const int cubeIndex  = qMax(0, index - 16);
    const int redIndex   = (cubeIndex / 36) % 6;
    const int greenIndex = (cubeIndex /  6) % 6;
    const int blueIndex  =  cubeIndex       % 6;
    static const int componentValues[] = {0, 95, 135, 175, 215, 255};
    return QColor(componentValues[redIndex],
                  componentValues[greenIndex],
                  componentValues[blueIndex]);
}

QColor runForegroundColor(const QVariantMap &run, const QColor &defaultForeground)
{
    const int foregroundRgb = run.value(QStringLiteral("foregroundRgb"), -1).toInt();
    if (foregroundRgb >= 0)
        return colorFromRgb(foregroundRgb);

    const int foregroundIndex = run.value(QStringLiteral("foregroundIndex"), -1).toInt();
    if (foregroundIndex >= 0)
        return colorFromPaletteIndex(foregroundIndex);

    return defaultForeground;
}

QColor runBackgroundColor(const QVariantMap &run)
{
    const int backgroundRgb = run.value(QStringLiteral("backgroundRgb"), -1).toInt();
    if (backgroundRgb >= 0)
        return colorFromRgb(backgroundRgb);

    const int backgroundIndex = run.value(QStringLiteral("backgroundIndex"), -1).toInt();
    if (backgroundIndex >= 0)
        return colorFromPaletteIndex(backgroundIndex);

    return QColor();
}

QColor effectiveForegroundColor(const QVariantMap &run, const QColor &defaultForeground)
{
    if (run.value(QStringLiteral("inverse")).toBool()) {
        const QColor background = runBackgroundColor(run);
        return background.isValid() ? background : QColor(QStringLiteral("#0a0f15"));
    }
    return runForegroundColor(run, defaultForeground);
}

QColor effectiveBackgroundColor(const QVariantMap &run, const QColor &defaultForeground)
{
    if (run.value(QStringLiteral("inverse")).toBool())
        return runForegroundColor(run, defaultForeground);
    return runBackgroundColor(run);
}

int runColumns(const QVariantMap &run)
{
    const int columns = run.value(QStringLiteral("columns"), 0).toInt();
    if (columns > 0)
        return columns;
    return qMax(1, run.value(QStringLiteral("text")).toString().size());
}

bool selectionSpansRow(const QTermSurfaceModel *surfaceModel, int row)
{
    return surfaceModel && surfaceModel->selectionVisible() &&
           row >= surfaceModel->selectionStartRow() &&
           row <= surfaceModel->selectionEndRow();
}

QFont buildFont(const QString &family, int pixelSize)
{
    QFont font(family);
    font.setPixelSize(pixelSize);
    return font;
}

} // namespace

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
    painter->fillRect(boundingRect(), m_backgroundColor);

    QTermTerminal *terminal = m_controller->terminal();
    if (!terminal)
        return;

    QTermSurfaceModel *surfaceModel = terminal->surfaceModel();
    if (!surfaceModel)
        return;

    const qreal cellW = m_controller->cellWidth();
    const qreal cellH = m_controller->cellHeight();
    const QFont baseFont = buildFont(m_controller->fontFamily(), m_controller->fontPixelSize());
    const QFontMetricsF metrics(baseFont);
    const qreal textTopOffset = (cellH - metrics.height()) * 0.5;
    const QVariantList visibleRuns = surfaceModel->visibleLineRuns();
    const int lineCount = qMin(surfaceModel->rows(), visibleRuns.size());

    painter->setRenderHint(QPainter::TextAntialiasing, true);

    for (int row = 0; row < lineCount; ++row) {
        const qreal y = row * cellH;
        const QVariantList lineRuns = visibleRuns.at(row).toList();
        qreal x = 0.0;

        // Pass 1: backgrounds
        for (const QVariant &runValue : lineRuns) {
            const QVariantMap run = runValue.toMap();
            const int columns = runColumns(run);
            const QRectF runRect(x, y, columns * cellW, cellH);
            const QColor background = effectiveBackgroundColor(run, m_foregroundColor);
            if (background.isValid())
                painter->fillRect(runRect, background);
            x += runRect.width();
        }

        // Pass 1b: selection overlay
        if (selectionSpansRow(surfaceModel, row)) {
            const int selStart = row == surfaceModel->selectionStartRow()
                ? surfaceModel->selectionStartColumn() : 0;
            const int selEnd   = row == surfaceModel->selectionEndRow()
                ? surfaceModel->selectionEndColumn() : surfaceModel->columns();
            if (selEnd > selStart) {
                painter->fillRect(
                    QRectF(selStart * cellW, y, (selEnd - selStart) * cellW, cellH),
                    m_selectionColor);
            }
        }

        // Pass 2: text
        x = 0.0;
        for (const QVariant &runValue : lineRuns) {
            const QVariantMap run = runValue.toMap();
            const int columns = runColumns(run);
            const QRectF runRect(x, y, columns * cellW, cellH);

            QFont runFont = baseFont;
            runFont.setBold(run.value(QStringLiteral("bold")).toBool());
            runFont.setItalic(run.value(QStringLiteral("italic")).toBool());
            const bool hasHyperlink = run.value(QStringLiteral("hyperlinkId")).toInt() > 0;
            runFont.setUnderline(run.value(QStringLiteral("underline")).toBool() || hasHyperlink);
            runFont.setStrikeOut(run.value(QStringLiteral("strikethrough")).toBool());
            painter->setFont(runFont);

            QColor foreground = effectiveForegroundColor(run, m_foregroundColor);
            if (hasHyperlink
                && run.value(QStringLiteral("foregroundIndex"), -1).toInt() < 0
                && run.value(QStringLiteral("foregroundRgb"),   -1).toInt() < 0) {
                foreground = QColor(QStringLiteral("#6ab0f5"));
            }
            foreground.setAlphaF(run.value(QStringLiteral("dim")).toBool() ? 0.65 : 1.0);
            painter->setPen(foreground);
            painter->drawText(runRect.adjusted(0.0, textTopOffset, 0.0, 0.0),
                              Qt::AlignLeft | Qt::AlignTop,
                              run.value(QStringLiteral("text")).toString());
            x += runRect.width();
        }
    }

    // Cursor (built-in styles; skipped when delegate item is active)
    if (surfaceModel->cursorVisible() && hasActiveFocus() && m_cursorOpacity > 0.0) {
        if (!m_cursorDelegateItem) {
            QColor cursor = m_cursorColor;
            cursor.setAlphaF(qBound(0.0, m_cursorOpacity * 0.72, 1.0));
            painter->setPen(Qt::NoPen);
            painter->setBrush(cursor);
            const qreal cx = surfaceModel->cursorColumn() * cellW;
            const qreal cy = surfaceModel->cursorRow()    * cellH;
            switch (m_cursorStyle) {
            case Underline:
                painter->drawRect(QRectF(cx, cy + cellH - 2.0, qMax<qreal>(2.0, cellW), 2.0));
                break;
            case Bar:
                painter->drawRect(QRectF(cx, cy, 2.0, cellH));
                break;
            case Block:
            default:
                painter->drawRoundedRect(
                    QRectF(cx, cy, qMax<qreal>(2.0, cellW), cellH), 1.0, 1.0);
                break;
            }
        }
    }
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
