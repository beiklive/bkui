#version 460

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 vertexColor;
layout(location = 1) out vec2 texCoord;

void main()
{
    vertexColor = inColor;
    texCoord = inTexCoord;
    gl_Position = vec4(inPosition, 0.0, 1.0);
}
