// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// T9 VR spike - scene in OpenXR. Qt Quick 3D units are centimetres, so the molecule
// (Angstrom scale) is scaled to ~40 cm and floated 80 cm in front of the user at eye
// height. Claude Generated.
import QtQuick
import QtQuick3D
import QtQuick3D.Xr

XrView {
    id: xrView
    xrOrigin: origin

    environment: SceneEnvironment {
        backgroundMode: SceneEnvironment.Color
        clearColor: "#1c2230"
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.Medium
    }

    XrOrigin {
        id: origin
        z: 0

        // Right controller pose; the marker shows where the runtime puts the hand and
        // turns red while the trigger is held.
        XrController {
            id: rightHand
            controller: XrController.ControllerRight
            poseSpace: XrController.AimPose

            Model {
                source: "#Sphere"
                scale: Qt.vector3d(0.03, 0.03, 0.03)
                materials: PrincipledMaterial {
                    baseColor: xrView.grabbing ? "#ff4040" : "#40ff80"
                    lighting: PrincipledMaterial.NoLighting
                }
            }
        }
    }

    XrInputAction {
        id: triggerAction
        hand: XrInputAction.RightHand
        actionId: XrInputAction.TriggerValue
    }

    DirectionalLight {
        eulerRotation.x: -40
        eulerRotation.y: -50
        brightness: 1.0
    }
    DirectionalLight {
        eulerRotation.x: 25
        eulerRotation.y: 130
        brightness: 0.35
    }

    Node {
        id: moleculeAnchor
        position: Qt.vector3d(0, 120, -80)

        Node {
            id: moleculeRoot
            property real fit: 40 / Math.max(controller.sceneExtent, 1)
            scale: Qt.vector3d(fit, fit, fit)

            Node {
                // centre the structure on the anchor
                position: Qt.vector3d(-controller.sceneCenter.x,
                                      -controller.sceneCenter.y,
                                      -controller.sceneCenter.z)
                Model {
                    source: "#Sphere"
                    instancing: controller.atomInstancing
                    materials: PrincipledMaterial { baseColor: "white"; roughness: 0.4 }
                }
                Model {
                    source: "#Cylinder"
                    instancing: controller.bondInstancing
                    materials: PrincipledMaterial { baseColor: "white"; roughness: 0.55 }
                }
            }
        }
    }

    property quaternion lastHandRot: Qt.quaternion(1, 0, 0, 0)
    property bool grabbing: triggerAction.value > 0.5
    onGrabbingChanged: if (grabbing) lastHandRot = rightHand.sceneRotation

    Connections {
        target: rightHand
        function onSceneRotationChanged() {
            if (!xrView.grabbing)
                return
            // delta = current * inverse(last), applied in world space
            var d = rightHand.sceneRotation.times(xrView.lastHandRot.inverted())
            moleculeAnchor.rotation = d.times(moleculeAnchor.rotation)
            xrView.lastHandRot = rightHand.sceneRotation
        }
    }
}
