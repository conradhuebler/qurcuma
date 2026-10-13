// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Offline preview of the headset scene (Claude Generated 2026): src/qml/VrScene.qml with MockVr and
// MockMolecule, seen from a head at the origin looking along -Z (the headset's eye camera, without
// stereo or lens distortion). Left click operates the panels, right drag looks around, the wheel
// changes the field of view. Started by main.cpp, which sets the properties below.
import QtQuick
import QtQuick3D
import "../../src/qml"

Item {
    id: root

    property string stateName: "idle"
    property real fieldOfView: 100   // horizontal, degrees
    property real lookYaw: 0         // degrees, > 0 = left
    property real lookPitch: -12     // degrees, > 0 = up

    width: 1600
    height: 1000

    MockVr {
        id: mockVr
        stateName: root.stateName
    }

    View3D {
        id: view
        anchors.fill: parent
        camera: head
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Color
            clearColor: "#1c2230"
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        PerspectiveCamera {
            id: head
            position: Qt.vector3d(0, 0, 0)
            eulerRotation: Qt.vector3d(root.lookPitch, root.lookYaw, 0)
            fieldOfView: root.fieldOfView
            fieldOfViewOrientation: PerspectiveCamera.Horizontal
            clipNear: 1
            clipFar: 10000
        }

        MockMolecule { id: mockMolecule }

        VrScene {
            id: scene
            session: mockVr
            sceneController: mockMolecule
            headPosition: Qt.vector3d(0, 0, 0)
            filesVisible: root.stateName === "files"
            onRecenterRequested: headYaw = root.lookYaw
        }

        DragHandler {
            property real startYaw
            property real startPitch
            acceptedButtons: Qt.RightButton
            target: null
            onActiveChanged: if (active) { startYaw = root.lookYaw; startPitch = root.lookPitch }
            onTranslationChanged: {
                root.lookYaw = startYaw + translation.x * 0.1
                root.lookPitch = Math.max(-80, Math.min(80, startPitch + translation.y * 0.1))
            }
        }
        WheelHandler {
            target: null
            onWheel: (event) => root.fieldOfView = Math.max(20, Math.min(140, root.fieldOfView - event.angleDelta.y / 40))
        }
    }

    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 8
        text: "preview  state: " + root.stateName + "   head at origin, yaw " + root.lookYaw.toFixed(0)
              + "°, pitch " + root.lookPitch.toFixed(0) + "°, horizontal FOV " + root.fieldOfView.toFixed(0) + "°"
        color: "#8090a0"
        font.pixelSize: 13
    }
}
