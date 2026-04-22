#version 330

// Claude Generated - Phase 3.1: GPU instancing fragment shader for atoms.
// Phong-like lighting with per-instance diffuse color.

in vec3 worldPos;
in vec3 worldNormal;
in vec4 instColor;

uniform vec3 cameraPosition;
uniform mat4 viewMatrix;

out vec4 fragColor;

void main()
{
    // Key light in view space: lighting stays fixed relative to camera
    // regardless of model rotation, matching the feel of camera-orbit rotation.
    vec3 N = normalize(worldNormal);
    vec3 V = normalize(cameraPosition - worldPos);
    // Transform normal to view space for headlight-style lighting
    vec3 Nview = normalize((viewMatrix * vec4(N, 0.0)).xyz);
    vec3 Lview = normalize(vec3(0.3, 0.7, 0.5));  // view-space key light
    vec3 Hview = normalize(Lview + vec3(0.0, 0.0, 1.0));  // half-vector toward camera

    float diff = max(dot(Nview, Lview), 0.0);
    float spec = pow(max(dot(Nview, Hview), 0.0), 64.0);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 64.0);

    vec3 ambient = instColor.rgb * 0.2;
    vec3 diffuse = instColor.rgb * diff * 0.8;
    vec3 specular = vec3(0.35) * spec;

    fragColor = vec4(ambient + diffuse + specular, instColor.a);
}
