#include <QTerm/QTermQuickItem.h>

#include <QTerm/QTermSurfaceModel.h>
#include <QTerm/QTermTerminal.h>

#include "QTermGlyphAtlas.h"
#include "QTermTextMaterial.h"
#include "../QTermCursorDiagnostics.h"
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
#include <QScreen>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGNode>
#include <QSGTextNode>
#include <QSGVertexColorMaterial>
#include <QTextCharFormat>
#include <QTextLayout>
#include <QTextOption>
#include <QWheelEvent>
#include <cmath>

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
    QSGGeometryNode *searchNode = nullptr;         // all matches (dim)
    QSGGeometryNode *searchCurrentNode = nullptr;  // current match (bright)
    QSGNode *textGroupNode = nullptr;
    QSGGeometryNode *cursorNode = nullptr;
    QVector<QSGTextNode *> textNodes; // parallel to visible rows, NOT OwnedByParent
    // Atlas path: every glyph on screen in one geometry node, one draw call.
    QSGGeometryNode *atlasTextNode = nullptr;
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

    // Full terminal background quad.
    //
    // termBg's alpha is honoured here and *only* here: this is what makes a
    // translucent terminal possible (the pane behind shows through the cells that
    // use the default background). The same colour must NOT be reused as the
    // reverse-video glyph colour -- see QTermQuickItem::effectiveInverseTextColor.
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
// `inverseTextColor` is only consulted for reverse-video runs -- it is the colour
// the swap produces for a cell that never set a background. It is deliberately
// NOT the item's backgroundColor: that one may carry alpha for a translucent
// terminal, and a translucent glyph over a solid block renders as a smear.
void populateRowTextNode(QSGTextNode *tn, int row, const QVariantList &lineRuns,
                         qreal cellW, qreal cellH,
                         const QFont &baseFont, const qreal topOffset,
                         const QColor &termFg, const QColor &inverseTextColor,
                         const QColor &hyperlinkTint,
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

            QColor fg = qtermEffectiveForeground(run, termFg, inverseTextColor, palette16);
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

            // A run fills a fixed span of the cell grid, so it must never wrap.
            // With the default word-wrapping mode, setLineWidth(runW) below
            // moves the trailing word onto a second line as soon as the run
            // measures runW or more -- and since only the first line is ever
            // laid out, that word is silently dropped. A monospaced run of n
            // columns measures exactly n * cellW, so it hits that boundary on
            // every full-width line.
            QTextOption textOption = layout.textOption();
            textOption.setWrapMode(QTextOption::NoWrap);
            layout.setTextOption(textOption);

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

// ── Atlas text geometry ───────────────────────────────────────────────────────

// A character the atlas cannot draw, to be handed to the general text path at
// this exact position. Colour emoji are the common case, so falling back per
// glyph rather than per row matters: one emoji per line would otherwise send
// every line down the slow path.
struct FallbackGlyph
{
    QPointF position;
    QString text;
    QColor color;
    bool bold = false;
    bool italic = false;
};

// Builds glyph quads for one row, appending to `vertices`.
//
// Returns false only when the row needs decorations the atlas does not draw
// (underline, strike-through), in which case the caller renders the whole row
// through the general path. Individual characters without an atlas glyph are
// collected into `fallbacks` instead.
bool buildRowGlyphs(QVector<QTermTextMaterial::Vertex> &vertices,
                    QVector<FallbackGlyph> &fallbacks,
                    QTermGlyphAtlas &atlas, const QSizeF &atlasSize,
                    int row, const QVariantList &lineRuns,
                    qreal cellW, qreal cellH, qreal topOffset, qreal ascent,
                    const QColor &termFg, const QColor &inverseTextColor,
                    const QColor &hyperlinkTint, const QColor *palette16)
{
    if (row >= lineRuns.size())
        return true;

    const QVariantList runs = lineRuns.at(row).toList();
    const qreal baseline = row * cellH + topOffset + ascent;
    qreal x = 0.0;

    for (const QVariant &rv : runs) {
        const QVariantMap run = rv.toMap();
        const int cols = qtermRunColumns(run);
        const QString text = run.value(QStringLiteral("text")).toString();
        if (text.isEmpty()) {
            x += cols * cellW;
            continue;
        }

        const bool bold = run.value(QStringLiteral("bold")).toBool();
        const bool italic = run.value(QStringLiteral("italic")).toBool();
        const auto style = QTermGlyphAtlas::Style(
            (bold ? QTermGlyphAtlas::Bold : 0) | (italic ? QTermGlyphAtlas::Italic : 0));

        const bool hasHyperlink = run.value(QStringLiteral("hyperlinkId")).toInt() > 0;
        const bool underline = run.value(QStringLiteral("underline")).toBool() || hasHyperlink;
        const bool strikeOut = run.value(QStringLiteral("strikethrough")).toBool();

        QColor fg = qtermEffectiveForeground(run, termFg, inverseTextColor, palette16);
        if (hasHyperlink
            && run.value(QStringLiteral("foregroundIndex"), -1).toInt() < 0
            && run.value(QStringLiteral("foregroundRgb"), -1).toInt() < 0) {
            fg = hyperlinkTint.isValid() ? hyperlinkTint : QColor(QStringLiteral("#6ab0f5"));
        }
        if (run.value(QStringLiteral("dim")).toBool())
            fg.setAlphaF(0.65);

        // Premultiplied, to match the atlas and the shader.
        const float alpha = float(fg.alphaF());
        const uchar cr = uchar(qRound(fg.red() * alpha));
        const uchar cg = uchar(qRound(fg.green() * alpha));
        const uchar cb = uchar(qRound(fg.blue() * alpha));
        const uchar ca = uchar(qRound(255.0 * alpha));

        qreal penX = x;
        for (qsizetype i = 0; i < text.size(); ) {
            char32_t codePoint = text.at(i).unicode();
            qsizetype units = 1;
            if (QChar::isHighSurrogate(codePoint) && i + 1 < text.size()
                && text.at(i + 1).isLowSurrogate()) {
                codePoint = QChar::surrogateToUcs4(text.at(i), text.at(i + 1));
                units = 2;
            }
            i += units;

            // A base character followed by combining marks is one grapheme and
            // has to be positioned as a unit. The atlas holds single code
            // points, so the whole cluster goes to the general path.
            qsizetype clusterEnd = i;
            while (clusterEnd < text.size()) {
                const QChar next = text.at(clusterEnd);
                const QChar::Category category = next.category();
                if (category != QChar::Mark_NonSpacing
                    && category != QChar::Mark_SpacingCombining
                    && category != QChar::Mark_Enclosing) {
                    break;
                }
                ++clusterEnd;
            }
            if (clusterEnd > i) {
                const qsizetype start = i - units;
                fallbacks.append(FallbackGlyph{
                    QPointF(penX, row * cellH + topOffset),
                    text.mid(start, clusterEnd - start), fg, bold, italic});
                penX += cellW * (codePoint >= 0x1100 ? 2 : 1);
                i = clusterEnd;
                continue;
            }

            // A space contributes nothing to draw; skip the lookup entirely.
            if (codePoint == U' ') {
                penX += cellW;
                continue;
            }

            const QTermGlyphAtlas::Glyph *glyph = atlas.glyphFor(codePoint, style);
            if (!glyph) {
                // Colour emoji and anything no installed font covers.
                fallbacks.append(FallbackGlyph{
                    QPointF(penX, row * cellH + topOffset),
                    QString::fromUcs4(&codePoint, 1), fg, bold, italic});
                penX += cellW * (codePoint >= 0x1100 ? 2 : 1);
                continue;
            }

            const float gx = float(penX + glyph->bearing.x());
            const float gy = float(baseline + glyph->bearing.y());
            const float gw = float(glyph->region.width());
            const float gh = float(glyph->region.height());
            const float u0 = float(glyph->region.x()) / float(atlasSize.width());
            const float v0 = float(glyph->region.y()) / float(atlasSize.height());
            const float u1 = float(glyph->region.right() + 1) / float(atlasSize.width());
            const float v1 = float(glyph->region.bottom() + 1) / float(atlasSize.height());

            const QTermTextMaterial::Vertex tl{gx,      gy,      u0, v0, cr, cg, cb, ca};
            const QTermTextMaterial::Vertex tr{gx + gw, gy,      u1, v0, cr, cg, cb, ca};
            const QTermTextMaterial::Vertex bl{gx,      gy + gh, u0, v1, cr, cg, cb, ca};
            const QTermTextMaterial::Vertex br{gx + gw, gy + gh, u1, v1, cr, cg, cb, ca};
            vertices << tl << tr << bl << tr << br << bl;

            // Wide characters occupy two columns; the run's column count already
            // accounts for that, so advance by the glyph's own width in cells.
            penX += (codePoint >= 0x1100 && glyph->region.width() > cellW * 1.2)
                    ? cellW * 2 : cellW;
        }

        if ((underline || strikeOut) && penX > x) {
            const QRect solid = atlas.solidRegion();
            if (solid.isNull())
                return false; // atlas full; the general path draws the row

            const float su = float(solid.x() + 1) / float(atlasSize.width());
            const float sv = float(solid.y() + 1) / float(atlasSize.height());
            const float thickness = qMax(1.0f, float(cellH / 14.0));

            const auto appendLine = [&](float top) {
                const float x0 = float(x);
                const float x1 = float(penX);
                const QTermTextMaterial::Vertex tl{x0, top, su, sv, cr, cg, cb, ca};
                const QTermTextMaterial::Vertex tr{x1, top, su, sv, cr, cg, cb, ca};
                const QTermTextMaterial::Vertex bl{x0, top + thickness, su, sv, cr, cg, cb, ca};
                const QTermTextMaterial::Vertex br{x1, top + thickness, su, sv, cr, cg, cb, ca};
                vertices << tl << tr << bl << tr << br << bl;
            };
            if (underline)
                appendLine(float(baseline + qMax(1.0, cellH / 10.0)));
            if (strikeOut)
                appendLine(float(baseline - ascent * 0.30));
        }

        x += cols * cellW;
    }
    return true;
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

// Search-match highlight rectangles. currentOnly selects which set to draw:
// false = all non-current matches; true = the current match. Two passes into
// two nodes give the two tints with flat-color materials.
void rebuildSearchHighlights(QSGGeometryNode *node, QTermSurfaceModel *sm,
                             qreal cellW, qreal cellH, const QColor &color,
                             bool currentOnly)
{
    auto *mat = static_cast<QSGFlatColorMaterial *>(node->material());
    mat->setColor(color);

    const QVariantList highlights = sm ? sm->searchHighlights() : QVariantList{};
    if (highlights.isEmpty()) {
        node->geometry()->allocate(0);
        node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
        return;
    }

    // Count quads in this pass first (allocate exact).
    int quadCount = 0;
    for (const QVariant &v : highlights) {
        const QVariantMap h = v.toMap();
        if (h.value(QStringLiteral("current")).toBool() == currentOnly)
            ++quadCount;
    }
    QSGGeometry *geom = node->geometry();
    geom->allocate(quadCount * 6);
    auto *vtx = geom->vertexDataAsPoint2D();
    int vi = 0;
    for (const QVariant &v : highlights) {
        const QVariantMap h = v.toMap();
        if (h.value(QStringLiteral("current")).toBool() != currentOnly)
            continue;
        const int row = h.value(QStringLiteral("row")).toInt();
        const int startCol = h.value(QStringLiteral("startColumn")).toInt();
        const int endCol = h.value(QStringLiteral("endColumn")).toInt();
        const float x0 = float(startCol * cellW);
        const float y0 = float(row * cellH);
        const float x1 = float(endCol * cellW);
        const float y1 = float(y0 + cellH);
        appendQuadP2D(vtx, vi, x0, y0, x1, y1);
    }
    node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
}

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

    m_frameCoalesceTimer.setSingleShot(true);
    connect(&m_frameCoalesceTimer, &QTimer::timeout, this, [this] {
        m_lastFrameRequest.restart();
        update();
    });
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(false);
    // updateMouseAcceptance() only runs when the mode changes, so the initial
    // shape has to be set here -- otherwise a terminal that never enables mouse
    // reporting keeps the default arrow for its whole life.
    applyCursorShape();

    connect(this, &QQuickItem::activeFocusChanged, this, [this]() {
        m_hasFocus = hasActiveFocus();
        scheduleCursorDirty();
        updateCursorDelegateGeometry();
    });

    connect(m_controller, &QTermViewController::repaintNeeded, this, [this]() {
        scheduleContentDirty();
    });
    connect(m_controller, &QTermViewController::contentRowsDirty, this, [this](QVector<int> rows) {
        scheduleRowsDirty(rows);
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
    connect(m_controller, &QTermViewController::zoomRequested,
            this, &QTermQuickItem::zoomRequested);
    connect(m_controller, &QTermViewController::copyRequested,
            this, &QTermQuickItem::copyRequested);
    connect(m_controller, &QTermViewController::hyperlinkActivated,
            this, &QTermQuickItem::hyperlinkActivated);
    connect(m_controller, &QTermViewController::contextMenuRequested,
            this, &QTermQuickItem::contextMenuRequested);
    connect(m_controller, &QTermViewController::terminalChanged, this, [this]() {
        scheduleFullDirty();
        emit terminalChanged();
        emit scrollChanged();
    });
    connect(m_controller, &QTermViewController::selectionChanged, this, [this]() {
        scheduleSelectionDirty();
    });
}

QTermQuickItem::~QTermQuickItem()
{
    delete m_atlasTexture;
}

// ── Dirty flag helpers ────────────────────────────────────────────────────────

int QTermQuickItem::minimumFrameIntervalMs() const
{
    // Follow the display: a 120 Hz panel should not be held down to 60.
    if (QQuickWindow *win = window()) {
        if (const QScreen *screen = win->screen()) {
            const qreal hz = screen->refreshRate();
            if (hz > 0.0) {
                return qMax(1, qRound(1000.0 / hz));
            }
        }
    }
    return 16;
}

void QTermQuickItem::requestFrame()
{
    // A frame is already queued; the dirty flags it will read are up to date.
    if (m_frameCoalesceTimer.isActive()) {
        return;
    }

    const int interval = minimumFrameIntervalMs();
    const qint64 since = m_lastFrameRequest.isValid() ? m_lastFrameRequest.elapsed()
                                                      : interval;
    if (since >= interval) {
        // Idle long enough that this is not a burst -- draw straight away so
        // typing and other interactive updates keep zero added latency.
        m_lastFrameRequest.restart();
        update();
        return;
    }

    // Mid-burst: fold everything that arrives before the next display refresh
    // into one repaint. The timer always fires, so the final state of a burst
    // is never left undrawn.
    m_frameCoalesceTimer.start(interval - int(since));
}

void QTermQuickItem::scheduleFullDirty()
{
    m_fullDirty = true;
    m_contentDirty = m_selectionDirty = m_cursorDirty = true;
    m_dirtyRowSet.clear();
    requestFrame();
}

void QTermQuickItem::scheduleContentDirty()
{
    m_contentDirty = true;
    m_dirtyRowSet.clear();
    requestFrame();
}

void QTermQuickItem::scheduleSelectionDirty()
{
    m_selectionDirty = true;
    requestFrame();
}

void QTermQuickItem::scheduleCursorDirty()
{
    m_cursorDirty = true;
    requestFrame();
}

void QTermQuickItem::scheduleRowsDirty(QVector<int> rows)
{
    if (m_contentDirty || m_fullDirty) {
        // Already doing a full repaint; no need to track individual rows.
        requestFrame();
        return;
    }
    for (int r : rows) {
        if (!m_dirtyRowSet.contains(r)) {
            m_dirtyRowSet.append(r);
        }
    }
    requestFrame();
}

// ── Terminal binding ─────────────────────────────────────────────────────────

QTermTerminal *QTermQuickItem::terminal() const noexcept
{
    return m_controller->terminal();
}

void QTermQuickItem::setTerminal(QTermTerminal *terminal)
{
    m_controller->setTerminal(terminal);
    scheduleFullDirty();
}

// ── Font ─────────────────────────────────────────────────────────────────────

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

// ── Palette ──────────────────────────────────────────────────────────────────

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

QColor QTermQuickItem::inverseTextColor() const { return m_inverseTextColor; }

void QTermQuickItem::setInverseTextColor(const QColor &inverseTextColor)
{
    if (m_inverseTextColor == inverseTextColor) return;
    m_inverseTextColor = inverseTextColor;
    scheduleFullDirty();
    emit paletteChanged();
}

// Reverse video (SGR 7) swaps the resolved colours: the block takes the cell's
// foreground, the glyph takes its background. A cell with no explicit background
// falls back to the theme default -- and that fallback must stay *opaque* even
// when m_backgroundColor carries alpha for a translucent terminal, or the glyph
// is drawn semi-transparent over a solid block of its own foreground (and
// disappears completely at alpha 0).
//
// Deriving keeps the hue: a hardcoded constant here only looks right on one of
// the two themes.
QColor QTermQuickItem::effectiveInverseTextColor() const
{
    if (m_inverseTextColor.isValid())
        return m_inverseTextColor;
    return QColor(m_backgroundColor.rgb());
}

QColor QTermQuickItem::selectionColor() const { return m_selectionColor; }

void QTermQuickItem::setSelectionColor(const QColor &selectionColor)
{
    if (m_selectionColor == selectionColor) return;
    m_selectionColor = selectionColor;
    scheduleSelectionDirty();
    emit paletteChanged();
}

QColor QTermQuickItem::searchHighlightColor() const { return m_searchHighlightColor; }

void QTermQuickItem::setSearchHighlightColor(const QColor &color)
{
    if (m_searchHighlightColor == color) return;
    m_searchHighlightColor = color;
    scheduleSelectionDirty();
    emit paletteChanged();
}

QColor QTermQuickItem::searchCurrentColor() const { return m_searchCurrentColor; }

void QTermQuickItem::setSearchCurrentColor(const QColor &color)
{
    if (m_searchCurrentColor == color) return;
    m_searchCurrentColor = color;
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

// ── Scrolling ────────────────────────────────────────────────────────────────

qreal QTermQuickItem::scrollSize() const noexcept { return m_controller->scrollSize(); }
qreal QTermQuickItem::scrollPosition() const noexcept { return m_controller->scrollPosition(); }

void QTermQuickItem::setScrollPosition(qreal position)
{
    m_controller->setScrollPosition(position);
}

// ── Coordinate helpers ───────────────────────────────────────────────────────

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
    if (!theme.fontFamily().isEmpty())
        m_controller->setFontFamily(theme.fontFamily());
    if (theme.fontPixelSize() > 0)
        m_controller->setFontPixelSize(theme.fontPixelSize());
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
        // The right button is accepted here too: it is what raises contextMenuRequested,
        // which is how a host gets a menu without laying a MouseArea over the terminal.
        setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
        setAcceptHoverEvents(false);
    }
    applyCursorShape();
}

Qt::CursorShape QTermQuickItem::cursorShape() const
{
    return cursor().shape();
}

void QTermQuickItem::setCursorShape(Qt::CursorShape shape)
{
    if (m_explicitCursorShape && *m_explicitCursorShape == shape)
        return;
    m_explicitCursorShape = shape;
    applyCursorShape();
    emit cursorShapeChanged();
}

void QTermQuickItem::resetCursorShape()
{
    if (!m_explicitCursorShape)
        return;
    m_explicitCursorShape.reset();
    applyCursorShape();
    emit cursorShapeChanged();
}

void QTermQuickItem::applyCursorShape()
{
    // Pointer shape: a terminal is a text surface, so the pointer is an I-beam --
    // every terminal emulator does this, and the arrow reads as "nothing here is
    // selectable". The exception is an application that has taken the mouse over
    // (DECSET 1000/1002/1003: vim, htop, tmux): clicks go to it rather than to a
    // selection, so an I-beam would promise something that does not happen.
    const Qt::CursorShape shape =
        m_explicitCursorShape ? *m_explicitCursorShape
        : (m_controller->mouseProtocolEnabled() ? Qt::ArrowCursor : Qt::IBeamCursor);
    if (cursor().shape() != shape)
        setCursor(shape);
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
        m_dirtyRowSet.clear();
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

        root->searchNode = createFlatColorGeomNode(m_searchHighlightColor);
        root->searchNode->setFlag(QSGNode::OwnedByParent);
        root->appendChildNode(root->searchNode);

        root->searchCurrentNode = createFlatColorGeomNode(m_searchCurrentColor);
        root->searchCurrentNode->setFlag(QSGNode::OwnedByParent);
        root->appendChildNode(root->searchCurrentNode);

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

    // ── Glyph atlas path ─────────────────────────────────────────────────────
    static const bool useAtlas = qEnvironmentVariableIsSet("QTERM_GLYPH_ATLAS");
    if (useAtlas && !m_glyphAtlas)
        m_glyphAtlas = std::make_unique<QTermGlyphAtlas>();

    const bool rowCountChanged = (root->textNodes.size() != rows);
    const bool needTextRebuild = m_fullDirty || m_contentDirty || rowCountChanged;
    const bool hasPartialRows = !m_dirtyRowSet.isEmpty() && !needTextRebuild;

    // ── Background fills ─────────────────────────────────────────────────────
    if (m_fullDirty || m_contentDirty) {
        rebuildBgFills(root->bgFillNode, sm->visibleLineRuns(), rows,
                       cellW, cellH, width(), height(),
                       m_backgroundColor, m_foregroundColor, m_theme.palette16());
    } else if (hasPartialRows) {
        // Rebuild bg fully even for partial updates: the geometry buffer is
        // a flat array of all rows so partial edits are not simpler than full.
        rebuildBgFills(root->bgFillNode, sm->visibleLineRuns(), rows,
                       cellW, cellH, width(), height(),
                       m_backgroundColor, m_foregroundColor, m_theme.palette16());
    }

    // ── Text nodes ────────────────────────────────────────────────────────────
    if (rowCountChanged) {
        recreateTextRowNodes(root, window(), rows);
    }

    if (useAtlas && (needTextRebuild || hasPartialRows)) {
        // The whole screen is rebuilt even for a partial update: the vertex
        // buffer is one flat array, so patching a few rows is no cheaper than
        // refilling it, and it keeps the row-to-vertex mapping out of the code.
        m_glyphAtlas->setFont(baseFont);

        const QVariantList lineRuns = sm->visibleLineRuns();
        const qreal ascent = fm.ascent();
        QVector<QTermTextMaterial::Vertex> vertices;
        vertices.reserve(rows * sm->columns() * 6);

        QVector<int> fallbackRows;
        QVector<QVector<FallbackGlyph>> rowFallbacks(rows);
        for (int row = 0; row < rows; ++row) {
            const qsizetype mark = vertices.size();
            if (!buildRowGlyphs(vertices, rowFallbacks[row], *m_glyphAtlas,
                                QSizeF(m_glyphAtlas->image().size()),
                                row, lineRuns, cellW, cellH, topOffset, ascent,
                                m_foregroundColor, effectiveInverseTextColor(),
                                m_theme.hyperlinkTint(), m_theme.palette16())) {
                // Underline or strike-through: the atlas draws glyphs only, so
                // the whole row goes to the general path.
                vertices.resize(mark);
                rowFallbacks[row].clear();
                fallbackRows.append(row);
            }
        }

        if (!root->atlasTextNode) {
            root->atlasTextNode = new QSGGeometryNode;
            auto *geometry = new QSGGeometry(QTermTextMaterial::attributes(), 0);
            geometry->setDrawingMode(QSGGeometry::DrawTriangles);
            root->atlasTextNode->setGeometry(geometry);
            root->atlasTextNode->setFlag(QSGNode::OwnsGeometry);
            auto *material = new QTermTextMaterial;
            material->setFlag(QSGMaterial::Blending);
            root->atlasTextNode->setMaterial(material);
            root->atlasTextNode->setFlag(QSGNode::OwnsMaterial);
            root->textGroupNode->appendChildNode(root->atlasTextNode);
        }

        // Re-upload only when the atlas actually grew.
        if (m_atlasTextureGeneration != m_glyphAtlas->generation()
            && !m_glyphAtlas->image().isNull()) {
            delete m_atlasTexture;
            m_atlasTexture = window()->createTextureFromImage(
                m_glyphAtlas->image(), QQuickWindow::TextureHasAlphaChannel);
            m_atlasTextureGeneration = m_glyphAtlas->generation();
        }
        static_cast<QTermTextMaterial *>(root->atlasTextNode->material())
            ->setTexture(m_atlasTexture);

        QSGGeometry *geometry = root->atlasTextNode->geometry();
        geometry->allocate(int(vertices.size()));
        if (!vertices.isEmpty()) {
            memcpy(geometry->vertexData(), vertices.constData(),
                   vertices.size() * sizeof(QTermTextMaterial::Vertex));
        }
        root->atlasTextNode->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);

        // Whatever the atlas could not draw goes through the general path: a
        // whole row when decorations are involved, otherwise just the odd glyph.
        for (int row = 0; row < rows; ++row) {
            QSGTextNode *node = root->textNodes[row];
            if (fallbackRows.contains(row)) {
                populateRowTextNode(node, row, lineRuns,
                                    cellW, cellH, baseFont, topOffset,
                                    m_foregroundColor, effectiveInverseTextColor(),
                                    m_theme.hyperlinkTint(), m_theme.palette16());
                continue;
            }

            node->clear();
            for (const FallbackGlyph &fallback : std::as_const(rowFallbacks[row])) {
                QFont glyphFont = baseFont;
                glyphFont.setBold(fallback.bold);
                glyphFont.setItalic(fallback.italic);

                QTextLayout layout(fallback.text, glyphFont);
                QTextOption option = layout.textOption();
                option.setWrapMode(QTextOption::NoWrap);
                layout.setTextOption(option);

                QTextCharFormat format;
                format.setForeground(fallback.color);
                layout.setFormats({QTextLayout::FormatRange{0, int(fallback.text.size()), format}});

                layout.beginLayout();
                QTextLine textLine = layout.createLine();
                if (textLine.isValid()) {
                    textLine.setLineWidth(cellW * 2);
                    textLine.setPosition(QPointF(0.0, 0.0));
                }
                layout.endLayout();
                node->addTextLayout(fallback.position, &layout);
            }
        }
    } else if (needTextRebuild) {
        const QVariantList lineRuns = sm->visibleLineRuns();
        for (int row = 0; row < rows; ++row) {
            populateRowTextNode(root->textNodes[row], row, lineRuns,
                                cellW, cellH, baseFont, topOffset,
                                m_foregroundColor, effectiveInverseTextColor(),
                                m_theme.hyperlinkTint(), m_theme.palette16());
        }
    } else if (hasPartialRows) {
        const QVariantList lineRuns = sm->visibleLineRuns();
        for (int row : std::as_const(m_dirtyRowSet)) {
            if (row >= 0 && row < rows) {
                populateRowTextNode(root->textNodes[row], row, lineRuns,
                                    cellW, cellH, baseFont, topOffset,
                                    m_foregroundColor, effectiveInverseTextColor(),
                                    m_theme.hyperlinkTint(), m_theme.palette16());
            }
        }
    }

    // ── Selection ─────────────────────────────────────────────────────────────
    if (m_fullDirty || m_selectionDirty) {
        rebuildSelection(root->selectionNode, sm, cellW, cellH, m_selectionColor);
        rebuildSearchHighlights(root->searchNode, sm, cellW, cellH,
                                m_searchHighlightColor, /*currentOnly=*/false);
        rebuildSearchHighlights(root->searchCurrentNode, sm, cellW, cellH,
                                m_searchCurrentColor, /*currentOnly=*/true);
    }

    // ── Cursor ────────────────────────────────────────────────────────────────
    if (m_fullDirty || m_cursorDirty) {
        const int cursorStyle = sm ? sm->cursorShape()
                                   : static_cast<int>(m_cursorStyle);
        const bool showCursor = !m_cursorDelegateItem && m_hasFocus;
        qtermReportCursorDraw(m_cursorDrawReason, m_hasFocus, sm && sm->cursorVisible(),
                              m_cursorOpacity, m_cursorDelegateItem != nullptr);
        rebuildCursor(root->cursorNode, sm, cellW, cellH,
                      m_cursorColor, m_cursorOpacity, cursorStyle, showCursor);
    }

    // ── Clear dirty flags ─────────────────────────────────────────────────────
    m_fullDirty = m_contentDirty = m_selectionDirty = m_cursorDirty = false;
    m_dirtyRowSet.clear();

    return root;
}

} // namespace QTerm
