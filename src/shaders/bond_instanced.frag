#version 330

// Claude Generated - Phase 3.2: GPU instancing fragment shader for bonds.
// Phong-like lighting matching atom_instanced.frag for visual consistency.

in vec3 worldPos;
in vec3 worldNormal;
in vec4 instColor;

uniform vec3 cameraPosition;
uniform mat4 viewMatrix;

// Claude Generated 2026 - 4 screen-fixed corner lights, matching atom_instanced.frag.
// One component per screen corner (◤ x, ◥ y, ◣ z, ◢ w); 0 = off, >0 = intensity.
uniform vec4 cornerLightEnabled = vec4(1.0, 1.0, 0.0, 0.0);

// Claude Generated 2026 - Phase 1 Fog: exponential squared distance fog
uniform float fogEnabled = 0.0;
uniform vec3  fogColor   = vec3(0.125, 0.141, 0.172);
uniform float fogDensity = 0.015;

out vec4 fragColor;

void main()
{
    // View-space lighting: the lit zone stays fixed to the screen regardless of
    // model rotation (matches atom_instanced.frag for visual consistency).
    vec3 N = normalize(worldNormal);
    vec3 Nview = normalize((viewMatrix * vec4(N, 0.0)).xyz);

    vec3 cornerDir[4] = vec3[4](
        normalize(vec3(-0.6,  0.6, 0.5)),  // ◤ top-left
        normalize(vec3( 0.6,  0.6, 0.5)),  // ◥ top-right
        normalize(vec3(-0.6, -0.6, 0.5)),  // ◣ bottom-left
        normalize(vec3( 0.6, -0.6, 0.5))   // ◢ bottom-right
    );
    vec3 toCam = vec3(0.0, 0.0, 1.0);

    float diff = 0.0;
    float spec = 0.0;
    for (int i = 0; i < 4; ++i) {
        float w = cornerLightEnabled[i];
        if (w <= 0.0) continue;
        vec3 L = cornerDir[i];
        vec3 H = normalize(L + toCam);
        diff += w * max(dot(Nview, L), 0.0);
        spec += w * pow(max(dot(Nview, H), 0.0), 48.0);
    }

    vec3 ambient = instColor.rgb * 0.25;
    vec3 diffuse = instColor.rgb * diff * 0.5;
    vec3 specular = vec3(0.2) * spec;

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
