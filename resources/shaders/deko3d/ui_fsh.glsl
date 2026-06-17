#version 460

layout(location = 0) in vec4 vertexColor;
layout(location = 1) in vec2 texCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D uTexture;

void main()
{
    outColor = texture(uTexture, texCoord) * vertexColor;
}
