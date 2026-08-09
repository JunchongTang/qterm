#version 440

// vec2, matching the two floats the geometry supplies. Declaring it vec4 works
// on OpenGL, which pads missing components, but Metal rejects the mismatch.
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec4 color;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec4 vColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

void main()
{
    gl_Position = qt_Matrix * vec4(position, 0.0, 1.0);
    vTexCoord = texCoord;
    vColor = color * qt_Opacity;
}
