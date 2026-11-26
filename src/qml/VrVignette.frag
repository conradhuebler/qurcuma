// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Comfort vignette (Claude Generated 2026): transparent in the middle, darkening towards the edge of
// the field of view. Narrowing the peripheral view while the user glides (stick walking) reduces the
// visual-vestibular conflict behind motion sickness (Fernandes & Feiner, IEEE 3DUI 2016).
// Qt Quick 3D custom material, unshaded; inner, outer and strength are QML properties.
VARYING vec2 uv;

void MAIN()
{
    float r = length(uv - vec2(0.5)) * 2.0;   // 0 in the centre, 1 at the edge midpoints
    float a = smoothstep(inner, outer, r) * strength;
    FRAGCOLOR = vec4(0.0, 0.0, 0.0, a);
}
