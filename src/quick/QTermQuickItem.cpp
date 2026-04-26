#include <QTerm/QTermQuickItem.h>

#include <QTerm/QTermSurfaceModel.h>
#include <QTerm/QTermTerminal.h>

#include "QTermViewController.h"
#include "../QTermRenderUtils.h"

#include <QFontMetricsF>
#include <QHoverEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGNode>
#include <QSGTextNode>
#include <QSGVertexColorMaterial>
#include <QTextCharFormat>
#include <QTextLayout>
#include <QWheelEvent>

namespace QTerm {

// ─────────────────────────────────────────────────────────────────────────────
// Internal scene-graph node tree
//
// Layer order (bottom to top):
//   1. bgFillNode   – terminal background + per-cell colored backgrounds
//   2. selectionNode – selection highlight rectangles (semi-transparent)
//   3. textGroupNode – one QSGTextNode per visible row
//   4. cursorNode   – cursor rectangle
// ─────────────────────────────────────────────────────────────────────────────

struct QTermSGRootNode : public QSGNode
{
    QSGGeometryNode *bgFillNode = nullptr;
    QSGGeometryNode *selectionNode = nullptr;
    QSGNode *textGroupNode = nullptr;
    QSGGeometryNode *cursorNode = nullptr;
    QVector<QSGTextNode *> textNodes; // parallel to visible rows, NOT OwnedByParent
};

namespace {

// ── Geometry helpers ──────────────────────────────────────────────────────────

// Append a solid quad (2 triangles) to a ColoredPoint2D vertex buffer.
void appendQuadCPD(QSGGeometry::ColoredPoint2D *v, int &vi,
                   float x0, float y0, float x1, float y1,
                   quint8 r, quint8 g, quint8 b, quint8 a)
{
    v[vi].set(x0, y0, r, g, b, a); ++vi;
    v[vi].set(x1, y0, r, g, b, a); ++vi;
    v[vi].set(x0, y1, r, g, b, a); ++vi;
    v[vi].set(x1, y0, r, g, b, a); ++vi;
    v[vi].set(x1, y1, r, g, b, a); ++vi;
    v[vi].set(x0, y1, r, g, b, a); ++vi;
}

// Append a solid quad to a Point2D vertex buffer (for flat-color nodes).
void appendQuadP2D(QSGGeometry::Point2D *v, int &vi,
                   float x0, float y0, float x1, float y1)
{
    v[vi].set(x0, y0); ++vi;
    v[vi].set(x1, y0); ++vi;
    v[vi].set(x0, y1); ++vi;
    v[vi].set(x1, y0); ++vi;
    v[vi].set(x1, y1); ++vi;
    v[vi].set(x0, y1); ++vi;
}

// ── Node factories ────────────────────────────────────────────────────────────

QSGGeometryNode *createColoredGeomNode()
{
    auto *node = new QSGGeometryNode;
    auto *geom = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
    geom->setDrawingMode(QSGGeometry::DrawTriangles);
    node->setGeometry(geom);
    node->setFlag(QSGNode::OwnsGeometry);
    auto *mat = new QSGVertexColorMaterial;
    node->setMaterial(mat);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

QSGGeometryNode *createFlatColorGeomNode(const QColor &color)
{
    auto *node = new QSGGeometryNode;
    auto *geom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
    geom->setDrawingMode(QSGGeometry::DrawTriangles);
    node->setGeometry(geom);
    node->setFlag(QSGNode::OwnsGeometry);
    auto *mat = new QSGFlatColorMaterial;
    mat->setColor(color);
    node->setMaterial(mat);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

// ── Background fills ──────────────────────────────────────────────────────────

void rebuildBgFills(QSGGeometryNode *node, const QVariantList &lineRuns,
                    int rowCount, qreal cellW, qreal cellH,
                    qreal itemW, qreal itemH,
                    const QColor &termBg, const QColor &termFg,
                    const QColor *palette16)
{
    // 1 quad for the full terminal background + 1 per non-default bg cell run
    int quadCount = 1;
    for (int row = 0; row < rowCount && row < lineRuns.size(); ++row) {
        const QVariantList runs = lineRuns.at(row).toList();
        for (const QVariant &rv : runs) {
            const QColor bg = qtermEffectiveBackground(rv.toMap(), termFg, palette16);
            if (bg.isValid())
                ++quadCount;
        }
    }

    QSGGeometry *geom = node->geometry();
    geom->allocate(quadCount * 6);

    auto *v = geom->vertexDataAsColoredPoint2D();
    int vi = 0;

    // Full terminal background quad
    appendQuadCPD(v, vi, 0.0f, 0.0f, float(itemW), float(itemH),
                  quint8(termBg.red()), quint8(termBg.green()),
                  quint8(termBg.blue()), quint8(termBg.alpha()));

    // Per-cell colored backgrounds
    for (int row = 0; row < rowCount && row < lineRuns.size(); ++row) {
        const float y0 = float(row * cellH);
        const float y1 = float(y0 + cellH);
        float x = 0.0f;
        const QVariantList runs = lineRuns.at(row).toList();
        for (const QVariant &rv : runs) {
            const QVariantMap run = rv.toMap();
            const int cols = qtermRunColumns(run);
            const float x0 = x;
            const float x1 = x + float(cols * cellW);
            const QColor bg = qtermEffectiveBackground(run, termFg, palette16);
            if (bg.isValid()) {
                appendQuadCPD(v, vi, x0, y0, x1, y1,
                              quint8(bg.red()), quint8(bg.green()),
                              quint8(bg.blue()), quint8(bg.alpha()));
            }
            x = x1;
        }
    }

    node->markDirty(QSGNode::DirtyGeometry);
}

// ── Text nodes ────────────────────────────────────────────────────────────────

// Remove and delete all existing text row nodes from the group, then populate
// the textNodes vector with fresh nodes for the given row count.
void recreateTextRowNodes(QTermSGRootNode *root, QQuickWindow *win,
                           int rowCount)
{
    // Remove old nodes from group and delete them.
    while (QSGNode *child = root->textGroupNode->firstChild()) {
        root->textGroupNode->removeChildNode(child);
        delete child;
    }
    root->textNodes.clear();
    root->textNodes.reserve(rowCount);

    for (int i = 0; i < rowCount; ++i) {
        QSGTextNode *tn = win->createTextNode();
        root->textGroupNode->appendChildNode(tn);
        root->textNodes.push_back(tn);
    }
}

// Populate one row's text node from style-run data.
// Uses one QTextLayout per style run to leverage QSGTextNode::addTextLayout.
void populateRowTextNode(QSGTextNode *tn, int row, const QVariantList &lineRuns,
                         qreal cellW, qreal cellH,
                         const QFont &baseFont, const qreal topOffset,
                         const QColor &termFg, const QColor &hyperlinkTint,
                         const QColor *palette16)
{
    tn->clear();

    if (row >= lineRuns.size())
        return;

    const QVariantList runs = lineRuns.at(row).toList();
    const qreal rowY = row * cellH + topOffset;
    qreal x = 0.0;

    for (const QVariant &rv : runs) {
        const QVariantMap run = rv.toMap();
        const int cols = qtermRunColumns(run);
        const qreal runW = cols * cellW;
        const QString text = run.value(QStringLiteral("text")).toString();

        if (!text.isEmpty()) {
            QFont runFont = baseFont;
            runFont.setBold(run.value(QStringLiteral("bold")).toBool());
            runFont.setItalic(run.value(QStringLiteral("italic")).toBool());
            const bool hasHyperlink = run.value(QStringLiteral("hyperlinkId")).toInt() > 0;
            runFont.setUnderline(run.value(QStringLiteral("underline")).toBool() || hasHyperlink);
            runFont.setStrikeOut(run.value(QStringLiteral("strikethrough")).toBool());

            QColor fg = qtermEffectiveForeground(run, termFg, palette16);
            if (hasHyperlink
                && run.value(QStringLiteral("foregroundIndex"), -1).toInt() < 0
                && run.value(QStringLiteral("foregroundRgb"),   -1).toInt() < 0) {
                fg = hyperlinkTint.isValid() ? hyperlinkTint
                                             : QColor(QStringLiteral("#6ab0f5"));
            }
            if (run.value(QStringLiteral("dim")).toBool())
                fg.setAlphaF(0.65);

            // Build a QTextLayout for this run so we can call addTextLayout.
            QTextLayout layout(text, runFont);

            QTextCharFormat cf;
            cf.setForeground(fg);
            QTextLayout::FormatRange fmt;
            fmt.start = 0;
            fmt.length = text.length();
            fmt.format = cf;
            layout.setFormats({fmt});

            layout.beginLayout();
            QTextLine line = layout.createLine();
            if (line.isValid()) {
                line.setLineWidth(runW);
                line.setPosition(QPointF(0.0, 0.0));
            }
            layout.endLayout();

            tn->addTextLayout(QPointF(x, rowY), &layout);
        }

        x += runW;
    }
}

// ── Selection geometry ────────────────────────────────────────────────────────

void rebuildSelection(QSGGeometryNode *node, QTermSurfaceModel *sm,
                      qreal cellW, qreal cellH, const QColor &selColor)
{
    // Update flat-color material color.
    auto *mat = static_cast<QSGFlatColorMaterial *>(node->material());
    mat->setColor(selColor);

    if (!sm->selectionVisible() || !sm->hasSelection()) {
        node->geometry()->allocate(0);
        node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
        return;
    }

    const int startRow = sm->selectionStartRow();
    const int endRow   = sm->selectionEndRow();
    const int rows     = sm->rows();
    const int cols     = sm->columns();

    // One quad per selected row.
    const int quadCount = qMax(0, endRow - startRow + 1);
    QSGGeometry *geom = node->geometry();
    geom->allocate(quadCount * 6);

    auto *v = geom->vertexDataAsPoint2D();
    int vi = 0;

    for (int row = startRow; row <= endRow && row < rows; ++row) {
        const int selStart = (row == startRow) ? sm->selectionStartColumn() : 0;
        const int selEnd   = (row == endRow)   ? sm->selectionEndColumn()   : cols;
        if (selEnd <= selStart)
            continue;
        const float x0 = float(selStart * cellW);
        const float y0 = float(row * cellH);
        const float x1 = float(selEnd   * cellW);
        const float y1 = float(y0 + cellH);
        appendQuadP2D(v, vi, x0, y0, x1, y1);
    }

    node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
}

// ── Cursor geometry ───────────────────────────────────────────────────────────

void rebuildCursor(QSGGeometryNode *node, QTermSurfaceModel *sm,
                   qreal cellW, qreal cellH,
                   const QColor &cursorColor, qreal cursorOpacity,
                   int cursorStyle, bool showCursor)
{
    auto *mat = static_cast<QSGFlatColorMaterial *>(node->material());
    QColor c = cursorColor;
    c.setAlphaF(qBound(0.0, cursorOpacity * 0.72, 1.0));
    mat->setColor(c);

    if (!showCursor || !sm->cursorVisible() || cursorOpacity <= 0.0) {
        node->geometry()->allocate(0);
        node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
        return;
    }

    QSGGeometry *geom = node->geometry();
    geom->allocate(6);
    auto *v = geom->vertexDataAsPoint2D();
    int vi = 0;

    const float cx = float(sm->cursorColumn() * cellW);
    const float cy = float(sm->cursorRow()    * cellH);
    const float cw = float(qMax(2.0, cellW));
    const float ch = float(cellH);

    switch (cursorStyle) {
    case 1: // Underline
        appendQuadP2D(v, vi, cx, cy + ch - 2.0f, cx + cw, cy + ch);
        break;
    case 2: // Bar
        appendQuadP2D(v, vi, cx, cy, cx + 2.0f, cy + ch);
        break;
    default: // Block
        appendQuadP2D(v, vi, cx, cy, cx + cw, cy + ch);
        break;
    }

    node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// QTermQuickItem
// ─────────────────────────────────────────────────────────────────────────────

QTermQuickItem::QTermQuickItem(QQuickItem *parent)
    : QQuickItem(parent)
    , m_controller(new QTermViewController(this))
    , m_theme(QTermTheme::dark())
{
    setFlag(QQuickItem::ItemHasContents, true);
    setFlag(QQuickItem::ItemAcceptsInputMethod, true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(false);

    connect(this, &QQuickItem::activeFocusChanged, this, [this]() {
        m_hasFocus = hasActiveFocus();
        scheduleCursorDirty();
        updateCursorDelegateGeometry();
    });

    connect(m_controller, &QTermViewController::repaintNeeded, this, [this]() {
        scheduleContentDirty();
    });
    connect(m_controller, &QTermViewController::cursorUpdateNeeded, this, [this]() {
        scheduleCursorDirty();
        updateCursorDelegateGeometry();
    });
    connect(m_controller, &QTermViewController::mouseAcceptanceChanged,
            this, &QTermQuickItem::updateMouseAcceptance);
    connect(m_controller, &QTermViewController::focusRequested, this, [this]() {
        forceActiveFocus(Qt::MouseFocusReason);
    });
    connect(m_controller, &QTermViewController::metricsChanged, this, [this]() {
        scheduleFullDirty();
        emit metricsChanged();
    });
    connect(m_controller, &QTermViewController::scrollChanged,
            this, &QTermQuickItem::scrollChanged);
    connect(m_controller, &QTermViewController::wheelScrolled,
            this, &QTermQuickItem::wheelScrolled);
    connect(m_controller, &QTermViewController::copyRequested,
            this, &QTermQuickItem::copyRequested);
    connect(m_controller, &QTermViewController::hyperlinkActivated,
            this, &QTermQuickItem::hyperlinkActivated);
    connect(m_controller, &QTermViewController::terminalChanged, this, [this]() {
        scheduleFullDirty();
        emit terminalChanged();
        emit scrollChanged();
    });
    connect(m_controller, &QTermViewController::selectionChanged, this, [this]() {
        scheduleSelectionDirty();
    });
}

// ── Dirty flag helpers ────────────────────────────────────────────────────────

void QTermQuickItem::scheduleFullDirty()
{
    m_fullDirty = true;
    m_contentDirty = m_selectionDirty = m_cursorDirty = true;
    update();
}

void QTermQuickItem::scheduleContentDirty()
{
    m_contentDirty = true;
    update();
}

void QTermQuickItem::scheduleSelectionDirty()
{
    m_selectionDirty = true;
    update();
}

void QTermQuickItem::scheduleCursorDirty()
{
    m_cursorDirty = true;
    update();
}

// ── Terminal 绑定 ─────────────────────────────────────────────────────────────

QTermTerminal *QTermQuickItem::terminal() const noexcept
{
    return m_controller->terminal();
}

void QTermQuickItem::setTerminal(QTermTerminal *terminal)
{
    m_controller->setTerminal(terminal);
    scheduleFullDirty();
}

// ── 字体 ──────────────────────────────────────────────────────────────────────

QString QTermQuickItem::fontFamily() const
{
    return m_controller->fontFamily();
}

void QTermQuickItem::setFontFamily(const QString &fontFamily)
{
    if (m_controller->fontFamily() == fontFamily)
        return;
    m_controller->setFontFamily(fontFamily);
    scheduleFullDirty();
    emit fontChanged();
}

int QTermQuickItem::fontPixelSize() const noexcept
{
    return m_controller->fontPixelSize();
}

void QTermQuickItem::setFontPixelSize(int fontPixelSize)
{
    const int bounded = qMax(1, fontPixelSize);
    if (m_controller->fontPixelSize() == bounded)
        return;
    m_controller->setFontPixelSize(bounded);
    scheduleFullDirty();
    emit fontChanged();
}

qreal QTermQuickItem::cellWidth() const noexcept { return m_controller->cellWidth(); }
qreal QTermQuickItem::cellHeight() const noexcept { return m_controller->cellHeight(); }

// ── 调色板 ────────────────────────────────────────────────────────────────────

QColor QTermQuickItem::foregroundColor() const { return m_foregroundColor; }

void QTermQuickItem::setForegroundColor(const QColor &foregroundColor)
{
    if (m_foregroundColor == foregroundColor) return;
    m_foregroundColor = foregroundColor;
    scheduleContentDirty();
    emit paletteChanged();
}

QColor QTermQuickItem::backgroundColor() const { return m_backgroundColor; }

void QTermQuickItem::setBackgroundColor(const QColor &backgroundColor)
{
    if (m_backgroundColor == backgroundColor) return;
    m_backgroundColor = backgroundColor;
    scheduleContentDirty();
    emit paletteChanged();
}

QColor QTermQuickItem::selectionColor() const { return m_selectionColor; }

void QTermQuickItem::setSelectionColor(const QColor &selectionColor)
{
    if (m_selectionColor == selectionColor) return;
    m_selectionColor = selectionColor;
    scheduleSelectionDirty();
    emit paletteChanged();
}

QColor QTermQuickItem::cursorColor() const { return m_cursorColor; }

void QTermQuickItem::setCursorColor(const QColor &cursorColor)
{
    if (m_cursorColor == cursorColor) return;
    m_cursorColor = cursorColor;
    scheduleCursorDirty();
    emit paletteChanged();
}

qreal QTermQuickItem::cursorOpacity() const noexcept { return m_cursorOpacity; }

void QTermQuickItem::setCursorOpacity(qreal cursorOpacity)
{
    const qreal bounded = qBound(0.0, cursorOpacity, 1.0);
    if (qFuzzyCompare(m_cursorOpacity, bounded)) return;
    m_cursorOpacity = bounded;
    scheduleCursorDirty();
    updateCursorDelegateGeometry();
    emit cursorOpacityChanged();
}

// ── 滚动 ──────────────────────────────────────────────────────────────────────

qreal QTermQuickItem::scrollSize() const noexcept { return m_controller->scrollSize(); }
qreal QTermQuickItem::scrollPosition() const noexcept { return m_controller->scrollPosition(); }

void QTermQuickItem::setScrollPosition(qreal position)
{
    m_controller->setScrollPosition(position);
}

// ── 坐标辅助 ──────────────────────────────────────────────────────────────────

int QTermQuickItem::rowAtPosition(qreal y) const { return m_controller->rowAtPosition(y); }
int QTermQuickItem::columnAtPosition(qreal x) const { return m_controller->columnAtPosition(x); }

// ── CursorStyle ───────────────────────────────────────────────────────────────

QTermQuickItem::CursorStyle QTermQuickItem::cursorStyle() const noexcept { return m_cursorStyle; }

void QTermQuickItem::setCursorStyle(CursorStyle style)
{
    if (m_cursorStyle == style) return;
    m_cursorStyle = style;
    scheduleCursorDirty();
    emit cursorStyleChanged();
}

// ── Cursor delegate ───────────────────────────────────────────────────────────

QQmlComponent *QTermQuickItem::cursorDelegate() const noexcept { return m_cursorDelegate; }

void QTermQuickItem::setCursorDelegate(QQmlComponent *delegate)
{
    if (m_cursorDelegate == delegate) return;
    m_cursorDelegate = delegate;
    recreateCursorDelegateItem();
    scheduleCursorDirty();
    emit cursorDelegateChanged();
}

void QTermQuickItem::recreateCursorDelegateItem()
{
    if (m_cursorDelegateItem) {
        m_cursorDelegateItem->deleteLater();
        m_cursorDelegateItem = nullptr;
    }
    if (!m_cursorDelegate)
        return;
    QQmlContext *ctx = QQmlEngine::contextForObject(this);
    if (!ctx)
        return;
    QObject *obj = m_cursorDelegate->create(ctx);
    m_cursorDelegateItem = qobject_cast<QQuickItem *>(obj);
    if (m_cursorDelegateItem) {
        m_cursorDelegateItem->setParentItem(this);
        updateCursorDelegateGeometry();
    } else {
        delete obj;
    }
}

void QTermQuickItem::updateCursorDelegateGeometry()
{
    if (!m_cursorDelegateItem)
        return;
    QTermTerminal *terminal = m_controller->terminal();
    QTermSurfaceModel *sm = terminal ? terminal->surfaceModel() : nullptr;
    const bool visible = sm && sm->cursorVisible() && m_hasFocus;
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

// ── Theme ─────────────────────────────────────────────────────────────────────

void QTermQuickItem::loadTheme(const QTermTheme &theme)
{
    m_theme = theme;
    m_foregroundColor = theme.foreground();
    m_backgroundColor = theme.background();
    m_selectionColor  = theme.selection();
    m_cursorColor     = theme.cursor();
    scheduleFullDirty();
    emit paletteChanged();
}

// ── IME ───────────────────────────────────────────────────────────────────────

QVariant QTermQuickItem::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImEnabled:
        return true;
    case Qt::ImCursorRectangle:
        return m_controller->cursorRect();
    default:
        return QQuickItem::inputMethodQuery(query);
    }
}

// ── componentComplete ─────────────────────────────────────────────────────────

void QTermQuickItem::componentComplete()
{
    QQuickItem::componentComplete();
    if (m_cursorDelegate && !m_cursorDelegateItem)
        recreateCursorDelegateItem();
}

// ── geometryChange ────────────────────────────────────────────────────────────

void QTermQuickItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        m_controller->notifyGeometryChanged(newGeometry.width(), newGeometry.height());
}

// ── Event forwarding ──────────────────────────────────────────────────────────

void QTermQuickItem::keyPressEvent(QKeyEvent *event)
{
    if (m_controller->handleKeyPress(event)) event->accept();
    else QQuickItem::keyPressEvent(event);
}

void QTermQuickItem::inputMethodEvent(QInputMethodEvent *event)
{
    if (m_controller->handleInputMethod(event)) event->accept();
    else QQuickItem::inputMethodEvent(event);
}

void QTermQuickItem::mousePressEvent(QMouseEvent *event)
{
    if (m_controller->handleMousePress(event)) event->accept();
    else QQuickItem::mousePressEvent(event);
}

void QTermQuickItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_controller->handleMouseDoubleClick(event)) event->accept();
    else QQuickItem::mouseDoubleClickEvent(event);
}

void QTermQuickItem::mouseMoveEvent(QMouseEvent *event)
{
    if (m_controller->handleMouseMove(event)) event->accept();
    else QQuickItem::mouseMoveEvent(event);
}

void QTermQuickItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_controller->handleMouseRelease(event)) event->accept();
    else QQuickItem::mouseReleaseEvent(event);
}

void QTermQuickItem::hoverMoveEvent(QHoverEvent *event)
{
    if (m_controller->handleHoverMove(event)) event->accept();
    else QQuickItem::hoverMoveEvent(event);
}

void QTermQuickItem::wheelEvent(QWheelEvent *event)
{
    if (m_controller->handleWheel(event)) event->accept();
    else QQuickItem::wheelEvent(event);
}

// ── updateMouseAcceptance ─────────────────────────────────────────────────────

void QTermQuickItem::updateMouseAcceptance()
{
    if (m_controller->mouseProtocolEnabled()) {
        setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton | Qt::MiddleButton);
        setAcceptHoverEvents(m_controller->hoverEventsNeeded());
    } else {
        setAcceptedMouseButtons(Qt::LeftButton);
        setAcceptHoverEvents(false);
    }
}

// ── updatePaintNode ───────────────────────────────────────────────────────────
//
// Called on the render thread with the GUI thread blocked; it is safe to read
// GUI-thread member variables directly.

QSGNode *QTermQuickItem::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    QTermTerminal *terminal = m_controller->terminal();
    QTermSurfaceModel *sm = terminal ? terminal->surfaceModel() : nullptr;

    if (!sm) {
        // No surface model: tear down the node tree.
        delete old;
        m_fullDirty = m_contentDirty = m_selectionDirty = m_cursorDirty = false;
        return nullptr;
    }

    // ── Build or retrieve root node ──────────────────────────────────────────
    auto *root = static_cast<QTermSGRootNode *>(old);
    if (!root) {
        root = new QTermSGRootNode;

        root->bgFillNode = createColoredGeomNode();
        root->bgFillNode->setFlag(QSGNode::OwnedByParent);
        root->appendChildNode(root->bgFillNode);

        root->selectionNode = createFlatColorGeomNode(m_selectionColor);
        root->selectionNode->setFlag(QSGNode::OwnedByParent);
        root->appendChildNode(root->selectionNode);

        root->textGroupNode = new QSGNode;
        root->textGroupNode->setFlag(QSGNode::OwnedByParent);
        root->appendChildNode(root->textGroupNode);

        root->cursorNode = createFlatColorGeomNode(m_cursorColor);
        root->cursorNode->setFlag(QSGNode::OwnedByParent);
        root->appendChildNode(root->cursorNode);

        m_fullDirty = true; // Ensure first-frame full build.
    }

    // ── Shared parameters ────────────────────────────────────────────────────
    const qreal cellW = m_controller->cellWidth();
    const qreal cellH = m_controller->cellHeight();
    const int rows    = sm->rows();

    QFont baseFont(m_controller->fontFamily());
    baseFont.setPixelSize(m_controller->fontPixelSize());
    const QFontMetricsF fm(baseFont);

    // Vertical offset to vertically center text glyphs within the cell.
    // QTextLayout position is the top-left of the text block.
    const qreal topOffset = (cellH - fm.height()) * 0.5;

    const bool rowCountChanged = (root->textNodes.size() != rows);
    const bool needTextRebuild = m_fullDirty || m_contentDirty || rowCountChanged;

    // ── Background fills ─────────────────────────────────────────────────────
    if (m_fullDirty || m_contentDirty) {
        rebuildBgFills(root->bgFillNode, sm->visibleLineRuns(), rows,
                       cellW, cellH, width(), height(),
                       m_backgroundColor, m_foregroundColor, m_theme.palette16());
    }

    // ── Text nodes ────────────────────────────────────────────────────────────
    if (rowCountChanged) {
        recreateTextRowNodes(root, window(), rows);
    }

    if (needTextRebuild) {
        const QVariantList lineRuns = sm->visibleLineRuns();
        for (int row = 0; row < rows; ++row) {
            populateRowTextNode(root->textNodes[row], row, lineRuns,
                                cellW, cellH, baseFont, topOffset,
                                m_foregroundColor, m_theme.hyperlinkTint(),
                                m_theme.palette16());
        }
    }

    // ── Selection ─────────────────────────────────────────────────────────────
    if (m_fullDirty || m_selectionDirty) {
        rebuildSelection(root->selectionNode, sm, cellW, cellH, m_selectionColor);
    }

    // ── Cursor ────────────────────────────────────────────────────────────────
    if (m_fullDirty || m_cursorDirty) {
        const int cursorStyle = sm ? sm->cursorShape()
                                   : static_cast<int>(m_cursorStyle);
        const bool showCursor = !m_cursorDelegateItem && m_hasFocus;
        rebuildCursor(root->cursorNode, sm, cellW, cellH,
                      m_cursorColor, m_cursorOpacity, cursorStyle, showCursor);
    }

    // ── Clear dirty flags ─────────────────────────────────────────────────────
    m_fullDirty = m_contentDirty = m_selectionDirty = m_cursorDirty = false;

    return root;
}

} // namespace QTerm
