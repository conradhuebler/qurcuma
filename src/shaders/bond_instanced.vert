#version 330

// Claude Generated - Phase 3.2: GPU instancing vertex shader for bonds.
// Base cylinder: axis = +Y, y in [-1, +1], radius = 1.
// Per-instance: center + halfLength, quaternion rotation, color (rgba), radius.

in vec3 vertexPosition;
in vec3 vertexNormal;

in vec4 instancePosLen;     // xyz=center, w=halfLength
in vec4 instanceQuat;       // xyz + w quaternion
in vec4 instanceColor;      // rgba
in vec4 instanceRadius;     // x=radius, yzw=pad

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

out vec3 worldPos;
out vec3 worldNormal;
out vec4 instColor;

vec3 rotateByQuat(vec4 q, vec3 v)
{
    vec3 t = 2.0 * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

void main()
{
    float r = instanceRadius.x;
    vec3 scaled = vec3(vertexPosition.x * r,
                       vertexPosition.y * instancePosLen.w,
                       vertexPosition.z * r);
    vec3 rotated = rotateByQuat(instanceQuat, scaled);
    vec3 localPos = rotated + instancePosLen.xyz;

    vec4 world = modelMatrix * vec4(localPos, 1.0);
    worldPos = world.xyz;

    vec3 rotNormal = rotateByQuat(instanceQuat, normalize(vertexNormal));
    worldNormal = normalize(mat3(modelMatrix) * rotNormal);

    instColor = instanceColor;
    gl_Position = projectionMatrix * viewMatrix * world;
}
