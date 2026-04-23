#version 330

// PBR Fragment Shader - Claude Generated Phase 4A
// Cook-Torrance BRDF with Fresnel, GGX Distribution, and Geometry Shadowing
// Reference: "Real Shading in Unreal Engine 4" - Epic Games
// "Physically Based Rendering" - Pharr, Jakob, Humphreys

in vec3 worldPosition;
in vec3 worldNormal;
in vec3 vertColor;

// Material parameters
uniform vec3 baseColor;        // Albedo color
uniform float metallic;        // 0.0 = dielectric, 1.0 = metal
uniform float roughness;       // 0.0 = smooth, 1.0 = rough
uniform float ambientOcclusion;// 0.0 = dark, 1.0 = full AO

// Lighting
uniform vec3 lightPosition;    // World space light position
uniform vec3 lightColor;       // Light color and intensity
uniform vec3 cameraPosition;   // Camera/eye position

// Claude Generated 2026 - Phase 1 Fog: exponential squared distance fog
uniform float fogEnabled = 0.0;  // 0.0 = off, 1.0 = on
uniform vec3  fogColor   = vec3(0.125, 0.141, 0.172);
uniform float fogDensity = 0.015;

out vec4 fragColor;

// Constants
const float PI = 3.14159265359;
const float EPSILON = 0.00001;

// Fresnel-Schlick approximation
// Calculates how much light is reflected vs refracted at surface
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// GGX/Trowbridge-Reitz distribution function
// Models surface microstructure roughness (normal distribution)
float distributionGGX(vec3 N, vec3 H, float alpha)
{
    float a2 = alpha * alpha * alpha * alpha;  // alpha^4
    float denom = pow(max(dot(N, H), 0.0), 2.0) * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom + EPSILON);
}

// Schlick-GGX geometry shadowing function
// Models how microfacets block light (self-shadowing)
float geometrySchlickGGX(float NdotV, float k)
{
    float denom = NdotV * (1.0 - k) + k;
    return NdotV / (denom + EPSILON);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float k)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    float ggx1 = geometrySchlickGGX(NdotV, k);
    float ggx2 = geometrySchlickGGX(NdotL, k);

    return ggx1 * ggx2;
}

void main()
{
    // Normal and view vectors
    vec3 N = normalize(worldNormal);
    vec3 V = normalize(cameraPosition - worldPosition);
    vec3 L = normalize(lightPosition - worldPosition);
    vec3 H = normalize(V + L);

    // Material properties
    vec3 finalBaseColor = mix(vertColor, vertColor, 0.5) * baseColor;  // Use vertex color if available
    vec3 F0 = mix(vec3(0.04), finalBaseColor, metallic);  // Fresnel base (0.04 for dielectrics)

    // Cook-Torrance BRDF
    float distance = length(lightPosition - worldPosition);
    float attenuation = 1.0 / (distance * distance + 1.0);  // Simple inverse-square falloff
    vec3 radiance = lightColor * attenuation;

    // BRDF components
    float NdotV = max(dot(N, V), EPSILON);
    float NdotL = max(dot(N, L), EPSILON);
    float NdotH = max(dot(N, H), 0.0);

    // Fresnel reflection
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    // Roughness remapping for better distribution
    float alpha = roughness * roughness;
    alpha = alpha * alpha;  // Remapped for perceptual roughness

    // Geometry and distribution functions
    float D = distributionGGX(N, H, alpha);
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;  // Direct lighting k
    float G = geometrySmith(N, V, L, k);

    // Specular contribution (Cook-Torrance)
    vec3 specular = (D * F * G) / (4.0 * NdotV * NdotL + EPSILON);

    // Diffuse contribution (Lambertian)
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * finalBaseColor / PI;

    // Final radiance
    vec3 Lo = (diffuse + specular) * radiance * NdotL;

    // Ambient (simple)
    vec3 ambient = finalBaseColor * 0.03 * ambientOcclusion;

    // Tone mapping (simple Reinhard)
    vec3 color = ambient + Lo;
    color = color / (color + vec3(1.0));

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    // Claude Generated 2026 - Phase 1 Fog blend (exp squared)
    if (fogEnabled > 0.5) {
        float d = length(cameraPosition - worldPosition);
        float f = exp(-pow(d * fogDensity, 2.0));
        f = clamp(f, 0.0, 1.0);
        color = mix(fogColor, color, f);
    }

    fragColor = vec4(color, 1.0);
}
