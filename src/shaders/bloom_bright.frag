// bloom_bright.frag - Bright pass extraction for bloom effect
// Claude Generated - Phase 5B: Bloom post-processing (bright pixel detection)
#version 430 core

in vec2 texCoord;

uniform sampler2D colorTexture;    // Input scene color
uniform float bloomThreshold;      // Brightness threshold (0.5-1.5)

out vec4 FragColor;

/**
 * Bloom Bright Pass Shader
 *
 * Extracts pixels brighter than the threshold for bloom effect.
 * Uses relative luminance calculation (ITU-R BT.709):
 *   L = 0.2126*R + 0.7152*G + 0.0722*B
 *
 * This creates a soft bloom effect by upsampling bright regions
 * and applying Gaussian blur before composite.
 */
void main()
{
    // Sample input color
    vec4 color = texture(colorTexture, texCoord);

    // Calculate relative luminance (ITU-R BT.709)
    float luminance = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));

    // Extract bright pixels above threshold
    // Soft knee: smooth transition around threshold
    float softness = 0.5;  // Width of soft knee transition
    float knee = smoothstep(bloomThreshold - softness, bloomThreshold + softness, luminance);

    // Output bright color or black
    FragColor = vec4(color.rgb * knee, color.a);
}
