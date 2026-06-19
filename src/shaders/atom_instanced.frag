#version 330

// Claude Generated - Phase 3.1: GPU instancing fragment shader for atoms.
// Phong-like lighting with per-instance diffuse color.

in vec3 worldPos;
in vec3 worldNormal;
in vec4 instColor;

uniform vec3 cameraPosition;
uniform mat4 viewMatrix;

// Claude Generated 2026 - 4 screen-fixed corner lights. One component per screen
// corner (◤ x, ◥ y, ◣ z, ◢ w); 0 = off, >0 = intensity. Driven from the 2×2
// control grid via AtomInstancingSystem::setCornerLightIntensities().
uniform vec4 cornerLightEnabled = vec4(1.0, 1.0, 0.0, 0.0);

// Claude Generated 2026 - Phase 1 Fog: exponential squared distance fog
uniform float fogEnabled = 0.0;
uniform vec3  fogColor   = vec3(0.125, 0.141, 0.172);
uniform float fogDensity = 0.015;

out vec4 fragColor;

void main()
{
    // Lighting is computed in VIEW space, so the lit zone stays fixed to the
    // screen: "lamp top-left" always lights the top-left of the view, no matter
    // how the molecule (model matrix) is rotated underneath.
    vec3 N = normalize(worldNormal);
    vec3 Nview = normalize((viewMatrix * vec4(N, 0.0)).xyz);

    // View-space directions toward each screen corner, tilted toward the camera
    // (+z) so front-facing surfaces are lit. Index matches cornerLightEnabled.
    vec3 cornerDir[4] = vec3[4](
        normalize(vec3(-0.6,  0.6, 0.5)),  // ◤ top-left
        normalize(vec3( 0.6,  0.6, 0.5)),  // ◥ top-right
        normalize(vec3(-0.6, -0.6, 0.5)),  // ◣ bottom-left
        normalize(vec3( 0.6, -0.6, 0.5))   // ◢ bottom-right
    );
    vec3 toCam = vec3(0.0, 0.0, 1.0);  // view-space direction toward camera

    float diff = 0.0;
    float spec = 0.0;
    for (int i = 0; i < 4; ++i) {
        float w = cornerLightEnabled[i];
        if (w <= 0.0) continue;
        vec3 L = cornerDir[i];
        vec3 H = normalize(L + toCam);
        diff += w * max(dot(Nview, L), 0.0);
        spec += w * pow(max(dot(Nview, H), 0.0), 64.0);
    }

    vec3 ambient = instColor.rgb * 0.22;
    vec3 diffuse = instColor.rgb * diff * 0.55;
    vec3 specular = vec3(0.3) * spec;

    vec3 color = ambient + diffuse + specular;

    // Claude Generated 2026 - Phase 1 Fog blend
    if (fogEnabled > 0.5) {
        float d = length(cameraPosition - worldPos);
        float f = exp(-pow(d * fogDensity, 2.0));
        f = clamp(f, 0.0, 1.0);
        color = mix(fogColor, color, f);
    }

    fragColor = vec4(color, instColor.a);
}
