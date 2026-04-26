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

#include <cmath>

namespace {

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
                                const QColor *palette16 = nullptr)
{
    if (run.value(QStringLiteral("inverse")).toBool()) {
        const QColor bg = qtermRunBackgroundColor(run, palette16);
        return bg.isValid() ? bg : QColor(QStringLiteral("#0a0f15"));
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
    QTerm::QTermSurfaceModel *surfaceModel = nullptr;
    qreal cellWidth = 1.0;
    qreal cellHeight = 1.0;
    QFont baseFont;
    QColor foreground;
    QColor background;
    QColor selection;
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
    painter->fillRect(req.bounds, req.background);

    QTerm::QTermSurfaceModel *sm = req.surfaceModel;
    if (!sm) return;

    const qreal cellW = req.cellWidth;
    const qreal cellH = req.cellHeight;
    const QFontMetricsF metrics(req.baseFont);
    const qreal textTopOffset = (cellH - metrics.height()) * 0.5;
    const QVariantList visibleRuns = sm->visibleLineRuns();
    const int lineCount = qMin(sm->rows(), visibleRuns.size());

    painter->setRenderHint(QPainter::TextAntialiasing, true);

    for (int row = 0; row < lineCount; ++row) {
        const qreal y = row * cellH;
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

            QColor fg = qtermEffectiveForeground(run, req.foreground, req.palette16);
            if (hasHyperlink
                && run.value(QStringLiteral("foregroundIndex"), -1).toInt() < 0
                && run.value(QStringLiteral("foregroundRgb"),   -1).toInt() < 0) {
                fg = req.hyperlinkTint.isValid() ? req.hyperlinkTint
                                                 : QColor(QStringLiteral("#6ab0f5"));
            }
            fg.setAlphaF(run.value(QStringLiteral("dim")).toBool() ? 0.65 : 1.0);
            painter->setPen(fg);
            painter->drawText(
                QRectF(x, y + textTopOffset, columns * cellW, cellH),
                Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip,
                run.value(QStringLiteral("text")).toString());
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
