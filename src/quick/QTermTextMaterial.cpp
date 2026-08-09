#include "QTermTextMaterial.h"

#include <QSGMaterialShader>

// qterm is a static library, so nothing references the generated resource
// initialiser and the linker drops it -- the shaders then fail to load at run
// time with no build-time complaint. Calling it explicitly keeps them.
static void qtermInitTextShaderResources()
{
    Q_INIT_RESOURCE(qterm_text_shaders);
}

namespace QTerm {

namespace {

class QTermTextShader : public QSGMaterialShader
{
public:
    QTermTextShader()
    {
        qtermInitTextShaderResources();
        setShaderFileName(VertexStage, QStringLiteral(":/qt/qml/QTerm/qterm_text.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/qt/qml/QTerm/qterm_text.frag.qsb"));
    }

    bool updateUniformData(RenderState &state, QSGMaterial *, QSGMaterial *) override
    {
        bool changed = false;
        QByteArray *buffer = state.uniformData();
        Q_ASSERT(buffer->size() >= 68);

        if (state.isMatrixDirty()) {
            const QMatrix4x4 matrix = state.combinedMatrix();
            memcpy(buffer->data(), matrix.constData(), 64);
            changed = true;
        }
        if (state.isOpacityDirty()) {
            const float opacity = state.opacity();
            memcpy(buffer->data() + 64, &opacity, 4);
            changed = true;
        }
        return changed;
    }

    void updateSampledImage(RenderState &state, int binding, QSGTexture **texture,
                            QSGMaterial *newMaterial, QSGMaterial *) override
    {
        if (binding != 1)
            return;
        auto *material = static_cast<QTermTextMaterial *>(newMaterial);
        QSGTexture *atlas = material->texture();
        if (!atlas)
            return;
        atlas->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
        *texture = atlas;
    }
};

} // namespace

const QSGGeometry::AttributeSet &QTermTextMaterial::attributes()
{
    static QSGGeometry::Attribute data[] = {
        QSGGeometry::Attribute::createWithAttributeType(
            0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
        QSGGeometry::Attribute::createWithAttributeType(
            1, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute),
        // Location 2, matching the shader. Getting this wrong does not fail to
        // build or draw -- it silently renders nothing.
        QSGGeometry::Attribute::createWithAttributeType(
            2, 4, QSGGeometry::UnsignedByteType, QSGGeometry::ColorAttribute),
    };
    static QSGGeometry::AttributeSet set = { 3, sizeof(Vertex), data };
    return set;
}

QSGMaterialType *QTermTextMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader *QTermTextMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new QTermTextShader;
}

int QTermTextMaterial::compare(const QSGMaterial *other) const
{
    const auto *o = static_cast<const QTermTextMaterial *>(other);
    if (m_texture == o->m_texture)
        return 0;
    return m_texture < o->m_texture ? -1 : 1;
}

} // namespace QTerm
