#include "QTermViewController.h"

#include <QTerm/QTermSurfaceModel.h>
#include <QTerm/QTermTerminal.h>

#include <QFont>
#include <QFontMetricsF>
#include <QHoverEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPoint>
#include <QWheelEvent>

#include <cmath>

namespace QTerm {

namespace {

constexpr int kMinimumColumns         = 20;
constexpr int kMinimumRows            = 8;
constexpr int kWheelScrollRowsPerStep = 3;
constexpr int kResizeDebounceIntervalMs = 30;

int runColumnCount(const QVariantMap &run)
{
    const int columns = run.value(QStringLiteral("columns"), 0).toInt();
    if (columns > 0)
        return columns;
    return qMax(1, run.value(QStringLiteral("text")).toString().size());
}

QFont buildFont(const QString &family, int pixelSize)
{
    QFont font(family);
    font.setPixelSize(pixelSize);
    return font;
}

} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

QTermViewController::QTermViewController(QObject *parent)
    : QObject(parent)
    , m_resizeDebounceTimer(new QTimer(this))
    , m_selectionAutoScrollTimer(new QTimer(this))
    , m_clickResetTimer(new QTimer(this))
{
    m_resizeDebounceTimer->setSingleShot(true);
    m_resizeDebounceTimer->setInterval(kResizeDebounceIntervalMs);
    connect(m_resizeDebounceTimer, &QTimer::timeout, this, [this]() {
        syncTerminalSize();
    });

    // 点击连击重置：360ms 内无新点击则归零
    m_clickResetTimer->setSingleShot(true);
    m_clickResetTimer->setInterval(360);
    connect(m_clickResetTimer, &QTimer::timeout, this, [this]() {
        m_clickStreak   = 0;
        m_lastClickRow    = -1;
        m_lastClickColumn = -1;
    });

    // 选区超出边界时自动滚动，35ms 一次
    m_selectionAutoScrollTimer->setSingleShot(false);
    m_selectionAutoScrollTimer->setInterval(35);
    connect(m_selectionAutoScrollTimer, &QTimer::timeout, this, [this]() {
        if (!m_terminal || m_autoScrollDirection == 0) {
            m_selectionAutoScrollTimer->stop();
            return;
        }
        const qreal overflow = m_autoScrollDirection > 0
            ? qMax<qreal>(0.0, -m_dragY)
            : qMax<qreal>(0.0, m_dragY - m_viewHeight);
        const int rowsPerTick = qMax(1, static_cast<int>(
            std::ceil(overflow / qMax<qreal>(1.0, m_cellHeight))));
        m_terminal->scrollByLines(m_autoScrollDirection * rowsPerTick);
        updateSelectionFromDrag(m_dragX, m_dragY);
    });

    updateMetrics();
}

// ── Terminal 绑定 ──────────────────────────────────────────────────────────────

QTermTerminal *QTermViewController::terminal() const noexcept
{
    return m_terminal;
}

void QTermViewController::setTerminal(QTermTerminal *terminal)
{
    if (m_terminal == terminal)
        return;

    disconnectSurfaceModel();
    QObject::disconnect(m_viewportConnection);
    QObject::disconnect(m_modeStateConnection);
    m_terminal = terminal;
    reconnectSurfaceModel();

    if (m_terminal) {
        m_viewportConnection = connect(
            m_terminal, &QTermTerminal::viewportChanged,
            this, [this]() { emit scrollChanged(); });
        m_modeStateConnection = connect(
            m_terminal, &QTermTerminal::modeStateChanged,
            this, [this]() { emit mouseAcceptanceChanged(); });
    }

    scheduleTerminalSizeSync();
    emit terminalChanged();
    emit scrollChanged();
}

// ── 字体 / 尺寸 ───────────────────────────────────────────────────────────────

QString QTermViewController::fontFamily() const
{
    return m_fontFamily;
}

void QTermViewController::setFontFamily(const QString &family)
{
    if (m_fontFamily == family)
        return;
    m_fontFamily = family;
    updateMetrics();
}

int QTermViewController::fontPixelSize() const noexcept
{
    return m_fontPixelSize;
}

void QTermViewController::setFontPixelSize(int size)
{
    const int bounded = qMax(1, size);
    if (m_fontPixelSize == bounded)
        return;
    m_fontPixelSize = bounded;
    updateMetrics();
}

qreal QTermViewController::cellWidth() const noexcept
{
    return m_cellWidth;
}

qreal QTermViewController::cellHeight() const noexcept
{
    return m_cellHeight;
}

// ── 几何通知 ──────────────────────────────────────────────────────────────────

void QTermViewController::notifyGeometryChanged(qreal w, qreal h)
{
    const bool wasEmpty = (m_viewWidth <= 0.0 || m_viewHeight <= 0.0);
    m_viewWidth  = w;
    m_viewHeight = h;

    if (w > 0.0 && h > 0.0) {
        if (wasEmpty)
            syncTerminalSize();       // 第一次真实尺寸：立即同步
        else
            scheduleTerminalSizeSync(); // 后续尺寸变化：防抖
    }
}

// ── 滚动 ──────────────────────────────────────────────────────────────────────

qreal QTermViewController::scrollSize() const noexcept
{
    if (!m_terminal) return 1.0;
    const int rows  = m_terminal->rows();
    const int total = rows + m_terminal->maxScrollOffset();
    if (total <= 0) return 1.0;
    return qMax(0.08, qMin(1.0, static_cast<qreal>(rows) / total));
}

qreal QTermViewController::scrollPosition() const noexcept
{
    if (!m_terminal || m_terminal->maxScrollOffset() <= 0) return 0.0;
    const qreal trackSpan = 1.0 - scrollSize();
    if (trackSpan <= 0.0) return 0.0;
    // scrollOffset=0 → 底部 → position=trackSpan；scrollOffset=max → 顶部 → position=0
    return trackSpan * (1.0 - static_cast<qreal>(m_terminal->scrollOffset())
                                   / m_terminal->maxScrollOffset());
}

void QTermViewController::setScrollPosition(qreal position)
{
    if (!m_terminal || m_terminal->maxScrollOffset() <= 0) return;
    const qreal trackSpan = 1.0 - scrollSize();
    if (trackSpan <= 0.0) return;
    const qreal normalized = qBound(0.0, position / trackSpan, 1.0);
    const int targetOffset = qRound(m_terminal->maxScrollOffset() * (1.0 - normalized));
    const int delta = targetOffset - m_terminal->scrollOffset();
    if (delta != 0) m_terminal->scrollByLines(delta);
}

// ── 坐标辅助 ──────────────────────────────────────────────────────────────────

int QTermViewController::rowAtPosition(qreal y) const
{
    const int rowCount = m_terminal ? m_terminal->rows() : 0;
    if (rowCount <= 0) return 0;
    const int row = static_cast<int>(std::floor(y / qMax<qreal>(1.0, m_cellHeight)));
    return qBound(0, row, rowCount - 1);
}

int QTermViewController::columnAtPosition(qreal x) const
{
    const int columnCount = m_terminal ? m_terminal->columns() : 0;
    if (columnCount <= 0) return 0;
    const int column = static_cast<int>(std::floor(x / qMax<qreal>(1.0, m_cellWidth)));
    return qBound(0, column, columnCount);
}

int QTermViewController::hyperlinkIdAtPosition(int row, int col) const
{
    if (!m_terminal) return 0;
    QTermSurfaceModel *surfaceModel = m_terminal->surfaceModel();
    if (!surfaceModel) return 0;
    const QVariantList allRuns = surfaceModel->visibleLineRuns();
    if (row < 0 || row >= allRuns.size()) return 0;
    const QVariantList lineRuns = allRuns.at(row).toList();
    int x = 0;
    for (const QVariant &rv : lineRuns) {
        const QVariantMap run = rv.toMap();
        const int columns = runColumnCount(run);
        if (col < x + columns)
            return run.value(QStringLiteral("hyperlinkId")).toInt();
        x += columns;
    }
    return 0;
}

// ── 鼠标协议状态查询 ──────────────────────────────────────────────────────────

bool QTermViewController::mouseProtocolEnabled() const
{
    if (!m_terminal) return false;
    return m_terminal->modeState().mouseTracking != MouseTracking::Disabled;
}

bool QTermViewController::hoverEventsNeeded() const
{
    if (!m_terminal) return false;
    return m_terminal->modeState().mouseTracking == MouseTracking::AnyEvent;
}

// ── IME 光标矩形 ──────────────────────────────────────────────────────────────

QRectF QTermViewController::cursorRect() const
{
    if (!m_terminal) return {};
    QTermSurfaceModel *sm = m_terminal->surfaceModel();
    if (!sm) return {};
    return QRectF(sm->cursorColumn() * m_cellWidth,
                  sm->cursorRow()    * m_cellHeight,
                  m_cellWidth,
                  m_cellHeight);
}

// ── 输入事件分发 ──────────────────────────────────────────────────────────────

bool QTermViewController::handleKeyPress(QKeyEvent *event)
{
    if (!m_terminal) return false;

    if (m_terminal->scrollOffset() > 0)
        m_terminal->scrollToBottom();
    // macOS 下 Ctrl+字母 的 text 可能为空，sendKey 的 encoder 会合成控制字符
    m_terminal->sendKey(event->key(), event->text());
    return true;
}

bool QTermViewController::handleInputMethod(QInputMethodEvent *event)
{
    if (!m_terminal) return false;

    const QString commit = event->commitString();
    if (!commit.isEmpty()) {
        if (m_terminal->scrollOffset() > 0)
            m_terminal->scrollToBottom();
        m_terminal->sendPaste(commit);
    }
    // preedit 由 Qt 平台插件在候选框中显示，无需额外处理
    return true;
}

bool QTermViewController::handleMousePress(QMouseEvent *event)
{
    if (!m_terminal || event->button() != Qt::LeftButton)
        return false;

    emit focusRequested();

    const auto &modeState = m_terminal->modeState();
    if (modeState.mouseTracking != MouseTracking::Disabled) {
        m_terminal->sendMouse(rowAtPosition(event->position().y()),
                              columnAtPosition(event->position().x()),
                              event->button(), event->modifiers(), true);
        return true;
    }

    const int row = rowAtPosition(event->position().y());
    const int col = columnAtPosition(event->position().x());

    // OSC 8 超链接：Cmd+单击（macOS: ControlModifier = ⌘）
    if (event->modifiers() & Qt::ControlModifier) {
        const int hyperlinkId = hyperlinkIdAtPosition(row, col);
        if (hyperlinkId > 0) {
            const QString url = m_terminal->hyperlinkUrl(hyperlinkId);
            if (!url.isEmpty()) {
                emit hyperlinkActivated(url);
                return true;
            }
        }
    }

    // 连击检测
    if (m_clickResetTimer->isActive() && m_lastClickRow == row && m_lastClickColumn == col)
        m_clickStreak += 1;
    else
        m_clickStreak = 1;

    m_lastClickRow    = row;
    m_lastClickColumn = col;
    m_clickResetTimer->start();

    if (m_clickStreak >= 3) {
        // 三击：选整逻辑行
        m_selectionAnchorRow    = -1;
        m_selectionAnchorColumn = -1;
        m_suppressSelectionRelease = true;
        m_autoScrollDirection = 0;
        m_selectionAutoScrollTimer->stop();
        m_terminal->selectLogicalLineAt(row);
        return true;
    }

    m_selectionAnchorRow    = row;
    m_selectionAnchorColumn = col;
    m_dragX = event->position().x();
    m_dragY = event->position().y();
    m_suppressSelectionRelease = false;
    m_autoScrollDirection = 0;
    m_selectionAutoScrollTimer->stop();
    m_terminal->clearSelection();
    return true;
}

bool QTermViewController::handleMouseDoubleClick(QMouseEvent *event)
{
    if (!m_terminal || event->button() != Qt::LeftButton)
        return false;

    emit focusRequested();

    m_selectionAnchorRow    = -1;
    m_selectionAnchorColumn = -1;
    m_autoScrollDirection = 0;
    m_selectionAutoScrollTimer->stop();
    m_terminal->selectWordAt(rowAtPosition(event->position().y()),
                             columnAtPosition(event->position().x()));
    return true;
}

bool QTermViewController::handleMouseMove(QMouseEvent *event)
{
    if (!m_terminal) return false;

    const auto &modeState = m_terminal->modeState();
    if (modeState.mouseTracking == MouseTracking::AnyEvent ||
        (modeState.mouseTracking == MouseTracking::Button && (event->buttons() & Qt::LeftButton))) {
        const Qt::MouseButton heldButton =
            (event->buttons() & Qt::LeftButton)   ? Qt::LeftButton   :
            (event->buttons() & Qt::MiddleButton) ? Qt::MiddleButton :
            (event->buttons() & Qt::RightButton)  ? Qt::RightButton  : Qt::NoButton;
        m_terminal->sendMouse(rowAtPosition(event->position().y()),
                              columnAtPosition(event->position().x()),
                              heldButton, event->modifiers(), false, /*isMotion=*/true);
        return true;
    }

    if (!(event->buttons() & Qt::LeftButton) || m_suppressSelectionRelease
        || m_selectionAnchorRow < 0 || m_selectionAnchorColumn < 0)
        return false;

    m_dragX = event->position().x();
    m_dragY = event->position().y();
    updateSelectionFromDrag(m_dragX, m_dragY);

    if (m_dragY < 0)
        m_autoScrollDirection = 1;           // 超出上边界 → 向上滚
    else if (m_dragY > m_viewHeight)
        m_autoScrollDirection = -1;          // 超出下边界 → 向下滚
    else
        m_autoScrollDirection = 0;

    if (m_autoScrollDirection != 0 && !m_selectionAutoScrollTimer->isActive())
        m_selectionAutoScrollTimer->start();
    else if (m_autoScrollDirection == 0)
        m_selectionAutoScrollTimer->stop();

    return true;
}

bool QTermViewController::handleMouseRelease(QMouseEvent *event)
{
    if (!m_terminal) return false;

    const auto &modeState = m_terminal->modeState();
    if (modeState.mouseTracking != MouseTracking::Disabled) {
        m_terminal->sendMouse(rowAtPosition(event->position().y()),
                              columnAtPosition(event->position().x()),
                              event->button(), event->modifiers(), false);
        return true;
    }

    if (event->button() != Qt::LeftButton)
        return false;

    m_autoScrollDirection = 0;
    m_selectionAutoScrollTimer->stop();

    if (m_suppressSelectionRelease) {
        m_suppressSelectionRelease = false;
        return true;
    }

    updateSelectionFromDrag(event->position().x(), event->position().y());
    m_selectionAnchorRow    = -1;
    m_selectionAnchorColumn = -1;
    return true;
}

bool QTermViewController::handleHoverMove(QHoverEvent *event)
{
    if (!m_terminal) return false;

    const auto &modeState = m_terminal->modeState();
    if (modeState.mouseTracking == MouseTracking::AnyEvent) {
        const QPointF pos = event->position();
        m_terminal->sendMouse(rowAtPosition(pos.y()), columnAtPosition(pos.x()),
                              Qt::NoButton, Qt::NoModifier, false, /*isMotion=*/true);
        return true;
    }
    return false;
}

bool QTermViewController::handleWheel(QWheelEvent *event)
{
    if (!m_terminal) return false;

    const auto &modeState = m_terminal->modeState();
    const QPoint angleDelta = event->angleDelta();

    if (modeState.mouseTracking != MouseTracking::Disabled && angleDelta.y() != 0) {
        // 鼠标协议启用：虚拟按钮码 64=向上、65=向下（X10/SGR 滚轮约定）
        const int wheelButton = angleDelta.y() > 0 ? 64 : 65;
        m_terminal->sendMouse(
            rowAtPosition(event->position().y()),
            columnAtPosition(event->position().x()),
            wheelButton,
            static_cast<int>(event->modifiers()),
            true);
        return true;
    }

    // 鼠标协议禁用：滚动 scrollback
    int deltaRows = 0;
    if (angleDelta.y() != 0) {
        const int stepCount = qMax(1, qAbs(angleDelta.y()) / 120);
        deltaRows = (angleDelta.y() > 0 ? 1 : -1) * stepCount * kWheelScrollRowsPerStep;
    } else if (event->pixelDelta().y() != 0) {
        const int pixelRows = static_cast<int>(
            std::round(event->pixelDelta().y() / qMax<qreal>(1.0, m_cellHeight)));
        deltaRows = pixelRows != 0 ? pixelRows : (event->pixelDelta().y() > 0 ? 1 : -1);
    }

    if (deltaRows == 0) return false;

    m_terminal->scrollByLines(deltaRows);
    emit wheelScrolled(m_terminal->scrollOffset());
    emit scrollChanged();
    return true;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void QTermViewController::reconnectSurfaceModel()
{
    if (!m_terminal) return;

    QTermSurfaceModel *surfaceModel = m_terminal->surfaceModel();
    if (!surfaceModel) return;

    m_surfaceSizeConnection = connect(surfaceModel, &QTermSurfaceModel::sizeChanged, this, [this]() {
        emit repaintNeeded();
    });
    m_surfaceCursorConnection = connect(surfaceModel, &QTermSurfaceModel::cursorChanged, this, [this]() {
        emit repaintNeeded();
        emit cursorUpdateNeeded();
    });
    m_surfaceSelectionConnection = connect(surfaceModel, &QTermSurfaceModel::selectionChanged, this, [this]() {
        emit repaintNeeded();
    });
    m_surfaceVisibleRunsConnection = connect(surfaceModel, &QTermSurfaceModel::visibleLineRunsChanged, this, [this]() {
        emit repaintNeeded();
    });
    m_surfaceDestroyedConnection = connect(surfaceModel, &QObject::destroyed, this, [this]() {
        disconnectSurfaceModel();
        emit repaintNeeded();
    });
}

void QTermViewController::disconnectSurfaceModel()
{
    QObject::disconnect(m_surfaceSizeConnection);
    QObject::disconnect(m_surfaceCursorConnection);
    QObject::disconnect(m_surfaceSelectionConnection);
    QObject::disconnect(m_surfaceVisibleRunsConnection);
    QObject::disconnect(m_surfaceDestroyedConnection);
}

void QTermViewController::updateMetrics()
{
    const QFontMetricsF metrics(buildFont(m_fontFamily, m_fontPixelSize));
    const qreal previousCellWidth  = m_cellWidth;
    const qreal previousCellHeight = m_cellHeight;

    m_cellWidth  = qMax<qreal>(1.0, metrics.horizontalAdvance(QLatin1Char('M')));
    m_cellHeight = qMax<qreal>(1.0, metrics.lineSpacing());

    if (!qFuzzyCompare(previousCellWidth,  m_cellWidth) ||
        !qFuzzyCompare(previousCellHeight, m_cellHeight)) {
        emit metricsChanged();
        scheduleTerminalSizeSync();
    }
}

void QTermViewController::scheduleTerminalSizeSync()
{
    if (!m_terminal || m_viewWidth <= 0.0 || m_viewHeight <= 0.0)
        return;
    m_resizeDebounceTimer->start();
}

void QTermViewController::syncTerminalSize()
{
    if (!m_terminal || m_viewWidth <= 0.0 || m_viewHeight <= 0.0)
        return;
    const int columns = qMax(kMinimumColumns,
        static_cast<int>(std::floor(m_viewWidth  / qMax<qreal>(1.0, m_cellWidth))));
    const int rows = qMax(kMinimumRows,
        static_cast<int>(std::floor(m_viewHeight / qMax<qreal>(1.0, m_cellHeight))));
    m_terminal->setTerminalSize(columns, rows);
}

void QTermViewController::updateSelectionFromDrag(qreal x, qreal y)
{
    if (!m_terminal || m_selectionAnchorRow < 0 || m_selectionAnchorColumn < 0)
        return;
    const int row = rowAtPosition(y);
    const int col = qMin(m_terminal->columns(), columnAtPosition(x) + 1);
    m_terminal->setSelectionRange(m_selectionAnchorRow, m_selectionAnchorColumn, row, col);
}

} // namespace QTerm
