// SPDX-License-Identifier: MIT
//
// Where a glyph's ink actually starts inside the raster QRawFont hands out.

#pragma once

#include <QImage>
#include <QPoint>
#include <QtGlobal>

namespace QTerm {

/*!
    The offset of the glyph's first inked pixel inside \a raster.

    Qt's font engines pad the alpha map with a transparent margin for antialiasing
    (QFontEngine::glyphMargin()), so the bitmap's top-left is **not** the glyph's ink:
    measured on Menlo and PingFang at 13px, the ink starts at (1, 1). Placing that
    bitmap at QRawFont::boundingRect().topLeft() -- the only thing that says where the
    ink belongs -- therefore draws every glyph one pixel low and one pixel right.

    Scanning for the ink is exact for any engine and any font, where hard-coding the
    margin would only be right for the engines that happen to use it.

    Returns (0, 0) for a null or fully transparent raster, so callers never have to
    special-case "this glyph has no ink".
*/
inline QPoint inkOffsetIn(const QImage &raster)
{
    if (raster.isNull())
        return QPoint(0, 0);

    int left = raster.width();
    int top = raster.height();
    int right = -1;
    for (int y = 0; y < raster.height(); ++y) {
        for (int x = 0; x < raster.width(); ++x) {
            if (raster.pixelColor(x, y).alpha() == 0)
                continue;
            left = qMin(left, x);
            top = qMin(top, y);
            right = qMax(right, x);
        }
    }
    if (right < 0)
        return QPoint(0, 0);
    return QPoint(left, top);
}

} // namespace QTerm
