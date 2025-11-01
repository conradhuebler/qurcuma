// Claude Generated - Phase 3A - SSAO Fragment Shader with Screen-Space Ambient Occlusion
#version 430

in VS_OUT {
    vec2 texCoord;
} fs_in;

// Uniforms
uniform sampler2D colorTexture;      // Input color texture
uniform sampler2D normalTexture;     // Normal vectors (from depth reconstruction)
uniform sampler2D depthTexture;      // Depth buffer
uniform sampler2D noiseTexture;      // Random noise for sample patterns
uniform mat4 projectionMatrix;

// SSAO Parameters
uniform float ssaoRadius = 0.5;      // Sampling radius in screen space
uniform float ssaoBias = 0.025;      // Bias to prevent artifacts
uniform float ssaoIntensity = 1.0;   // Occlusion darkness

// Output
out vec4 FragColor;

const int KERNEL_SIZE = 32;  // Number of samples for SSAO

// Pseudo-random number generator
float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

// Reconstruct position from depth
vec3 reconstructPosition(vec2 uv, float depth) {
    // Convert from normalized device coordinates to view space
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = inverse(projectionMatrix) * clipPos;
    return viewPos.xyz / viewPos.w;
}

// Calculate normal from depth texture (Sobel filter)
vec3 calculateNormal(vec2 uv) {
    float texelSize = 1.0 / textureSize(depthTexture, 0).x;

    float depth_c  = texture(depthTexture, uv).r;
    float depth_n  = texture(depthTexture, uv + vec2(0.0, texelSize)).r;
    float depth_s  = texture(depthTexture, uv - vec2(0.0, texelSize)).r;
    float depth_e  = texture(depthTexture, uv + vec2(texelSize, 0.0)).r;
    float depth_w  = texture(depthTexture, uv - vec2(texelSize, 0.0)).r;

    vec3 pos_c = reconstructPosition(uv, depth_c);
    vec3 pos_n = reconstructPosition(uv + vec2(0.0, texelSize), depth_n);
    vec3 pos_s = reconstructPosition(uv - vec2(0.0, texelSize), depth_s);
    vec3 pos_e = reconstructPosition(uv + vec2(texelSize, 0.0), depth_e);
    vec3 pos_w = reconstructPosition(uv - vec2(texelSize, 0.0), depth_w);

    vec3 dx = pos_e - pos_w;
    vec3 dy = pos_n - pos_s;

    return normalize(cross(dx, dy));
}

void main()
{
    // Get base color
    vec4 colorSample = texture(colorTexture, fs_in.texCoord);

    // Get depth and position
    float depth = texture(depthTexture, fs_in.texCoord).r;
    vec3 position = reconstructPosition(fs_in.texCoord, depth);

    // Calculate normal from depth
    vec3 normal = calculateNormal(fs_in.texCoord);

    // Get noise for randomization
    vec3 noise = normalize(texture(noiseTexture, fs_in.texCoord * 4.0).xyz * 2.0 - 1.0);

    // Create local coordinate system
    vec3 tangent = normalize(noise - normal * dot(noise, normal));
    vec3 bitangent = cross(normal, tangent);

    // SSAO Calculation
    float occlusion = 0.0;

    for (int i = 0; i < KERNEL_SIZE; ++i) {
        // Generate sample vector in hemisphere around normal
        float angle = 2.0 * 3.14159 * float(i) / float(KERNEL_SIZE);
        float rad = sqrt(float(i) / float(KERNEL_SIZE)) * ssaoRadius;

        vec3 sampleDir = vec3(cos(angle) * rad, sin(angle) * rad, sqrt(1.0 - rad * rad));

        // Transform to world space using TBN matrix
        vec3 sampleVector = sampleDir.x * tangent + sampleDir.y * bitangent + sampleDir.z * normal;
        vec3 samplePos = position + sampleVector;

        // Project sample position to screen space
        vec4 projectedPos = projectionMatrix * vec4(samplePos, 1.0);
        vec2 sampleTexCoord = (projectedPos.xy / projectedPos.w) * 0.5 + 0.5;

        // Check if sample is within bounds
        if (sampleTexCoord.x >= 0.0 && sampleTexCoord.x <= 1.0 &&
            sampleTexCoord.y >= 0.0 && sampleTexCoord.y <= 1.0) {

            // Get depth at sample point
            float sampleDepth = texture(depthTexture, sampleTexCoord).r;
            float sampleZ = reconstructPosition(sampleTexCoord, sampleDepth).z;

            // Check if sample point is occluded
            float distZ = position.z - sampleZ;

            if (distZ > ssaoBias && distZ < ssaoRadius * 10.0) {
                occlusion += 1.0;
            }
        }
    }

    // Normalize occlusion
    occlusion = 1.0 - (occlusion / float(KERNEL_SIZE) * ssaoIntensity);
    occlusion = pow(occlusion, 2.0);  // Smoothing curve

    // Apply occlusion to color
    vec3 result = colorSample.rgb * occlusion;

    FragColor = vec4(result, colorSample.a);
}
