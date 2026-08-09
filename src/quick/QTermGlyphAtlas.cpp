#include "QTermGlyphAtlas.h"

#include <QFont>
#include <QGlyphRun>
#include <QPainter>
#include <QTextLayout>

namespace QTerm {

namespace {

// Large enough for the several hundred distinct glyphs a terminal session
// typically touches, including a CJK working set, without being so large that
// a size change makes reallocation noticeable.
constexpr int kAtlasWidth = 2048;
constexpr int kAtlasHeight = 2048;

// Keeps neighbouring glyphs from bleeding into each other under linear
// filtering.
constexpr int kPadding = 1;

} // namespace

void QTermGlyphAtlas::setFont(const QFont &font)
{
    if (m_font == font)
        return;

    m_font = font;

    for (int i = 0; i < StyleCount; ++i) {
        QFont styled = font;
        styled.setBold(i == Bold || i == BoldItalic);
        styled.setItalic(i == Italic || i == BoldItalic);
        m_faces[i] = QRawFont::fromFont(styled);
    }

    // The rasterised pixels depend on the size, so they go; the glyph index
    // lookups would survive a pure size change, but the faces have been
    // rebuilt here anyway.
    clear();
}

void QTermGlyphAtlas::clear()
{
    m_glyphs.clear();
    m_image = QImage();
    m_shelfX = 0;
    m_shelfY = 0;
    m_shelfHeight = 0;
    ++m_generation;
}

QRawFont &QTermGlyphAtlas::faceFor(Style style)
{
    return m_faces[style];
}

bool QTermGlyphAtlas::packInto(const QImage &rasterIn, QRect *region)
{
    // alphaMapForGlyph() hands back coverage, one byte per pixel. Converting it
    // to a colour format loses that -- Alpha8 to Grayscale8 in particular reads
    // as solid black -- so it is taken as raw bytes instead.
    const QImage raster = (rasterIn.depth() == 8)
            ? rasterIn
            : rasterIn.convertToFormat(QImage::Format_Alpha8);

    const int w = raster.width() + kPadding;
    const int h = raster.height() + kPadding;
    if (w > kAtlasWidth || h > kAtlasHeight)
        return false;

    if (m_image.isNull()) {
        // Premultiplied white with the glyph's coverage in alpha. An alpha-only
        // texture would halve the memory, but which channel it lands in differs
        // between RHI backends; this leaves no ambiguity for the shader.
        m_image = QImage(kAtlasWidth, kAtlasHeight, QImage::Format_RGBA8888_Premultiplied);
        m_image.fill(Qt::transparent);
    }

    if (m_shelfX + w > kAtlasWidth) {
        // Start a new shelf above the tallest glyph on the current one.
        m_shelfX = 0;
        m_shelfY += m_shelfHeight;
        m_shelfHeight = 0;
    }
    if (m_shelfY + h > kAtlasHeight)
        return false; // full; caller falls back to the general path

    const QRect target(m_shelfX, m_shelfY, raster.width(), raster.height());

    for (int y = 0; y < raster.height(); ++y) {
        const uchar *src = raster.constScanLine(y);
        auto *dst = reinterpret_cast<quint32 *>(m_image.scanLine(target.y() + y))
                    + target.x();
        for (int x = 0; x < raster.width(); ++x) {
            const quint32 coverage = src[x];
            dst[x] = coverage | (coverage << 8) | (coverage << 16) | (coverage << 24);
        }
    }

    m_shelfX += w;
    m_shelfHeight = qMax(m_shelfHeight, h);
    ++m_generation;

    *region = target;
    return true;
}

const QTermGlyphAtlas::Glyph *QTermGlyphAtlas::glyphFor(char32_t codePoint, Style style)
{
    const Key key{codePoint, style};
    if (const auto it = m_glyphs.constFind(key); it != m_glyphs.constEnd())
        return it->valid ? &it.value() : nullptr;

    Glyph glyph;
    const QString text = QString::fromUcs4(&codePoint, 1);

    // The configured font first, then any fallback face already discovered, and
    // only then ask Qt to resolve a new one. A monospaced terminal font usually
    // has no CJK coverage at all, so for those payloads the fallback is the
    // main path rather than an edge case -- but one resolution serves every
    // character that face covers.
    quint32 index = 0;
    QRawFont face = faceFor(style);
    if (!face.isValid() || (index = firstGlyphIndex(face, text)) == 0) {
        for (const QRawFont &candidate : std::as_const(m_fallbacks[style])) {
            if ((index = firstGlyphIndex(candidate, text)) != 0) {
                face = candidate;
                break;
            }
        }
    }
    if (index == 0) {
        const QRawFont resolved = resolveFallback(text, style);
        if ((index = firstGlyphIndex(resolved, text)) != 0) {
            m_fallbacks[style].append(resolved);
            face = resolved;
        }
    }

    // A colour-emoji face keeps its artwork in CBDT/sbix tables, which
    // alphaMapForGlyph() cannot reach -- it returns only a coverage mask, and
    // the glyph would render as a white silhouette. Those go to the general
    // path, which knows how to draw them in colour.
    if (index != 0 && isColorFont(face))
        index = 0;

    // Index 0 is .notdef: nothing anywhere has this character, so the caller
    // falls back to the general text path.
    if (index != 0) {
        const QImage raster = face.alphaMapForGlyph(index, QRawFont::PixelAntialiasing);
        if (!raster.isNull()) {
            QRect region;
            if (packInto(raster, &region)) {
                // boundingRect() is relative to the pen position on the
                // baseline, which is exactly the offset the geometry needs.
                glyph.region = region;
                glyph.bearing = face.boundingRect(index).topLeft();
                glyph.valid = true;
            }
        }
    }

    const auto inserted = m_glyphs.insert(key, glyph);
    return inserted->valid ? &inserted.value() : nullptr;
}

bool QTermGlyphAtlas::isColorFont(const QRawFont &face)
{
    static const QStringList colorFamilies = {
        QStringLiteral("Apple Color Emoji"),
        QStringLiteral("Segoe UI Emoji"),
        QStringLiteral("Noto Color Emoji"),
    };
    return colorFamilies.contains(face.familyName());
}

quint32 QTermGlyphAtlas::firstGlyphIndex(const QRawFont &face, const QString &text)
{
    if (!face.isValid())
        return 0;
    const QList<quint32> indexes = face.glyphIndexesForString(text);
    return indexes.size() == 1 ? indexes.first() : 0;
}

QRawFont QTermGlyphAtlas::resolveFallback(const QString &text, Style style)
{
    // Rather than maintaining a list of candidate fonts per script, lay the
    // character out once and see which face Qt itself picked. That is the same
    // choice the general text path would make, so the two stay consistent, and
    // it costs one layout per newly seen script.
    QFont styled = m_font;
    styled.setBold(style == Bold || style == BoldItalic);
    styled.setItalic(style == Italic || style == BoldItalic);

    QTextLayout layout(text, styled);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    if (!line.isValid()) {
        layout.endLayout();
        return QRawFont();
    }
    line.setLineWidth(10000);
    layout.endLayout();

    const QList<QGlyphRun> runs = line.glyphRuns();
    return runs.isEmpty() ? QRawFont() : runs.first().rawFont();
}

} // namespace QTerm
