#version 440

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;

layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D atlasTexture;

void main()
{
    // The atlas holds premultiplied white with the glyph's coverage in alpha,
    // and vColor arrives premultiplied too, so one multiply gives the result.
    fragColor = vColor * texture(atlasTexture, vTexCoord).a;
}
