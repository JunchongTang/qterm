#ifndef QTERM_QTERMGLYPHATLAS_H
#define QTERM_QTERMGLYPHATLAS_H

#include <QHash>
#include <QImage>
#include <QList>
#include <QRawFont>
#include <QRectF>
#include <QSize>

namespace QTerm {

/*
    Rasterised glyphs packed into a single texture, keyed by code point and
    style.

    A terminal is a fixed grid: every cell sits at column * cellWidth and no
    character may influence its neighbours. General text rendering cannot assume
    that, so QTextLayout shapes each run -- consulting the font's GSUB and GPOS
    tables to apply ligatures, kerning and script-specific reordering. None of
    that can change what a terminal draws, yet it is redone on every frame that
    rebuilds a text node. Measured against simply looking up glyph indices, that
    shaping costs 124x for ASCII and 52x for CJK.

    This class replaces it with a lookup: code point to glyph index to a
    rectangle in one shared texture.

    Two levels of caching, and they invalidate differently:

      - Code point to glyph index depends only on the font, not its size. The
        same "A" is the same glyph index at 12px and at 24px.
      - Glyph index to pixels depends on the size, so changing the font size
        does discard the packed image -- but not the index lookups, and a size
        change is a rare, explicit user action.

    Only the common case is handled here. Combining marks, colour emoji and
    characters the font has no glyph for return nullptr, and the caller is
    expected to fall back to the general text path for those.
*/
class QTermGlyphAtlas
{
public:
    struct Glyph
    {
        // Position within the atlas image, in pixels.
        QRect region;
        // Offset from the pen position to the top-left of region.
        QPointF bearing;
        bool valid = false;
    };

    // Style variants kept as separate faces, since bold and italic are
    // different glyph sets rather than transforms of the regular one.
    enum Style { Regular = 0, Bold = 1, Italic = 2, BoldItalic = 3, StyleCount = 4 };

    void setFont(const QFont &font);
    QFont font() const { return m_font; }

    // Returns nullptr when the glyph cannot be handled here: no glyph in the
    // font, an empty raster, or no room left in the atlas.
    const Glyph *glyphFor(char32_t codePoint, Style style);

    // A fully opaque block, so solid shapes (underlines, strike-through) can be
    // drawn by the same material and end up in the same draw call as the text.
    // Returns a null rect only if the atlas is full.
    QRect solidRegion();

    // Set once packing has failed for lack of room. The renderer checks this at
    // a frame boundary and calls clear(): resetting mid-frame would invalidate
    // the regions already written into the vertex buffer.
    bool isFull() const noexcept { return m_full; }

    // The packed image. Its generation counter changes whenever the contents
    // grow, which is the cue for the renderer to re-upload the texture.
    const QImage &image() const { return m_image; }
    quint32 generation() const noexcept { return m_generation; }

    void clear();

private:
    struct Key
    {
        char32_t codePoint;
        Style style;
        bool operator==(const Key &other) const noexcept
        {
            return codePoint == other.codePoint && style == other.style;
        }
    };
    friend size_t qHash(const Key &key, size_t seed) noexcept
    {
        return qHashMulti(seed, key.codePoint, int(key.style));
    }

    QRawFont &faceFor(Style style);
    bool packInto(const QImage &rasterIn, QRect *region);
    static quint32 firstGlyphIndex(const QRawFont &face, const QString &text);
    static bool isColorFont(const QRawFont &face);
    // Asks Qt which face it would use for this text, so the atlas and the
    // general text path agree on the substitution.
    QRawFont resolveFallback(const QString &text, Style style);

    QFont m_font;
    QRawFont m_faces[StyleCount];
    // Faces discovered for characters the configured font does not cover, kept
    // per style and tried in order before resolving a new one.
    QList<QRawFont> m_fallbacks[StyleCount];
    QHash<Key, Glyph> m_glyphs;

    QImage m_image;
    quint32 m_generation = 0;

    // Shelf packing: rows are filled left to right, a new row starts above the
    // tallest glyph so far. Terminal glyphs are all about one cell tall, so the
    // waste this leaves is negligible and it avoids a real bin packer.
    int m_shelfX = 0;
    int m_shelfY = 0;
    int m_shelfHeight = 0;
    bool m_full = false;
    QRect m_solidRegion;
};

} // namespace QTerm

#endif // QTERM_QTERMGLYPHATLAS_H
