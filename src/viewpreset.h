// viewpreset.h - Camera + display preset for reproducible molecular views.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Reproducible camera/display presets for Qurcuma.

#pragma once

#include <QColor>
#include <QQuaternion>
#include <QVector3D>
#include <QString>

#include "displaysettings.h"

/** @brief How the stored camera distance is interpreted when loading a preset.
 *
 *  - Absolute: the stored `cameraDistance` is applied verbatim. Identical only
 *    for molecules of the same size.
 *  - Relative: the camera distance is reconstructed as `zoomFactor *
 *    sceneExtent` of the *current* molecule, so the molecule appears at the
 *    same on-screen size regardless of its real extent.
 */
enum class ZoomMode {
    Absolute = 0,
    Relative = 1
};

/** @brief A reproducible view preset: camera orientation plus the display
 *  settings that should be applied together for uniform figures.
 *
 *  The camera distance is stored in two forms: `cameraDistance` (absolute)
 *  and `zoomFactor` (relative, = cameraDistance / sceneExtent at capture
 *  time). `zoomMode` selects which one is used on load.
 *
 *  Claude Generated 2026.
 */
struct ViewPreset : public DisplaySettings {
    QString name;

    // --- camera (SceneController transform) ---
    QQuaternion rootRotation;    // molecule / scene rotation
    QVector3D pan;               // pivot offset relative to sceneCenter
    float fieldOfView = 45.0f; // vertical field of view in degrees

    float sceneExtent = 0.0f;     // reference bounding radius at capture time
    float cameraDistance = 0.0f;  // absolute camera distance
    float zoomFactor = 3.0f;      // relative zoom = cameraDistance / sceneExtent
    ZoomMode zoomMode = ZoomMode::Absolute;

    // --- display / appearance ---
    // The shared appearance fields (rendering mode, colours, effects, walls, ...)
    // are inherited from DisplaySettings. Only the preset-specific extras below
    // are declared here. Claude Generated 2026.
    float fogDistance = 0.2f;
    QColor backgroundColor = QColor(32, 36, 44);
    bool cornerLightEnabled[4] = { true, true, false, false };

    bool operator==(const ViewPreset& other) const
    {
        return name == other.name;
    }
};