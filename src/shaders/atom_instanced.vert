#version 330

// Claude Generated - Phase 3.1: GPU instancing vertex shader for atoms.
// Base sphere mesh instanced N times with per-instance position/scale/color.

in vec3 vertexPosition;
in vec3 vertexNormal;

in vec3 instancePosition;
in float instanceScale;
in vec4 instanceColor;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

out vec3 worldPos;
out vec3 worldNormal;
out vec4 instColor;

void main()
{
    vec3 local = vertexPosition * instanceScale + instancePosition;
    vec4 world = modelMatrix * vec4(local, 1.0);
    worldPos = world.xyz;
    worldNormal = normalize(mat3(modelMatrix) * vertexNormal);
    instColor = instanceColor;
    gl_Position = projectionMatrix * viewMatrix * world;
}
