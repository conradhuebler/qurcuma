// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// qurcuma 3D viewer scene (Qt Quick 3D). "controller" = SceneController context
// property set by view.cpp. All transform/effect state lives in the controller and
// mouse/picking/grab are driven from C++; this file stays declarative. The molecule
// rotates under moleculeRoot (model rotation about sceneCenter) while the camera
// stays axis-aligned, so C++ can replicate the projection for picking/grab without
// the private QQuick3DViewport/QQuick3DCamera headers. Claude Generated.
import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

Item {
    id: root

    View3D {
        id: view3D
        anchors.fill: parent
        camera: camera

        environment: ExtendedSceneEnvironment {
            // Transparent clear for image export/compositing; otherwise the scene colour.
            backgroundMode: controller.transparentBackground
                ? SceneEnvironment.Transparent
                : SceneEnvironment.Color
            clearColor: controller.backgroundColor
            // SSAA/VeryHigh for high-quality image export; MSAA/High interactively.
            antialiasingMode: controller.highQualityAA
                ? SceneEnvironment.SSAA
                : SceneEnvironment.MSAA
            antialiasingQuality: controller.highQualityAA
                ? SceneEnvironment.VeryHigh
                : SceneEnvironment.High

            aoEnabled: controller.ssaoEnabled
            aoStrength: controller.ssaoStrength * 90
            aoDistance: 4.0
            aoSoftness: 30

            glowEnabled: controller.bloomEnabled
            glowStrength: 0.9
            glowIntensity: 0.7
            glowBloom: 0.4

            tonemapMode: controller.tonemapEnabled
                ? SceneEnvironment.TonemapModeFilmic
                : SceneEnvironment.TonemapModeNone

            // Optional depth fog: atoms farther from the camera fade into the
            // background. The near/far band follows the molecule's depth range so
            // the effect stays meaningful as you zoom. density = user-set strength.
            fog: Fog {
                enabled: controller.fogEnabled
                color: controller.backgroundColor
                density: controller.fogDensity   // strength
                depthEnabled: true
                // distance: fogDistance (0..1) slides where the fog starts, from the
                // front of the molecule (0) to its back (1); the far plane sits beyond.
                // distance slides the fog START from the molecule front (0) toward
                // its back (1); the band is kept tight (~0.8 extent) so everything
                // past it sits at full density -> the fade is strong, not washed out.
                depthNear: Math.max(0.1, controller.cameraDistance - controller.sceneExtent
                                         + controller.fogDistance * 1.5 * controller.sceneExtent)
                depthFar: depthNear + Math.max(0.5, controller.sceneExtent * 0.8)
                depthCurve: 1.0
            }
        }

        // Axis-aligned camera node: sits at pivotPosition (sceneCenter + pan), camera
        // child pulled back by cameraDistance. No rotation -> the molecule rotates, not
        // the camera (matches the former Qt3D "Model rotation" default).
        Node {
            id: cameraNode
            position: controller.pivotPosition
            PerspectiveCamera {
                id: camera
                z: controller.cameraDistance
                clipNear: 0.5
                clipFar: controller.cameraDistance * 8.0 + controller.sceneExtent * 8.0 + 1000.0
                fieldOfView: controller.fieldOfView

                // Screen-fixed corner lights (parented to camera; lit zone does not
                // rotate with the molecule) — matches the former Qt3D corner lights.
                DirectionalLight { eulerRotation.x: -30; eulerRotation.y: 30;  brightness: 0.7; visible: controller.cornerLight0; castsShadow: false }
                DirectionalLight { eulerRotation.x: -30; eulerRotation.y: -30; brightness: 0.7; visible: controller.cornerLight1; castsShadow: false }
                DirectionalLight { eulerRotation.x: 30;  eulerRotation.y: 30;  brightness: 0.7; visible: controller.cornerLight2; castsShadow: false }
                DirectionalLight { eulerRotation.x: 30;  eulerRotation.y: -30; brightness: 0.7; visible: controller.cornerLight3; castsShadow: false }
            }
        }

        // World-fixed key light -> world-fixed cast shadows.
        DirectionalLight {
            eulerRotation.x: -40
            eulerRotation.y: -50
            brightness: 0.6
            castsShadow: controller.shadowsEnabled
            shadowMapQuality: Light.ShadowMapQualityHigh
            shadowFactor: 80
        }

        // Floor (world-horizontal) to receive cast shadows.
        Model {
            source: "#Rectangle"
            visible: controller.shadowsEnabled
            position: Qt.vector3d(controller.sceneCenter.x,
                                  controller.sceneCenter.y - controller.sceneExtent * 1.1,
                                  controller.sceneCenter.z)
            eulerRotation.x: -90
            scale: Qt.vector3d(controller.sceneExtent * 0.3, controller.sceneExtent * 0.3, 1)
            materials: PrincipledMaterial { baseColor: "#3a4252"; roughness: 0.9 }
        }

        // Molecule root: rotates about sceneCenter (model rotation). All instanced content
        // (atoms, bonds, overlays, walls, NCI, grab arrows, measurement lines) is model-local
        // and lives in MoleculeModels.qml, which the headset scene (VrScene.qml) shares.
        Node {
            id: moleculeRoot
            position: controller.sceneCenter
            pivot: controller.sceneCenter
            rotation: controller.rootRotation

            MoleculeModels {
                ctl: controller
                inst: controller
            }
        }
    }

    // Rubber-band (box) selection overlay (2D, structure editing). Rect is in viewport
    // pixels supplied by C++; selection happens on mouse release.
    Rectangle {
        visible: controller.rubberBandActive
        x: controller.rubberBandRect.x
        y: controller.rubberBandRect.y
        width: controller.rubberBandRect.width
        height: controller.rubberBandRect.height
        color: "#2290e0ff"
        border.color: "#cc90e0ff"
        border.width: 1
    }

    // Edit-mode hint bar (2D overlay, bottom-centre). Empty editHint = hidden.
    Rectangle {
        visible: controller.editHint.length > 0
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 10 }
        width: Math.min(editHintText.implicitWidth + 18, root.width - 24)
        height: editHintText.implicitHeight + 8
        radius: 5
        color: "#cc101418"
        border.color: "#5affffff"
        Text {
            id: editHintText
            anchors.centerIn: parent
            width: parent.width - 16
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            color: "#d8e6ff"
            font.pixelSize: 12
            text: controller.editHint
        }
    }

    // Measurement result label (2D overlay).
    Rectangle {
        visible: controller.measurementActive
        anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 8 }
        width: measureText.width + 18
        height: measureText.height + 10
        radius: 6
        color: "#cc101418"
        border.color: "#5a90e0ff"
        Text {
            id: measureText
            anchors.centerIn: parent
            width: Math.min(implicitWidth, root.width - 40)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: "#aef0ff"
            font.pixelSize: 14
            font.bold: true
            font.family: "monospace"
            text: controller.measurementText
        }
    }

    // Per-atom overlay labels (element/type/index). C++ (SceneController::rebuildLabels)
    // supplies each atom's LOCAL position + text; we project to screen here so
    // rotation/zoom/pan reposition the labels reactively. The projection mirrors
    // SceneController's axis-aligned camera exactly (camera at pivotPosition +
    // (0,0,cameraDistance), looking down -Z, vertical FoV = fieldOfView).
    Repeater {
        model: controller.atomLabels
        delegate: Text {
            // atom position relative to the rotation centre (sceneCenter)
            readonly property real rx: modelData.px - controller.sceneCenter.x
            readonly property real ry: modelData.py - controller.sceneCenter.y
            readonly property real rz: modelData.pz - controller.sceneCenter.z
            // rotate (rx,ry,rz) by rootRotation: v' = v + w*t + q.xyz × t, t = 2·(q.xyz × v)
            readonly property quaternion q: controller.rootRotation
            readonly property real tx: 2 * (q.y * rz - q.z * ry)
            readonly property real ty: 2 * (q.z * rx - q.x * rz)
            readonly property real tz: 2 * (q.x * ry - q.y * rx)
            readonly property real wx: controller.sceneCenter.x + rx + q.scalar * tx + (q.y * tz - q.z * ty)
            readonly property real wy: controller.sceneCenter.y + ry + q.scalar * ty + (q.z * tx - q.x * tz)
            readonly property real wz: controller.sceneCenter.z + rz + q.scalar * tz + (q.x * ty - q.y * tx)
            readonly property real depth: (controller.pivotPosition.z + controller.cameraDistance) - wz
            readonly property real halfH: Math.tan(controller.fieldOfView * Math.PI / 360.0)
            readonly property real halfW: halfH * (root.width / Math.max(1, root.height))
            readonly property real ndcX: (wx - controller.pivotPosition.x) / (Math.max(depth, 0.0001) * halfW)
            readonly property real ndcY: (wy - controller.pivotPosition.y) / (Math.max(depth, 0.0001) * halfH)
            visible: depth > 0.0001
            x: (ndcX + 1) * 0.5 * root.width - width / 2
            y: (1 - ndcY) * 0.5 * root.height - height / 2
            text: modelData.text
            color: "#ffffff"
            font.pixelSize: 11
            font.bold: true
            style: Text.Outline
            styleColor: "#000000"
        }
    }

    // Contact labels of the NCI overlay (the interaction distance, drawn at the
    // midpoint of each contact). Same {px,py,pz,text} map shape and the same
    // projection bindings as the atom-label Repeater above - keep the two in sync
    // when the camera model changes; only size and colour differ here.
    Repeater {
        model: controller.nciLabels
        delegate: Text {
            readonly property real rx: modelData.px - controller.sceneCenter.x
            readonly property real ry: modelData.py - controller.sceneCenter.y
            readonly property real rz: modelData.pz - controller.sceneCenter.z
            readonly property quaternion q: controller.rootRotation
            readonly property real tx: 2 * (q.y * rz - q.z * ry)
            readonly property real ty: 2 * (q.z * rx - q.x * rz)
            readonly property real tz: 2 * (q.x * ry - q.y * rx)
            readonly property real wx: controller.sceneCenter.x + rx + q.scalar * tx + (q.y * tz - q.z * ty)
            readonly property real wy: controller.sceneCenter.y + ry + q.scalar * ty + (q.z * tx - q.x * tz)
            readonly property real wz: controller.sceneCenter.z + rz + q.scalar * tz + (q.x * ty - q.y * tx)
            readonly property real depth: (controller.pivotPosition.z + controller.cameraDistance) - wz
            readonly property real halfH: Math.tan(controller.fieldOfView * Math.PI / 360.0)
            readonly property real halfW: halfH * (root.width / Math.max(1, root.height))
            readonly property real ndcX: (wx - controller.pivotPosition.x) / (Math.max(depth, 0.0001) * halfW)
            readonly property real ndcY: (wy - controller.pivotPosition.y) / (Math.max(depth, 0.0001) * halfH)
            visible: depth > 0.0001
            x: (ndcX + 1) * 0.5 * root.width - width / 2
            y: (1 - ndcY) * 0.5 * root.height - height / 2
            text: modelData.text
            color: modelData.color !== undefined ? modelData.color : "#cfe9ff"
            font.pixelSize: 10
            font.bold: true
            style: Text.Outline
            styleColor: "#000000"
        }
    }
}
