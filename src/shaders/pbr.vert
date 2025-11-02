#version 330

// PBR Vertex Shader - Claude Generated Phase 4A
// Cook-Torrance BRDF Physically-Based Rendering
// Educational implementation with references to physical lighting equations

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec3 vertexColor;

// Uniform variables
uniform mat4 modelView;
uniform mat4 modelViewProjection;
uniform mat3 normalMatrix;

// Output to fragment shader
out vec3 worldPosition;
out vec3 worldNormal;
out vec3 vertColor;

void main()
{
    // Transform vertex to world space
    worldPosition = vec3(modelView * vec4(vertexPosition, 1.0));

    // Transform normal to world space (handles non-uniform scaling)
    worldNormal = normalize(normalMatrix * vertexNormal);

    // Pass color to fragment shader
    vertColor = vertexColor;

    // Project vertex to clip space
    gl_Position = modelViewProjection * vec4(vertexPosition, 1.0);
}
