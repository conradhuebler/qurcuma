// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// WP0 Qt Quick 3D spike - scene. Claude Generated.
//
// "controller" is the SceneController exposed as a QML context property by main.cpp.
import QtQuick
import QtQuick.Controls
import QtQuick3D
import QtQuick3D.Helpers

Item {
    id: root

    View3D {
        id: view3D
        anchors.fill: parent
        camera: cameraNode

        // T4 - built-in post-processing. Toggles bound to the side-panel checkboxes.
        environment: ExtendedSceneEnvironment {
            backgroundMode: SceneEnvironment.Color
            clearColor: "#1c2230"
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High

            // SSAO
            aoEnabled: controller.ssaoEnabled
            aoStrength: 90
            aoDistance: 4.0
            aoSoftness: 30

            // Bloom / glow
            glowEnabled: controller.bloomEnabled
            glowStrength: 0.9
            glowIntensity: 0.7
            glowBloom: 0.4
            glowHDRMinimumValue: 0.9

            // Tonemapping
            tonemapMode: controller.tonemapEnabled
                ? SceneEnvironment.TonemapModeFilmic
                : SceneEnvironment.TonemapModeNone
        }

        // Orbit pivot at the molecule centre; the camera hangs off it at a distance
        // scaled to the structure's bounding sphere.
        Node {
            id: orbitOrigin
            position: controller.sceneCenter
            PerspectiveCamera {
                id: cameraNode
                z: controller.sceneExtent * 3.0
                clipNear: 1.0
                clipFar: controller.sceneExtent * 20.0 + 2000.0
                fieldOfView: 55
            }
        }

        OrbitCameraController {
            anchors.fill: parent
            origin: orbitOrigin
            camera: cameraNode
        }

        // Key light casts the shadows (T4); a dim fill light keeps the dark side readable.
        DirectionalLight {
            eulerRotation.x: -40
            eulerRotation.y: -50
            brightness: 1.0
            castsShadow: controller.shadowsEnabled
            shadowMapQuality: Light.ShadowMapQualityHigh
            shadowFactor: 85
        }
        DirectionalLight {
            eulerRotation.x: 25
            eulerRotation.y: 130
            brightness: 0.35
            castsShadow: false
        }

        // Floor so cast shadows are actually visible; sits just below the structure.
        Model {
            source: "#Rectangle"
            visible: controller.shadowsEnabled
            position: Qt.vector3d(controller.sceneCenter.x,
                                  controller.sceneCenter.y - controller.sceneExtent * 1.1,
                                  controller.sceneCenter.z)
            eulerRotation.x: -90
            scale: Qt.vector3d(controller.sceneExtent * 0.3,
                               controller.sceneExtent * 0.3, 1)
            materials: PrincipledMaterial {
                baseColor: "#3a4252"
                roughness: 0.9
            }
        }

        // T3 - atoms: one instanced draw call for the whole structure.
        Model {
            id: atomModel
            source: "#Sphere"
            instancing: controller.atomInstancing
            pickable: true   // REQUIRED: View3D.pick() ignores non-pickable models
            materials: PrincipledMaterial {
                baseColor: "white"   // per-instance colour multiplies this
                metalness: 0.0
                roughness: 0.4
            }
        }

        // T3 - bonds: instanced half-cylinders.
        Model {
            id: bondModel
            source: "#Cylinder"
            visible: controller.bondsVisible
            instancing: controller.bondInstancing
            materials: PrincipledMaterial {
                baseColor: "white"
                metalness: 0.0
                roughness: 0.55
            }
        }

        // T7 - picking. Tap (not drag, so it coexists with the orbit controller)
        // -> View3D.pick -> instanceIndex on the atom model.
        TapHandler {
            onTapped: function(eventPoint) {
                var result = view3D.pick(eventPoint.position.x, eventPoint.position.y)
                // Only atoms are instanced + pickable, so a valid instanceIndex
                // uniquely identifies an atom hit (more robust than objectHit ===).
                controller.setSelectedAtom(result.instanceIndex)
            }
        }
    }

    // --- In-scene HUD (UX #2): live info overlaid on the 3D view ---
    Rectangle {
        anchors { left: parent.left; top: parent.top; margins: 8 }
        width: hud.implicitWidth + 16
        height: hud.implicitHeight + 12
        radius: 6
        color: "#cc101418"
        border.color: "#3399ccff"

        Column {
            id: hud
            anchors.centerIn: parent
            spacing: 2
            Text { color: "#e8f0ff"; font.bold: true; text: "qurcuma · Quick3D spike" }
            Text { color: "#9fe3ff"; text: "Backend: " + controller.backendName
                   + "   Embed: " + controller.embedMode }
            Text { color: "#e8f0ff"; font.pixelSize: 18; font.bold: true
                   text: controller.fps.toFixed(1) + " FPS" }
            Text { color: "#cfd8e6"; text: "Atoms: " + controller.atomCount
                   + "   Bonds: " + controller.bondCount }
            Text { color: "#cfd8e6"
                   text: "FX: " + (controller.ssaoEnabled ? "AO " : "")
                         + (controller.shadowsEnabled ? "Shadow " : "")
                         + (controller.bloomEnabled ? "Bloom " : "")
                         + (controller.tonemapEnabled ? "Tonemap" : "") }
            Text { color: "#ffd0ff"; text: "Picked: " + controller.selectionLabel }
        }
    }

    // --- HUD buttons: reset view (UX #3) + screenshot (UX #1) ---
    Row {
        anchors { right: parent.right; top: parent.top; margins: 8 }
        spacing: 6
        Button {
            text: "Reset view"
            onClicked: {
                orbitOrigin.eulerRotation = Qt.vector3d(0, 0, 0)
                cameraNode.z = controller.sceneExtent * 3.0
            }
        }
        Button {
            text: "Screenshot"
            onClicked: view3D.grabToImage(function(result) {
                var name = "spike_" + Date.now() + ".png"
                if (result.saveToFile(name))
                    console.log("saved screenshot:", name)
            })
        }
    }
}
