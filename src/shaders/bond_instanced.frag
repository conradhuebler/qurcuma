#version 330

// Claude Generated - Phase 3.2: GPU instancing fragment shader for bonds.
// Phong-like lighting matching atom_instanced.frag for visual consistency.

in vec3 worldPos;
in vec3 worldNormal;
in vec4 instColor;

uniform mat4 inverseViewMatrix;

out vec4 fragColor;

void main()
{
    vec3 eyePos = inverseViewMatrix[3].xyz;
    vec3 lightPos = eyePos + vec3(50.0, 50.0, 50.0);

    vec3 N = normalize(worldNormal);
    vec3 L = normalize(lightPos - worldPos);
    vec3 V = normalize(eyePos - worldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 48.0);

    vec3 ambient = instColor.rgb * 0.25;
    vec3 diffuse = instColor.rgb * diff * 0.75;
    vec3 specular = vec3(0.25) * spec;

    fragColor = vec4(ambient + diffuse + specular, instColor.a);
}
