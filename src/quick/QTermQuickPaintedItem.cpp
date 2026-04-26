#include <QTerm/QTermQuickPaintedItem.h>

#include <QTerm/QTermSurfaceModel.h>
#include <QTerm/QTermTerminal.h>

#include "../core/QTermInputEncoder.h"

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

constexpr int kMinimumColumns = 20;
constexpr int kMinimumRows = 8;
constexpr int kWheelScrollRowsPerStep = 3;
constexpr int kResizeDebounceIntervalMs = 30;

QColor colorFromRgb(int rgb)
{
    if (rgb < 0) {
        return QColor();
    }

    return QColor((rgb >> 16) & 0xff,
                  (rgb >> 8) & 0xff,
                  rgb & 0xff);
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

    if (index < 0) {
        return QColor();
    }

    if (index < static_cast<int>(std::size(palette))) {
        return palette[index];
    }

    if (index >= 232) {
        const int level = 8 + (index - 232) * 10;
        return QColor(level, level, level);
    }

    const int cubeIndex = qMax(0, index - 16);
    const int redIndex = (cubeIndex / 36) % 6;
    const int greenIndex = (cubeIndex / 6) % 6;
    const int blueIndex = cubeIndex % 6;
    static const int componentValues[] = {0, 95, 135, 175, 215, 255};
    return QColor(componentValues[redIndex],
                  componentValues[greenIndex],
                  componentValues[blueIndex]);
}

QColor runForegroundColor(const QVariantMap &run, const QColor &defaultForeground)
{
    const int foregroundRgb = run.value(QStringLiteral("foregroundRgb"), -1).toInt();
    if (foregroundRgb >= 0) {
        return colorFromRgb(foregroundRgb);
    }

    const int foregroundIndex = run.value(QStringLiteral("foregroundIndex"), -1).toInt();
    if (foregroundIndex >= 0) {
        return colorFromPaletteIndex(foregroundIndex);
    }

    return defaultForeground;
}

QColor runBackgroundColor(const QVariantMap &run)
{
    const int backgroundRgb = run.value(QStringLiteral("backgroundRgb"), -1).toInt();
    if (backgroundRgb >= 0) {
        return colorFromRgb(backgroundRgb);
    }

    const int backgroundIndex = run.value(QStringLiteral("backgroundIndex"), -1).toInt();
    if (backgroundIndex >= 0) {
        return colorFromPaletteIndex(backgroundIndex);
    }

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
    if (run.value(QStringLiteral("inverse")).toBool()) {
        return runForegroundColor(run, defaultForeground);
    }

    return runBackgroundColor(run);
}

int runColumns(const QVariantMap &run)
{
    const int columns = run.value(QStringLiteral("columns"), 0).toInt();
    if (columns > 0) {
        return columns;
    }

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

QTermQuickPaintedItem::QTermQuickPaintedItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
    , m_resizeDebounceTimer(new QTimer(this))
    , m_selectionAutoScrollTimer(new QTimer(this))
    , m_clickResetTimer(new QTimer(this))
{
    setOpaquePainting(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setFlag(QQuickItem::ItemAcceptsInputMethod, true);
    setAcceptHoverEvents(false);
    connect(this, &QQuickItem::activeFocusChanged, this, [this]() {
        update();
        updateCursorDelegateGeometry();
    });

    m_resizeDebounceTimer->setSingleShot(true);
    m_resizeDebounceTimer->setInterval(kResizeDebounceIntervalMs);
    connect(m_resizeDebounceTimer, &QTimer::timeout, this, [this]() {
        syncTerminalSize();
    });

    // 点击连击重置：360ms 内无新点击则归零
    m_clickResetTimer->setSingleShot(true);
    m_clickResetTimer->setInterval(360);
    connect(m_clickResetTimer, &QTimer::timeout, this, [this]() {
        m_clickStreak = 0;
        m_lastClickRow = -1;
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
            : qMax<qreal>(0.0, m_dragY - height());
        const int rowsPerTick = qMax(1, static_cast<int>(std::ceil(overflow / qMax<qreal>(1.0, m_cellHeight))));
        m_terminal->scrollByLines(m_autoScrollDirection * rowsPerTick);
        updateSelectionFromDrag(m_dragX, m_dragY);
    });

    updateMetrics();
}

QTermTerminal *QTermQuickPaintedItem::terminal() const noexcept
{
    return m_terminal;
}

void QTermQuickPaintedItem::setTerminal(QTermTerminal *terminal)
{
    if (m_terminal == terminal) {
        return;
    }

    disconnectSurfaceModel();
    QObject::disconnect(m_viewportConnection);
    m_terminal = terminal;
    reconnectSurfaceModel();
    if (m_terminal) {
        m_viewportConnection = connect(m_terminal, &QTermTerminal::viewportChanged,
                                       this, [this]() { emit scrollChanged(); });
    }
    scheduleTerminalSizeSync();
    update();
    emit terminalChanged();
    emit scrollChanged();
}

QString QTermQuickPaintedItem::fontFamily() const
{
    return m_fontFamily;
}

void QTermQuickPaintedItem::setFontFamily(const QString &fontFamily)
{
    if (m_fontFamily == fontFamily) {
        return;
    }

    m_fontFamily = fontFamily;
    updateMetrics();
    update();
    emit fontChanged();
}

int QTermQuickPaintedItem::fontPixelSize() const noexcept
{
    return m_fontPixelSize;
}

void QTermQuickPaintedItem::setFontPixelSize(int fontPixelSize)
{
    const int boundedPixelSize = qMax(1, fontPixelSize);
    if (m_fontPixelSize == boundedPixelSize) {
        return;
    }

    m_fontPixelSize = boundedPixelSize;
    updateMetrics();
    update();
    emit fontChanged();
}

qreal QTermQuickPaintedItem::cellWidth() const noexcept
{
    return m_cellWidth;
}

qreal QTermQuickPaintedItem::cellHeight() const noexcept
{
    return m_cellHeight;
}

QColor QTermQuickPaintedItem::foregroundColor() const
{
    return m_foregroundColor;
}

void QTermQuickPaintedItem::setForegroundColor(const QColor &foregroundColor)
{
    if (m_foregroundColor == foregroundColor) {
        return;
    }

    m_foregroundColor = foregroundColor;
    update();
    emit paletteChanged();
}

QColor QTermQuickPaintedItem::backgroundColor() const
{
    return m_backgroundColor;
}

void QTermQuickPaintedItem::setBackgroundColor(const QColor &backgroundColor)
{
    if (m_backgroundColor == backgroundColor) {
        return;
    }

    m_backgroundColor = backgroundColor;
    update();
    emit paletteChanged();
}

QColor QTermQuickPaintedItem::selectionColor() const
{
    return m_selectionColor;
}

void QTermQuickPaintedItem::setSelectionColor(const QColor &selectionColor)
{
    if (m_selectionColor == selectionColor) {
        return;
    }

    m_selectionColor = selectionColor;
    update();
    emit paletteChanged();
}

QColor QTermQuickPaintedItem::cursorColor() const
{
    return m_cursorColor;
}

void QTermQuickPaintedItem::setCursorColor(const QColor &cursorColor)
{
    if (m_cursorColor == cursorColor) {
        return;
    }

    m_cursorColor = cursorColor;
    update();
    emit paletteChanged();
}

qreal QTermQuickPaintedItem::cursorOpacity() const noexcept
{
    return m_cursorOpacity;
}

void QTermQuickPaintedItem::setCursorOpacity(qreal cursorOpacity)
{
    const qreal boundedOpacity = qBound(0.0, cursorOpacity, 1.0);
    if (qFuzzyCompare(m_cursorOpacity, boundedOpacity)) {
        return;
    }

    m_cursorOpacity = boundedOpacity;
    update();
    updateCursorDelegateGeometry();
    emit cursorOpacityChanged();
}

int QTermQuickPaintedItem::rowAtPosition(qreal y) const
{
    const int rowCount = m_terminal ? m_terminal->rows() : 0;
    if (rowCount <= 0) {
        return 0;
    }

    const int row = static_cast<int>(std::floor(y / qMax<qreal>(1.0, m_cellHeight)));
    return qBound(0, row, rowCount - 1);
}

int QTermQuickPaintedItem::columnAtPosition(qreal x) const
{
    const int columnCount = m_terminal ? m_terminal->columns() : 0;
    if (columnCount <= 0) {
        return 0;
    }

    const int column = static_cast<int>(std::floor(x / qMax<qreal>(1.0, m_cellWidth)));
    return qBound(0, column, columnCount);
}

void QTermQuickPaintedItem::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), m_backgroundColor);

    if (!m_terminal) {
        return;
    }

    QTermSurfaceModel *surfaceModel = m_terminal->surfaceModel();
    if (!surfaceModel) {
        return;
    }

    const QFont baseFont = buildFont(m_fontFamily, m_fontPixelSize);
    const QFontMetricsF metrics(baseFont);
    const qreal textTopOffset = (m_cellHeight - metrics.height()) * 0.5;
    const QVariantList visibleRuns = surfaceModel->visibleLineRuns();
    const int lineCount = qMin(surfaceModel->rows(), visibleRuns.size());

    painter->setRenderHint(QPainter::TextAntialiasing, true);

    for (int row = 0; row < lineCount; ++row) {
        const qreal y = row * m_cellHeight;
        const QVariantList lineRuns = visibleRuns.at(row).toList();
        qreal x = 0.0;

        for (const QVariant &runValue : lineRuns) {
            const QVariantMap run = runValue.toMap();
            const int columns = runColumns(run);
            const QRectF runRect(x, y, columns * m_cellWidth, m_cellHeight);
            const QColor background = effectiveBackgroundColor(run, m_foregroundColor);
            if (background.isValid()) {
                painter->fillRect(runRect, background);
            }
            x += runRect.width();
        }

        if (selectionSpansRow(surfaceModel, row)) {
            const int selectionStart = row == surfaceModel->selectionStartRow() ? surfaceModel->selectionStartColumn() : 0;
            const int selectionEnd = row == surfaceModel->selectionEndRow() ? surfaceModel->selectionEndColumn() : surfaceModel->columns();
            if (selectionEnd > selectionStart) {
                painter->fillRect(QRectF(selectionStart * m_cellWidth,
                                         y,
                                         (selectionEnd - selectionStart) * m_cellWidth,
                                         m_cellHeight),
                                  m_selectionColor);
            }
        }

        x = 0.0;
        for (const QVariant &runValue : lineRuns) {
            const QVariantMap run = runValue.toMap();
            const int columns = runColumns(run);
            const QRectF runRect(x, y, columns * m_cellWidth, m_cellHeight);

            QFont runFont = baseFont;
            runFont.setBold(run.value(QStringLiteral("bold")).toBool());
            runFont.setItalic(run.value(QStringLiteral("italic")).toBool());
            runFont.setUnderline(run.value(QStringLiteral("underline")).toBool());
            runFont.setStrikeOut(run.value(QStringLiteral("strikethrough")).toBool());
            painter->setFont(runFont);

            QColor foreground = effectiveForegroundColor(run, m_foregroundColor);
            foreground.setAlphaF(run.value(QStringLiteral("dim")).toBool() ? 0.65 : 1.0);
            painter->setPen(foreground);
            painter->drawText(runRect.adjusted(0.0, textTopOffset, 0.0, 0.0),
                              Qt::AlignLeft | Qt::AlignTop,
                              run.value(QStringLiteral("text")).toString());
            x += runRect.width();
        }
    }

    if (surfaceModel->cursorVisible() && hasActiveFocus() && m_cursorOpacity > 0.0) {
        // delegate item 存在时跳过此处绘制，由 QQuickItem 子项自行渲染
        if (!m_cursorDelegateItem) {
            QColor cursor = m_cursorColor;
            cursor.setAlphaF(qBound(0.0, m_cursorOpacity * 0.72, 1.0));
            painter->setPen(Qt::NoPen);
            painter->setBrush(cursor);
            const qreal cx = surfaceModel->cursorColumn() * m_cellWidth;
            const qreal cy = surfaceModel->cursorRow() * m_cellHeight;
            switch (m_cursorStyle) {
            case Underline:
                painter->drawRect(QRectF(cx, cy + m_cellHeight - 2.0,
                                         qMax<qreal>(2.0, m_cellWidth), 2.0));
                break;
            case Bar:
                painter->drawRect(QRectF(cx, cy, 2.0, m_cellHeight));
                break;
            case Block:
            default:
                painter->drawRoundedRect(QRectF(cx, cy,
                                                 qMax<qreal>(2.0, m_cellWidth),
                                                 m_cellHeight),
                                          1.0, 1.0);
                break;
            }
        }
    }
}

void QTermQuickPaintedItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        // First time the item gets a real size (e.g. window just showed):
        // sync immediately so the PTY receives TIOCSWINSZ before the user
        // can interact with the shell. This prevents full-screen apps like
        // top(1) from starting with the stale 80x24 default PTY size.
        //
        // Subsequent geometry changes (window drag): coalesce via debounce to
        // avoid sending a SIGWINCH on every pixel, which would flood the shell
        // with intermediate redraws and corrupt the buffer state.
        if (oldGeometry.isEmpty()) {
            syncTerminalSize();
        } else {
            scheduleTerminalSizeSync();
        }
    }
}

void QTermQuickPaintedItem::wheelEvent(QWheelEvent *event)
{
    if (!m_terminal) {
        QQuickPaintedItem::wheelEvent(event);
        return;
    }

    // 检查鼠标协议模式
    const auto &modeState = m_terminal->modeState();
    const QPoint angleDelta = event->angleDelta();
    
    if (modeState.mouseMode != MouseMode::Disabled && angleDelta.y() != 0) {
        // 鼠标协议启用：直接编码并转发滚轮事件
        // 虚拟按钮码：64=向上，65=向下
        const int wheelButton = angleDelta.y() > 0 ? 64 : 65;
        const QByteArray wheelSequence = QTermInputEncoder::encodeMouse(
            rowAtPosition(event->position().y()),
            columnAtPosition(event->position().x()),
            static_cast<Qt::MouseButton>(wheelButton),  // 临时转换虚拟码
            event->modifiers(), true, modeState);
        
        if (!wheelSequence.isEmpty()) {
            m_terminal->feedText(QString::fromLatin1(wheelSequence));
        }
        event->accept();
        return;
    }

    // 鼠标协议禁用或纵向滚轮无效：进行页面滚动
    int deltaRows = 0;
    if (angleDelta.y() != 0) {
        const int stepCount = qMax(1, qAbs(angleDelta.y()) / 120);
        deltaRows = (angleDelta.y() > 0 ? 1 : -1) * stepCount * kWheelScrollRowsPerStep;
    } else if (event->pixelDelta().y() != 0) {
        const int pixelRows = static_cast<int>(std::round(event->pixelDelta().y() / qMax<qreal>(1.0, m_cellHeight)));
        deltaRows = pixelRows != 0 ? pixelRows : (event->pixelDelta().y() > 0 ? 1 : -1);
    }

    if (deltaRows == 0) {
        QQuickPaintedItem::wheelEvent(event);
        return;
    }

    m_terminal->scrollByLines(deltaRows);
    emit wheelScrolled(m_terminal->scrollOffset());
    emit scrollChanged();
    event->accept();
}

// ── 键盘事件 ──────────────────────────────────────────────────────────────

void QTermQuickPaintedItem::keyPressEvent(QKeyEvent *event)
{
    if (!m_terminal) {
        QQuickPaintedItem::keyPressEvent(event);
        return;
    }

    // Cmd+C (macOS) / Ctrl+C (other platforms) 优先检查选区复制。
    // 在 macOS 上只有 Cmd+C 触发复制（MetaModifier），Ctrl+C 要发送 ^C 给 PTY。
    const bool isCopyShortcut = (event->modifiers() & Qt::MetaModifier) && event->key() == Qt::Key_C;
    if (isCopyShortcut) {
        QTermSurfaceModel *surfaceModel = m_terminal->surfaceModel();
        const QString text = surfaceModel ? surfaceModel->selectedText() : QString();
        emit copyRequested(text);
        event->accept();
        return;
    }

    // 有任何可见按键时回到底部
    if (m_terminal->scrollOffset() > 0)
        m_terminal->scrollToBottom();

    // 将 (key, text) 传给 encoder。macOS 下 Ctrl+字母 的 text 可能为空，
    // QTermInputEncoder::encodeKey 会在 default 分支中合成控制字符。
    m_terminal->sendKey(event->key(), event->text());
    event->accept();
}

// ── IME / CJK 输入 ────────────────────────────────────────────────────────

QVariant QTermQuickPaintedItem::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImEnabled:
        return true;
    case Qt::ImCursorRectangle: {
        if (!m_terminal) return QVariant();
        QTermSurfaceModel *sm = m_terminal->surfaceModel();
        if (!sm) return QVariant();
        return QRectF(sm->cursorColumn() * m_cellWidth,
                      sm->cursorRow() * m_cellHeight,
                      m_cellWidth,
                      m_cellHeight);
    }
    default:
        return QQuickPaintedItem::inputMethodQuery(query);
    }
}

void QTermQuickPaintedItem::inputMethodEvent(QInputMethodEvent *event)
{
    if (!m_terminal) {
        QQuickPaintedItem::inputMethodEvent(event);
        return;
    }

    // commit string: IME 确认输入，发送给终端（等同于粘贴）
    const QString commit = event->commitString();
    if (!commit.isEmpty()) {
        if (m_terminal->scrollOffset() > 0)
            m_terminal->scrollToBottom();
        m_terminal->sendPaste(commit);
    }

    // preedit（输入法预输入）由 Qt 平台插件在候选框中显示，无需我们额外处理
    event->accept();
}

// ── 鼠标事件 ──────────────────────────────────────────────────────────────

void QTermQuickPaintedItem::mousePressEvent(QMouseEvent *event)
{
    if (!m_terminal || event->button() != Qt::LeftButton) {
        QQuickPaintedItem::mousePressEvent(event);
        return;
    }

    forceActiveFocus(Qt::MouseFocusReason);

    // 检查鼠标协议模式
    const auto &modeState = m_terminal->modeState();
    if (modeState.mouseMode != MouseMode::Disabled) {
        // 鼠标协议启用：转发鼠标事件
        m_terminal->sendMouse(rowAtPosition(event->position().y()),
                             columnAtPosition(event->position().x()),
                             event->button(), event->modifiers(), true);
        event->accept();
        return;
    }

    // 鼠标协议禁用：进行文本选择
    const int row = rowAtPosition(event->position().y());
    const int col = columnAtPosition(event->position().x());

    // 连击检测
    if (m_clickResetTimer->isActive() && m_lastClickRow == row && m_lastClickColumn == col)
        m_clickStreak += 1;
    else
        m_clickStreak = 1;

    m_lastClickRow = row;
    m_lastClickColumn = col;
    m_clickResetTimer->start();

    if (m_clickStreak >= 3) {
        // 三击：选整逻辑行
        m_selectionAnchorRow = -1;
        m_selectionAnchorColumn = -1;
        m_suppressSelectionRelease = true;
        m_autoScrollDirection = 0;
        m_selectionAutoScrollTimer->stop();
        m_terminal->selectLogicalLineAt(row);
        event->accept();
        return;
    }

    m_selectionAnchorRow = row;
    m_selectionAnchorColumn = col;
    m_dragX = event->position().x();
    m_dragY = event->position().y();
    m_suppressSelectionRelease = false;
    m_autoScrollDirection = 0;
    m_selectionAutoScrollTimer->stop();
    m_terminal->clearSelection();
    event->accept();
}

void QTermQuickPaintedItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_terminal || event->button() != Qt::LeftButton) {
        QQuickPaintedItem::mouseDoubleClickEvent(event);
        return;
    }

    forceActiveFocus(Qt::MouseFocusReason);

    // 双击：选词
    m_selectionAnchorRow = -1;
    m_selectionAnchorColumn = -1;
    m_autoScrollDirection = 0;
    m_selectionAutoScrollTimer->stop();
    m_terminal->selectWordAt(rowAtPosition(event->position().y()),
                             columnAtPosition(event->position().x()));
    event->accept();
}

void QTermQuickPaintedItem::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_terminal) {
        QQuickPaintedItem::mouseMoveEvent(event);
        return;
    }

    // 检查鼠标协议模式
    const auto &modeState = m_terminal->modeState();
    if (modeState.mouseMode == MouseMode::AnyEvent ||
        (modeState.mouseMode == MouseMode::Button && (event->buttons() & Qt::LeftButton))) {
        // 鼠标协议启用且支持移动报告：转发鼠标移动事件
        // 对于 Button 模式，只有按钮按下时才报告移动
        m_terminal->sendMouse(rowAtPosition(event->position().y()),
                             columnAtPosition(event->position().x()),
                             Qt::NoButton, event->modifiers(), false);
        event->accept();
        return;
    }

    // 鼠标协议禁用或不支持移动：进行文本选择
    if (!(event->buttons() & Qt::LeftButton) || m_suppressSelectionRelease
        || m_selectionAnchorRow < 0 || m_selectionAnchorColumn < 0) {
        QQuickPaintedItem::mouseMoveEvent(event);
        return;
    }

    m_dragX = event->position().x();
    m_dragY = event->position().y();
    updateSelectionFromDrag(m_dragX, m_dragY);

    // 判断是否需要自动滚动
    if (m_dragY < 0)
        m_autoScrollDirection = 1;   // 超出上边界 → 向上滚
    else if (m_dragY > height())
        m_autoScrollDirection = -1;  // 超出下边界 → 向下滚
    else
        m_autoScrollDirection = 0;

    if (m_autoScrollDirection != 0 && !m_selectionAutoScrollTimer->isActive())
        m_selectionAutoScrollTimer->start();
    else if (m_autoScrollDirection == 0)
        m_selectionAutoScrollTimer->stop();

    event->accept();
}

void QTermQuickPaintedItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QQuickPaintedItem::mouseReleaseEvent(event);
        return;
    }

    if (!m_terminal) {
        QQuickPaintedItem::mouseReleaseEvent(event);
        return;
    }

    // 检查鼠标协议模式
    const auto &modeState = m_terminal->modeState();
    if (modeState.mouseMode != MouseMode::Disabled) {
        // 鼠标协议启用：转发鼠标释放事件
        m_terminal->sendMouse(rowAtPosition(event->position().y()),
                             columnAtPosition(event->position().x()),
                             event->button(), event->modifiers(), false);
        event->accept();
        return;
    }

    // 鼠标协议禁用：完成文本选择
    m_autoScrollDirection = 0;
    m_selectionAutoScrollTimer->stop();

    if (m_suppressSelectionRelease) {
        m_suppressSelectionRelease = false;
        event->accept();
        return;
    }

    updateSelectionFromDrag(event->position().x(), event->position().y());
    m_selectionAnchorRow = -1;
    m_selectionAnchorColumn = -1;
    event->accept();
}

void QTermQuickPaintedItem::updateSelectionFromDrag(qreal x, qreal y)
{
    if (!m_terminal || m_selectionAnchorRow < 0 || m_selectionAnchorColumn < 0)
        return;

    const int row = rowAtPosition(y);
    const int col = qMin(m_terminal->columns(),
                         columnAtPosition(x) + 1);
    m_terminal->setSelectionRange(m_selectionAnchorRow, m_selectionAnchorColumn, row, col);
}

void QTermQuickPaintedItem::reconnectSurfaceModel()
{
    if (!m_terminal) {
        return;
    }

    QTermSurfaceModel *surfaceModel = m_terminal->surfaceModel();
    if (!surfaceModel) {
        return;
    }

    m_surfaceSizeConnection = connect(surfaceModel, &QTermSurfaceModel::sizeChanged, this, [this]() {
        update();
    });
    m_surfaceCursorConnection = connect(surfaceModel, &QTermSurfaceModel::cursorChanged, this, [this]() {
        update();
        updateCursorDelegateGeometry();
    });
    m_surfaceSelectionConnection = connect(surfaceModel, &QTermSurfaceModel::selectionChanged, this, [this]() {
        update();
    });
    m_surfaceVisibleRunsConnection = connect(surfaceModel, &QTermSurfaceModel::visibleLineRunsChanged, this, [this]() {
        update();
    });
    m_surfaceDestroyedConnection = connect(surfaceModel, &QObject::destroyed, this, [this]() {
        disconnectSurfaceModel();
        update();
    });
}

void QTermQuickPaintedItem::disconnectSurfaceModel()
{
    QObject::disconnect(m_surfaceSizeConnection);
    QObject::disconnect(m_surfaceCursorConnection);
    QObject::disconnect(m_surfaceSelectionConnection);
    QObject::disconnect(m_surfaceVisibleRunsConnection);
    QObject::disconnect(m_surfaceDestroyedConnection);
}

void QTermQuickPaintedItem::updateMetrics()
{
    const QFontMetricsF metrics(buildFont(m_fontFamily, m_fontPixelSize));
    const qreal previousCellWidth = m_cellWidth;
    const qreal previousCellHeight = m_cellHeight;

    m_cellWidth = qMax<qreal>(1.0, metrics.horizontalAdvance(QLatin1Char('M')));
    m_cellHeight = qMax<qreal>(1.0, metrics.lineSpacing());

    if (!qFuzzyCompare(previousCellWidth, m_cellWidth) || !qFuzzyCompare(previousCellHeight, m_cellHeight)) {
        emit metricsChanged();
        scheduleTerminalSizeSync();
    }
}

void QTermQuickPaintedItem::scheduleTerminalSizeSync()
{
    if (!m_terminal) {
        return;
    }

    if (width() <= 0.0 || height() <= 0.0) {
        return;
    }

    // Used only for font changes (not geometry changes) to avoid redundant
    // reflows when the font metrics haven't actually settled yet.
    m_resizeDebounceTimer->start();
}

void QTermQuickPaintedItem::syncTerminalSize()
{
    if (!m_terminal || width() <= 0.0 || height() <= 0.0) {
        return;
    }

    const int columns = qMax(kMinimumColumns, static_cast<int>(std::floor(width() / qMax<qreal>(1.0, m_cellWidth))));
    const int rows = qMax(kMinimumRows, static_cast<int>(std::floor(height() / qMax<qreal>(1.0, m_cellHeight))));
    m_terminal->setTerminalSize(columns, rows);
}

// ── Scroll position / size ────────────────────────────────────────────────

qreal QTermQuickPaintedItem::scrollSize() const noexcept
{
    if (!m_terminal) return 1.0;
    const int rows = m_terminal->rows();
    const int total = rows + m_terminal->maxScrollOffset();
    if (total <= 0) return 1.0;
    return qMax(0.08, qMin(1.0, static_cast<qreal>(rows) / total));
}

qreal QTermQuickPaintedItem::scrollPosition() const noexcept
{
    if (!m_terminal || m_terminal->maxScrollOffset() <= 0) return 0.0;
    const qreal trackSpan = 1.0 - scrollSize();
    if (trackSpan <= 0.0) return 0.0;
    // scrollOffset=0 → 底部 → position=trackSpan；scrollOffset=max → 顶部 → position=0
    return trackSpan * (1.0 - static_cast<qreal>(m_terminal->scrollOffset())
                                   / m_terminal->maxScrollOffset());
}

void QTermQuickPaintedItem::setScrollPosition(qreal position)
{
    if (!m_terminal || m_terminal->maxScrollOffset() <= 0) return;
    const qreal trackSpan = 1.0 - scrollSize();
    if (trackSpan <= 0.0) return;
    const qreal normalized = qBound(0.0, position / trackSpan, 1.0);
    const int targetOffset = qRound(m_terminal->maxScrollOffset() * (1.0 - normalized));
    const int delta = targetOffset - m_terminal->scrollOffset();
    if (delta != 0) m_terminal->scrollByLines(delta);
}

// ── componentComplete ─────────────────────────────────────────────────────

void QTermQuickPaintedItem::componentComplete()
{
    QQuickPaintedItem::componentComplete();
    // 组件完成后 QML context 已就绪，可以实例化 delegate
    if (m_cursorDelegate && !m_cursorDelegateItem)
        recreateCursorDelegateItem();
}

// ── CursorStyle ────────────────────────────────────────────────────────────

QTermQuickPaintedItem::CursorStyle QTermQuickPaintedItem::cursorStyle() const noexcept
{
    return m_cursorStyle;
}

void QTermQuickPaintedItem::setCursorStyle(CursorStyle style)
{
    if (m_cursorStyle == style)
        return;
    m_cursorStyle = style;
    update();
    emit cursorStyleChanged();
}

// ── CursorDelegate ────────────────────────────────────────────────────────

QQmlComponent *QTermQuickPaintedItem::cursorDelegate() const noexcept
{
    return m_cursorDelegate;
}

void QTermQuickPaintedItem::setCursorDelegate(QQmlComponent *delegate)
{
    if (m_cursorDelegate == delegate)
        return;

    m_cursorDelegate = delegate;

    // 销毁旧 item
    if (m_cursorDelegateItem) {
        m_cursorDelegateItem->setParentItem(nullptr);
        m_cursorDelegateItem->deleteLater();
        m_cursorDelegateItem = nullptr;
    }

    // 组件完成后才有 context；未完成则等 componentComplete() 创建
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

    if (!m_cursorDelegate)
        return;

    QQmlContext *ctx = QQmlEngine::contextForObject(this);
    if (!ctx)
        return;

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
    if (!m_cursorDelegateItem)
        return;

    if (!m_terminal) {
        m_cursorDelegateItem->setVisible(false);
        return;
    }

    QTermSurfaceModel *sm = m_terminal->surfaceModel();
    const bool visible = sm && sm->cursorVisible() && hasActiveFocus() && m_cursorOpacity > 0.0;
    m_cursorDelegateItem->setVisible(visible);

    if (visible) {
        m_cursorDelegateItem->setX(sm->cursorColumn() * m_cellWidth);
        m_cursorDelegateItem->setY(sm->cursorRow() * m_cellHeight);
        m_cursorDelegateItem->setWidth(m_cellWidth);
        m_cursorDelegateItem->setHeight(m_cellHeight);
        m_cursorDelegateItem->setOpacity(m_cursorOpacity);
    }
}

} // namespace QTerm