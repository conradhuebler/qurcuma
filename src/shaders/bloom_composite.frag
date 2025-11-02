// bloom_composite.frag - Composite bloom effect with scene
// Claude Generated - Phase 5B: Bloom post-processing (final composite)
#version 430 core

in vec2 texCoord;

uniform sampler2D sceneTexture;    // Original scene color
uniform sampler2D bloomTexture;    // Blurred bloom texture
uniform float bloomIntensity;      // Bloom strength (0.0-2.0)

out vec4 FragColor;

/**
 * Bloom Composite Shader
 *
 * Additive blending of bloom texture with scene.
 * Creates the classic "glowing light" effect on bright areas.
 *
 * Formula: result = scene + bloom * intensity
 */
void main()
{
    vec4 sceneColor = texture(sceneTexture, texCoord);
    vec4 bloomColor = texture(bloomTexture, texCoord);

    // Additive blending: scene + bloom * intensity
    vec3 result = sceneColor.rgb + bloomColor.rgb * bloomIntensity;

    // Clamp to prevent excessive over-saturation
    result = min(result, vec3(2.0));

    FragColor = vec4(result, sceneColor.a);
}
