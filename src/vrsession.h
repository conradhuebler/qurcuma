/*
 * <Qurcuma VR session: mirrors the viewer scene into an OpenXR headset.>
 * Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <QObject>
#include <QQmlEngine>

#include <memory>

class SceneController;

/**
 * @brief OpenXR session showing the current molecule in a headset (Claude Generated 2026).
 *
 * Owns a second SceneController (instancing buffers belong to one Quick 3D scene, so the
 * desktop controller cannot be shared) and a QML engine hosting src/qml/viewerxr.qml
 * (Qt Quick 3D XR). The viewer mirrors structure, positions and selection into scene();
 * a controller-ray pick in the headset arrives as atomPicked().
 */
class VrSession : public QObject {
    Q_OBJECT
public:
    explicit VrSession(QObject* parent = nullptr);
    ~VrSession() override;

    /// Create the XR view. Returns false if the QML could not be loaded.
    bool start();
    /// Tear the XR view down (ends the OpenXR session).
    void stop();
    bool isActive() const { return m_engine != nullptr; }

    /// Scene the headset renders; feed it like the desktop SceneController.
    SceneController* scene() const { return m_scene; }

signals:
    /// Instance index of the atom hit by the controller ray (-1 = nothing hit).
    void atomPicked(int atomIndex);
    /// The OpenXR session ended (headset removed, runtime stopped, or stop()).
    void ended();

private:
    SceneController* m_scene = nullptr;
    std::unique_ptr<QQmlEngine> m_engine;
    QObject* m_root = nullptr;
};
