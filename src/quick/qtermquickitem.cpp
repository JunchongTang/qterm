#include <QTerm/QTermQuickItem.h>

#include <QTerm/QTermSurfaceModel.h>
#include <QTerm/QTermTerminal.h>

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QVariantList>
#include <QVariantMap>

#include <cmath>

namespace QTerm {

namespace {

constexpr int kMinimumColumns = 20;
constexpr int kMinimumRows = 8;

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

QTermQuickItem::QTermQuickItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setOpaquePainting(true);
    setAcceptedMouseButtons(Qt::NoButton);
    connect(this, &QQuickItem::activeFocusChanged, this, [this]() {
        update();
    });
    updateMetrics();
}

QTermTerminal *QTermQuickItem::terminal() const noexcept
{
    return m_terminal;
}

void QTermQuickItem::setTerminal(QTermTerminal *terminal)
{
    if (m_terminal == terminal) {
        return;
    }

    disconnectSurfaceModel();
    m_terminal = terminal;
    reconnectSurfaceModel();
    syncTerminalSize();
    update();
    emit terminalChanged();
}

QString QTermQuickItem::fontFamily() const
{
    return m_fontFamily;
}

void QTermQuickItem::setFontFamily(const QString &fontFamily)
{
    if (m_fontFamily == fontFamily) {
        return;
    }

    m_fontFamily = fontFamily;
    updateMetrics();
    update();
    emit fontChanged();
}

int QTermQuickItem::fontPixelSize() const noexcept
{
    return m_fontPixelSize;
}

void QTermQuickItem::setFontPixelSize(int fontPixelSize)
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

qreal QTermQuickItem::cellWidth() const noexcept
{
    return m_cellWidth;
}

qreal QTermQuickItem::cellHeight() const noexcept
{
    return m_cellHeight;
}

QColor QTermQuickItem::foregroundColor() const
{
    return m_foregroundColor;
}

void QTermQuickItem::setForegroundColor(const QColor &foregroundColor)
{
    if (m_foregroundColor == foregroundColor) {
        return;
    }

    m_foregroundColor = foregroundColor;
    update();
    emit paletteChanged();
}

QColor QTermQuickItem::backgroundColor() const
{
    return m_backgroundColor;
}

void QTermQuickItem::setBackgroundColor(const QColor &backgroundColor)
{
    if (m_backgroundColor == backgroundColor) {
        return;
    }

    m_backgroundColor = backgroundColor;
    update();
    emit paletteChanged();
}

QColor QTermQuickItem::selectionColor() const
{
    return m_selectionColor;
}

void QTermQuickItem::setSelectionColor(const QColor &selectionColor)
{
    if (m_selectionColor == selectionColor) {
        return;
    }

    m_selectionColor = selectionColor;
    update();
    emit paletteChanged();
}

QColor QTermQuickItem::cursorColor() const
{
    return m_cursorColor;
}

void QTermQuickItem::setCursorColor(const QColor &cursorColor)
{
    if (m_cursorColor == cursorColor) {
        return;
    }

    m_cursorColor = cursorColor;
    update();
    emit paletteChanged();
}

qreal QTermQuickItem::cursorOpacity() const noexcept
{
    return m_cursorOpacity;
}

void QTermQuickItem::setCursorOpacity(qreal cursorOpacity)
{
    const qreal boundedOpacity = qBound(0.0, cursorOpacity, 1.0);
    if (qFuzzyCompare(m_cursorOpacity, boundedOpacity)) {
        return;
    }

    m_cursorOpacity = boundedOpacity;
    update();
    emit cursorOpacityChanged();
}

int QTermQuickItem::rowAtPosition(qreal y) const
{
    const int rowCount = m_terminal ? m_terminal->rows() : 0;
    if (rowCount <= 0) {
        return 0;
    }

    const int row = static_cast<int>(std::floor(y / qMax<qreal>(1.0, m_cellHeight)));
    return qBound(0, row, rowCount - 1);
}

int QTermQuickItem::columnAtPosition(qreal x) const
{
    const int columnCount = m_terminal ? m_terminal->columns() : 0;
    if (columnCount <= 0) {
        return 0;
    }

    const int column = static_cast<int>(std::floor(x / qMax<qreal>(1.0, m_cellWidth)));
    return qBound(0, column, columnCount);
}

void QTermQuickItem::paint(QPainter *painter)
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
        QColor cursor = m_cursorColor;
        cursor.setAlphaF(qBound(0.0, m_cursorOpacity * 0.72, 1.0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(cursor);
        painter->drawRoundedRect(QRectF(surfaceModel->cursorColumn() * m_cellWidth,
                                        surfaceModel->cursorRow() * m_cellHeight,
                                        qMax<qreal>(2.0, m_cellWidth),
                                        m_cellHeight),
                                 1.0,
                                 1.0);
    }
}

void QTermQuickItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        syncTerminalSize();
    }
}

void QTermQuickItem::reconnectSurfaceModel()
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

void QTermQuickItem::disconnectSurfaceModel()
{
    QObject::disconnect(m_surfaceSizeConnection);
    QObject::disconnect(m_surfaceCursorConnection);
    QObject::disconnect(m_surfaceSelectionConnection);
    QObject::disconnect(m_surfaceVisibleRunsConnection);
    QObject::disconnect(m_surfaceDestroyedConnection);
}

void QTermQuickItem::updateMetrics()
{
    const QFontMetricsF metrics(buildFont(m_fontFamily, m_fontPixelSize));
    const qreal previousCellWidth = m_cellWidth;
    const qreal previousCellHeight = m_cellHeight;

    m_cellWidth = qMax<qreal>(1.0, metrics.horizontalAdvance(QLatin1Char('M')));
    m_cellHeight = qMax<qreal>(1.0, metrics.lineSpacing());

    if (!qFuzzyCompare(previousCellWidth, m_cellWidth) || !qFuzzyCompare(previousCellHeight, m_cellHeight)) {
        emit metricsChanged();
        syncTerminalSize();
    }
}

void QTermQuickItem::syncTerminalSize()
{
    if (!m_terminal || width() <= 0.0 || height() <= 0.0) {
        return;
    }

    const int columns = qMax(kMinimumColumns, static_cast<int>(std::floor(width() / qMax<qreal>(1.0, m_cellWidth))));
    const int rows = qMax(kMinimumRows, static_cast<int>(std::floor(height() / qMax<qreal>(1.0, m_cellHeight))));
    m_terminal->setTerminalSize(columns, rows);
}

} // namespace QTerm