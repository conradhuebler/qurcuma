// blur_horizontal.frag - Horizontal Gaussian blur for bloom/post-processing
// Claude Generated - Phase 5B: Bloom post-processing (horizontal blur)
#version 430 core

in vec2 texCoord;

uniform sampler2D inputTexture;    // Input texture to blur
uniform vec2 texelSize;            // 1.0 / texture dimensions

out vec4 FragColor;

/**
 * Horizontal Gaussian Blur Shader
 *
 * 7-tap Gaussian blur in horizontal direction
 * Kernel weights: [0.0625, 0.25, 0.375, 0.25, 0.0625]
 * This is a fast separable blur (H then V) for better performance
 *
 * For proper 2D blur, combine with blur_vertical.frag
 */
void main()
{
    // Gaussian kernel weights (7-tap, normalized)
    float weights[4] = float[](
        0.0625,   // ±3 pixels
        0.25,     // ±2 pixels
        0.375,    // ±1 pixel
        0.25      // ±0 pixels (center, applied twice as the 4th weight)
    );

    vec4 result = vec4(0.0);

    // Center tap
    result += texture(inputTexture, texCoord) * weights[3] * 2.0;

    // Symmetric taps
    for (int i = 1; i < 4; ++i) {
        float offset = float(i);
        vec2 coord1 = texCoord + vec2(offset * texelSize.x, 0.0);
        vec2 coord2 = texCoord - vec2(offset * texelSize.x, 0.0);

        result += texture(inputTexture, coord1) * weights[4 - i];
        result += texture(inputTexture, coord2) * weights[4 - i];
    }

    FragColor = result;
}
