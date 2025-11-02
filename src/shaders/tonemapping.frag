// tonemapping.frag - HDR to SDR tone mapping with exposure control
// Claude Generated - Phase 5B: Bloom post-processing (HDR tone mapping)
#version 430 core

in vec2 texCoord;

uniform sampler2D hdrTexture;      // HDR input (RGBA16F)
uniform float exposure;            // Exposure adjustment (0.5-3.0)

out vec4 FragColor;

/**
 * Reinhard Tone Mapping Operator
 *
 * Converts HDR (floating-point) color values to SDR (0-1 LDR).
 * Uses Reinhard's global tone mapping operator:
 *
 *   Ldr = Hdr * exposure / (1.0 + Hdr * exposure)
 *
 * This preserves detail in bright areas while compressing dynamic range.
 * Simple, fast, and perceptually pleasing.
 *
 * @param hdr Floating-point HDR color value
 * @param exposure Exposure compensation multiplier
 * @return LDR color in range [0, 1]
 */
vec3 reinhardToneMapping(vec3 hdr, float exposure)
{
    // Apply exposure first
    vec3 mapped = hdr * exposure;

    // Reinhard operator: map to [0, 1]
    mapped = mapped / (vec3(1.0) + mapped);

    return mapped;
}

/**
 * Gamma Correction
 *
 * Converts linear RGB to sRGB gamma space for display.
 * sRGB gamma: 1/2.2 ≈ 0.454545
 */
vec3 gammaCorrect(vec3 linear)
{
    return pow(linear, vec3(1.0 / 2.2));
}

void main()
{
    // Sample HDR texture (may have values > 1.0)
    vec3 hdrColor = texture(hdrTexture, texCoord).rgb;

    // Apply Reinhard tone mapping
    vec3 mapped = reinhardToneMapping(hdrColor, exposure);

    // Apply gamma correction for display
    vec3 gammaCorrected = gammaCorrect(mapped);

    // Clamp to valid SDR range just in case
    gammaCorrected = clamp(gammaCorrected, vec3(0.0), vec3(1.0));

    FragColor = vec4(gammaCorrected, 1.0);
}
