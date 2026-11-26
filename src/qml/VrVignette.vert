// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Comfort vignette (Claude Generated 2026): hands the texture coordinate to the fragment shader
// (UV0 exists only here). An unshaded material must write POSITION itself; under stereo (multiview)
// rendering Qt indexes MODELVIEWPROJECTION_MATRIX by the view on its own.
VARYING vec2 uv;

void MAIN()
{
    uv = UV0;
    POSITION = MODELVIEWPROJECTION_MATRIX * vec4(VERTEX, 1.0);
}
