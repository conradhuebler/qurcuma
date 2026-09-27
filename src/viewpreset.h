// viewpreset.h - Camera + display preset for reproducible molecular views.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Reproducible camera/display presets for Qurcuma.

#pragma once

#include <QColor>
#include <QQuaternion>
#include <QVector3D>
#include <QString>


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

/** @brief A reproducible camera view ("View"): orientation, pan, field of view and
 *  zoom. Since UX stage 3 it carries no display settings; how the scene looks is a
 *  Look (look.h), chosen independently.
 *
 *  The camera distance is stored in two forms: `cameraDistance` (absolute)
 *  and `zoomFactor` (relative, = cameraDistance / sceneExtent at capture
 *  time). `zoomMode` selects which one is used on load.
 *
 *  Claude Generated 2026.
 */
struct ViewPreset {
    QString name;

    // --- camera (SceneController transform) ---
    QQuaternion rootRotation;    // molecule / scene rotation
    QVector3D pan;               // pivot offset relative to sceneCenter
    float fieldOfView = 45.0f; // vertical field of view in degrees

    float sceneExtent = 0.0f;     // reference bounding radius at capture time
    float cameraDistance = 0.0f;  // absolute camera distance
    float zoomFactor = 3.0f;      // relative zoom = cameraDistance / sceneExtent
    ZoomMode zoomMode = ZoomMode::Absolute;

    bool operator==(const ViewPreset& other) const
    {
        return name == other.name;
    }
};

/** @brief Camera orientation of the quick views: 0 = front (looking down -Z),
 *  1 = top (down the Y axis), 2 = side (along the X axis). Any other value gives
 *  the front view. Shared by the Appearance dock's buttons and View ▸ Views.
 *  Claude Generated 2026. */
inline QQuaternion quickViewOrientation(int axis)
{
    switch (axis) {
    case 1:
        return QQuaternion::fromEulerAngles(-90.0f, 0.0f, 0.0f);
    case 2:
        return QQuaternion::fromEulerAngles(0.0f, 90.0f, 0.0f);
    default:
        return QQuaternion();
    }
}
