#ifndef QTERM_QTERMTEXTMATERIAL_H
#define QTERM_QTERMTEXTMATERIAL_H

#include <QSGGeometry>
#include <QSGMaterial>
#include <QSGTexture>

namespace QTerm {

/*
    Draws glyph quads sampled from QTermGlyphAtlas.

    Qt ships materials for a texture (QSGTextureMaterial) and for per-vertex
    colour (QSGVertexColorMaterial), but a terminal needs both at once: one
    texture holding coverage, and a colour that changes per cell. Hence a
    material of our own.

    Both the atlas and the vertex colours are premultiplied, so the fragment
    shader is a single multiply and the result composites with the standard
    source-over blend.
*/
class QTermTextMaterial : public QSGMaterial
{
public:
    struct Vertex
    {
        float x, y;
        float u, v;
        uchar r, g, b, a; // premultiplied
    };

    static const QSGGeometry::AttributeSet &attributes();

    QSGMaterialType *type() const override;
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode renderMode) const override;
    int compare(const QSGMaterial *other) const override;

    void setTexture(QSGTexture *texture) { m_texture = texture; }
    QSGTexture *texture() const { return m_texture; }

private:
    QSGTexture *m_texture = nullptr;
};

} // namespace QTerm

#endif // QTERM_QTERMTEXTMATERIAL_H
