// Claude Generated - Phase 3A - SSAO Blur Fragment Shader for Noise Reduction
#version 430

in VS_OUT {
    vec2 texCoord;
} fs_in;

uniform sampler2D ssaoTexture;

out vec4 FragColor;

// Gaussian blur kernel (5x5)
const float kernel[25] = float[](
    1.0/273.0, 4.0/273.0,  7.0/273.0,  4.0/273.0, 1.0/273.0,
    4.0/273.0, 16.0/273.0, 26.0/273.0, 16.0/273.0, 4.0/273.0,
    7.0/273.0, 26.0/273.0, 41.0/273.0, 26.0/273.0, 7.0/273.0,
    4.0/273.0, 16.0/273.0, 26.0/273.0, 16.0/273.0, 4.0/273.0,
    1.0/273.0, 4.0/273.0,  7.0/273.0,  4.0/273.0, 1.0/273.0
);

void main()
{
    vec2 texelSize = 1.0 / textureSize(ssaoTexture, 0);
    vec3 blurred = vec3(0.0);
    int index = 0;

    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            blurred += texture(ssaoTexture, fs_in.texCoord + offset).rgb * kernel[index];
            index++;
        }
    }

    FragColor = vec4(blurred, 1.0);
}
