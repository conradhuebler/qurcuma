// Claude Generated - Phase 3A - SSAO Vertex Shader
#version 430

in vec3 vertexPosition;
in vec2 vertexTexCoord;

out VS_OUT {
    vec2 texCoord;
} vs_out;

void main()
{
    gl_Position = vec4(vertexPosition, 1.0);
    vs_out.texCoord = vertexTexCoord;
}
