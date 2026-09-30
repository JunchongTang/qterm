// Internal header – include from .cpp files only, not installed as public API.
// All symbols live in an anonymous namespace so including in multiple TUs is safe.
//
// Provides shared QPainter-based terminal rendering used by both
// QTermQuickPaintedItem and QTermWidget.

#pragma once

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QRectF>
#include <QVariantList>
#include <QVariantMap>

#include <QTerm/QTermSurfaceModel.h>

#include "core/QTermCharWidth.h"

#include <cmath>

namespace {

// ── Keeping text on the cell grid ────────────────────────────────────────────
//
// A terminal is a grid: cell n starts at exactly n * cellW, and the emulator
// already decided how many cells each character claims (QTerm::CharWidth).
// Handing a whole run to drawText()/QTextLayout throws that away -- Qt advances
// by each glyph's own metrics, which matches the grid only while every glyph
// comes from the monospaced primary font.
//
// It stops matching the moment a character falls back to another font. Measured
// with Menlo at 15 px: a cell is 9.016 px, so a wide character owes 18.03 px,
// but the CJK fallback advances 15.00 -- three pixels short *per character*.
// Ten Chinese characters in a file name and everything after them on that line
// sits three cells to the left; that is the "the columns of `ls` don't line up"
// report. Narrow characters are not safe either: with JetBrains Mono a
// box-drawing U+2500 advances 15.0 against a 12.875 px cell, so a TUI's
// horizontal rule grows two pixels per character.
//
// So the run is split into segments of characters that need the same
// correction, and each segment is drawn with a font whose letter spacing makes
// its glyphs advance exactly one (or two) cells. Each segment is positioned at
// its own grid x, so error cannot accumulate across segments either. Plain
// ASCII in the primary font is a single segment with a zero correction -- the
// common case stays one draw call.
struct QTermGridSegment
{
    qsizetype start = 0;        // offset into the run text, in UTF-16 units
    qsizetype length = 0;
    int column = 0;             // first cell, relative to the run's first cell
    int columns = 0;
    qreal letterSpacing = 0.0;  // QFont::AbsoluteSpacing to add after each glyph
};

QVector<QTermGridSegment> qtermGridSegments(const QString &text, const QFontMetricsF &metrics,
                                            qreal cellW)
{
    QVector<QTermGridSegment> out;
    const QStringView view(text);
    qsizetype i = 0;
    int column = 0;

    // One grapheme at a time: a base character plus its combining marks is a
    // single cell and must be measured (and drawn) as a unit.
    const auto clusterAt = [&view](qsizetype at, int *cells) -> qsizetype {
        qsizetype consumed = 0;
        QTerm::CharWidth::leadingCodePoint(view.sliced(at), &consumed);
        if (consumed <= 0)
            return 0;
        *cells = qMax(1, QTerm::CharWidth::displayWidth(view.sliced(at, consumed)));
        qsizetype end = at + consumed;
        while (end < view.size()) {
            qsizetype markLen = 0;
            QTerm::CharWidth::leadingCodePoint(view.sliced(end), &markLen);
            if (markLen <= 0
                || QTerm::CharWidth::displayWidth(view.sliced(end, markLen)) != 0) {
                break;
            }
            end += markLen;
        }
        return end - at;
    };

    while (i < view.size()) {
        int cells = 1;
        const qsizetype clusterLen = clusterAt(i, &cells);
        if (clusterLen <= 0)
            break;
        const qreal advance = metrics.horizontalAdvance(text.mid(i, clusterLen));
        const qreal correction = cells * cellW - advance;

        // Extend while the next cluster needs the same correction. That is what
        // keeps a stretch of Chinese, or a stretch of ASCII, in one segment.
        qsizetype end = i + clusterLen;
        int columns = cells;
        while (end < view.size()) {
            int nextCells = 1;
            const qsizetype nextLen = clusterAt(end, &nextCells);
            if (nextLen <= 0)
                break;
            const qreal nextAdvance = metrics.horizontalAdvance(text.mid(end, nextLen));
            if (nextCells != cells || !qFuzzyCompare(nextAdvance + 1.0, advance + 1.0))
                break;
            end += nextLen;
            columns += nextCells;
        }

        out.append(QTermGridSegment{i, end - i, column, columns, correction});
        column += columns;
        i = end;
    }
    return out;
}

// ── Color helpers ─────────────────────────────────────────────────────────────

QColor qtermColorFromRgb(int rgb)
{
    if (rgb < 0) return QColor();
    return QColor((rgb >> 16) & 0xff,
                  (rgb >>  8) & 0xff,
                   rgb        & 0xff);
}

// palette16: pointer to QColor[16] from QTermTheme, or nullptr to use the
// built-in fallback palette.  index 16–255 always use the fixed 256-color cube.
QColor qtermColorFromPaletteIndex(int index, const QColor *palette16 = nullptr)
{
    if (index < 0) return QColor();

    if (index < 16) {
        if (palette16) return palette16[index];
        static const QColor builtIn[] = {
            QColor(QStringLiteral("#10151c")), QColor(QStringLiteral("#ff5f56")),
            QColor(QStringLiteral("#27c93f")), QColor(QStringLiteral("#ffbd2e")),
            QColor(QStringLiteral("#4f8cff")), QColor(QStringLiteral("#c678dd")),
            QColor(QStringLiteral("#56b6c2")), QColor(QStringLiteral("#dce7f3")),
            QColor(QStringLiteral("#5b6574")), QColor(QStringLiteral("#ff8f88")),
            QColor(QStringLiteral("#58d26a")), QColor(QStringLiteral("#ffd866")),
            QColor(QStringLiteral("#7aa2ff")), QColor(QStringLiteral("#d7a6ff")),
            QColor(QStringLiteral("#7dd3d8")), QColor(QStringLiteral("#f5fbff"))
        };
        return builtIn[index];
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

QColor qtermRunForegroundColor(const QVariantMap &run, const QColor &defaultForeground,
                               const QColor *palette16 = nullptr)
{
    const int foregroundRgb = run.value(QStringLiteral("foregroundRgb"), -1).toInt();
    if (foregroundRgb >= 0) return qtermColorFromRgb(foregroundRgb);

    const int foregroundIndex = run.value(QStringLiteral("foregroundIndex"), -1).toInt();
    if (foregroundIndex >= 0) return qtermColorFromPaletteIndex(foregroundIndex, palette16);

    return defaultForeground;
}

QColor qtermRunBackgroundColor(const QVariantMap &run, const QColor *palette16 = nullptr)
{
    const int backgroundRgb = run.value(QStringLiteral("backgroundRgb"), -1).toInt();
    if (backgroundRgb >= 0) return qtermColorFromRgb(backgroundRgb);

    const int backgroundIndex = run.value(QStringLiteral("backgroundIndex"), -1).toInt();
    if (backgroundIndex >= 0) return qtermColorFromPaletteIndex(backgroundIndex, palette16);

    return QColor();
}

QColor qtermEffectiveForeground(const QVariantMap &run, const QColor &defaultForeground,
                                const QColor &defaultBackground,
                                const QColor *palette16 = nullptr)
{
    if (run.value(QStringLiteral("inverse")).toBool()) {
        // Reverse video swaps the *resolved* colors: a cell with no explicit
        // background must take the theme's default background as its glyph
        // color. (A hardcoded dark constant here only looks right on dark
        // themes; on light themes it renders dark-on-dark.)
        const QColor bg = qtermRunBackgroundColor(run, palette16);
        return bg.isValid() ? bg : defaultBackground;
    }
    return qtermRunForegroundColor(run, defaultForeground, palette16);
}

QColor qtermEffectiveBackground(const QVariantMap &run, const QColor &defaultForeground,
                                const QColor *palette16 = nullptr)
{
    if (run.value(QStringLiteral("inverse")).toBool())
        return qtermRunForegroundColor(run, defaultForeground, palette16);
    return qtermRunBackgroundColor(run, palette16);
}

int qtermRunColumns(const QVariantMap &run)
{
    const int columns = run.value(QStringLiteral("columns"), 0).toInt();
    if (columns > 0) return columns;
    return qMax(1, run.value(QStringLiteral("text")).toString().size());
}

// ── Paint request ─────────────────────────────────────────────────────────────

// cursorStyle values: 0 = Block, 1 = Underline, 2 = Bar.
struct QTermPaintRequest {
    QRectF bounds;
    // When non-null, only rows intersecting this rect are repainted.
    // The caller is responsible for setting the painter clip accordingly.
    QRectF clipBounds;
    QTerm::QTermSurfaceModel *surfaceModel = nullptr;
    qreal cellWidth = 1.0;
    qreal cellHeight = 1.0;
    QFont baseFont;
    QColor foreground;
    QColor background;
    // Glyph colour for reverse-video cells with no explicit background of their
    // own. **Deliberately separate from `background`**: that one may carry alpha
    // for a translucent terminal, and a translucent glyph painted over a solid
    // block of its own foreground renders as a smear (or vanishes at alpha 0).
    // Invalid = derive from `background` with alpha forced opaque, which is the
    // historical behaviour.
    QColor inverseText;
    QColor selection;
    // Search matches, drawn over the selection and under the text. Invalid
    // disables the pass, which is what a host that never calls search() gets.
    QColor searchHighlight;
    QColor searchCurrent;
    QColor cursor;
    qreal cursorOpacity = 1.0;
    int cursorStyle = 0;
    // Draw built-in cursor (false when delegate item handles it, or no focus).
    bool showCursor = false;
    // Hyperlink tint. If invalid, falls back to "#6ab0f5".
    QColor hyperlinkTint;
    // Pointer to QTermTheme::palette16() for ANSI color resolution.
    // nullptr → use the built-in fallback palette.
    const QColor *palette16 = nullptr;
};

// ── Main paint function ───────────────────────────────────────────────────────

void qtermPaintTerminal(QPainter *painter, const QTermPaintRequest &req)
{
    const bool hasClip = req.clipBounds.isValid() && req.clipBounds != req.bounds;
    const QRectF paintArea = hasClip ? req.clipBounds : req.bounds;

    painter->fillRect(paintArea, req.background);

    QTerm::QTermSurfaceModel *sm = req.surfaceModel;
    if (!sm) return;

    const qreal cellW = req.cellWidth;
    const qreal cellH = req.cellHeight;
    const QFontMetricsF metrics(req.baseFont);
    const qreal textTopOffset = (cellH - metrics.height()) * 0.5;
    const QVariantList visibleRuns = sm->visibleLineRuns();
    const int lineCount = qMin(sm->rows(), visibleRuns.size());
    const QVariantList searchHighlights = sm->searchHighlights();

    painter->setRenderHint(QPainter::TextAntialiasing, true);

    for (int row = 0; row < lineCount; ++row) {
        const qreal y = row * cellH;

        // Skip rows that lie entirely outside the paint area.
        if (hasClip && (y + cellH <= paintArea.top() || y >= paintArea.bottom()))
            continue;

        const QVariantList lineRuns = visibleRuns.at(row).toList();
        qreal x = 0.0;

        // Pass 1: background fills
        for (const QVariant &rv : lineRuns) {
            const QVariantMap run = rv.toMap();
            const int columns = qtermRunColumns(run);
            const QRectF runRect(x, y, columns * cellW, cellH);
            const QColor bg = qtermEffectiveBackground(run, req.foreground, req.palette16);
            if (bg.isValid()) painter->fillRect(runRect, bg);
            x += runRect.width();
        }

        // Pass 1b: selection overlay
        if (sm->selectionVisible()
            && row >= sm->selectionStartRow()
            && row <= sm->selectionEndRow()) {
            const int selStart = row == sm->selectionStartRow() ? sm->selectionStartColumn() : 0;
            const int selEnd   = row == sm->selectionEndRow()   ? sm->selectionEndColumn()   : sm->columns();
            if (selEnd > selStart) {
                painter->fillRect(
                    QRectF(selStart * cellW, y, (selEnd - selStart) * cellW, cellH),
                    req.selection);
            }
        }

        // Pass 1c: search matches. The rows carried by the highlights are
        // viewport-relative, same as the runs, so they index directly.
        if (req.searchHighlight.isValid() || req.searchCurrent.isValid()) {
            for (const QVariant &hv : searchHighlights) {
                const QVariantMap highlight = hv.toMap();
                if (highlight.value(QStringLiteral("row")).toInt() != row)
                    continue;
                const bool isCurrent = highlight.value(QStringLiteral("current")).toBool();
                const QColor tint = isCurrent ? req.searchCurrent : req.searchHighlight;
                if (!tint.isValid())
                    continue;
                const int startColumn = highlight.value(QStringLiteral("startColumn")).toInt();
                const int endColumn = highlight.value(QStringLiteral("endColumn")).toInt();
                if (endColumn <= startColumn)
                    continue;
                painter->fillRect(QRectF(startColumn * cellW, y,
                                         (endColumn - startColumn) * cellW, cellH),
                                  tint);
            }
        }

        // Pass 2: text
        x = 0.0;
        for (const QVariant &rv : lineRuns) {
            const QVariantMap run = rv.toMap();
            const int columns = qtermRunColumns(run);

            QFont runFont = req.baseFont;
            runFont.setBold(run.value(QStringLiteral("bold")).toBool());
            runFont.setItalic(run.value(QStringLiteral("italic")).toBool());
            const bool hasHyperlink = run.value(QStringLiteral("hyperlinkId")).toInt() > 0;
            runFont.setUnderline(run.value(QStringLiteral("underline")).toBool() || hasHyperlink);
            runFont.setStrikeOut(run.value(QStringLiteral("strikethrough")).toBool());
            painter->setFont(runFont);

            const QColor inverseText = req.inverseText.isValid()
                                     ? req.inverseText
                                     : QColor(req.background.rgb());
            QColor fg = qtermEffectiveForeground(run, req.foreground, inverseText, req.palette16);
            if (hasHyperlink
                && run.value(QStringLiteral("foregroundIndex"), -1).toInt() < 0
                && run.value(QStringLiteral("foregroundRgb"),   -1).toInt() < 0) {
                fg = req.hyperlinkTint.isValid() ? req.hyperlinkTint
                                                 : QColor(QStringLiteral("#6ab0f5"));
            }
            fg.setAlphaF(run.value(QStringLiteral("dim")).toBool() ? 0.65 : 1.0);
            painter->setPen(fg);
            // Per segment, so every cell lands on the grid (see qtermGridSegments).
            const QString runText = run.value(QStringLiteral("text")).toString();
            const QFontMetricsF runMetrics(runFont);
            // Kerning would move glyphs off the grid for the same reason; a
            // terminal never wants it.
            runFont.setKerning(false);
            for (const QTermGridSegment &seg : qtermGridSegments(runText, runMetrics, cellW)) {
                QFont segFont = runFont;
                if (!qFuzzyIsNull(seg.letterSpacing))
                    segFont.setLetterSpacing(QFont::AbsoluteSpacing, seg.letterSpacing);
                painter->setFont(segFont);
                painter->drawText(
                    QRectF(x + seg.column * cellW, y + textTopOffset,
                           seg.columns * cellW, cellH),
                    Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip,
                    runText.mid(seg.start, seg.length));
            }
            x += columns * cellW;
        }
    }

    // Cursor (built-in styles)
    if (req.showCursor && sm->cursorVisible() && req.cursorOpacity > 0.0) {
        QColor cur = req.cursor;
        cur.setAlphaF(qBound(0.0, req.cursorOpacity * 0.72, 1.0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(cur);
        const qreal cx = sm->cursorColumn() * cellW;
        const qreal cy = sm->cursorRow()    * cellH;
        switch (req.cursorStyle) {
        case 1: // Underline
            painter->drawRect(QRectF(cx, cy + cellH - 2.0, qMax<qreal>(2.0, cellW), 2.0));
            break;
        case 2: // Bar
            painter->drawRect(QRectF(cx, cy, 2.0, cellH));
            break;
        default: // Block
            painter->drawRoundedRect(QRectF(cx, cy, qMax<qreal>(2.0, cellW), cellH), 1.0, 1.0);
            break;
        }
    }
}

} // anonymous namespace
