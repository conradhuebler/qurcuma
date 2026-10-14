// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Qurcuma VR view (OpenXR), hosted by VrSession. Context: "controller" = the desktop viewer's
// SceneController, "xrInstancing" = XrSceneBridge (its instance tables, mirrored into this window),
// "vr" = VrSession. Claude Generated.
//
// This file holds what only exists in a headset: the XrView, the tracked head and hands, controller
// input and the grab/rotate logic. The molecule and the panels are in VrScene.qml, which the offline
// preview (spikes/vr_preview) renders without a headset. Units are centimetres.
//   right hand: trigger on an atom (simulation running) = grab it with a spring force,
//               trigger elsewhere or grip = rotate the molecule, A = select atom under the ray
//   left hand : ray + trigger operate the panels (XrVirtualMouse)
import QtQuick
import QtQuick3D
import QtQuick3D.Xr

XrView {
    id: xrView
    xrOrigin: origin
    quitOnSessionEnd: false   // the headset session ending must not quit qurcuma
    onSessionEnded: vr.ended()

    environment: SceneEnvironment {
        backgroundMode: SceneEnvironment.Color
        clearColor: controller.backgroundColor
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.Medium
    }

    // ---- head and hands ----------------------------------------------------------------------
    XrOrigin {
        id: origin
        z: 0
        camera: XrCamera { id: headCamera; clipNear: 1; clipFar: 10000 }

        XrController {
            id: rightHand
            controller: XrController.ControllerRight
            poseSpace: XrController.AimPose

            // aim ray, 150 cm along -Z (Cylinder axis is Y, 100 units tall)
            Model {
                source: "#Cylinder"
                position: Qt.vector3d(0, 0, -75)
                eulerRotation.x: 90
                scale: Qt.vector3d(0.004, 1.5, 0.004)
                materials: PrincipledMaterial { baseColor: "#80c0ff"; lighting: PrincipledMaterial.NoLighting }
            }
            // marker: green idle, red while grabbing an atom, orange while rotating
            Model {
                source: "#Sphere"
                scale: Qt.vector3d(0.03, 0.03, 0.03)
                materials: PrincipledMaterial {
                    baseColor: xrView.grabAtom >= 0 ? "#ff4040" : (xrView.rotating ? "#ffa030" : "#40ff80")
                    lighting: PrincipledMaterial.NoLighting
                }
            }
        }

        XrController {
            id: leftHand
            controller: XrController.ControllerLeft
            poseSpace: XrController.AimPose

            Model {
                source: "#Cylinder"
                position: Qt.vector3d(0, 0, -75)
                eulerRotation.x: 90
                scale: Qt.vector3d(0.004, 1.5, 0.004)
                materials: PrincipledMaterial { baseColor: "#ffd080"; lighting: PrincipledMaterial.NoLighting }
            }
            Model {
                source: "#Sphere"
                scale: Qt.vector3d(0.03, 0.03, 0.03)
                materials: PrincipledMaterial {
                    baseColor: leftTrigger.value > 0.5 ? "#ff4040" : "#ffd040"
                    lighting: PrincipledMaterial.NoLighting
                }
            }
        }
    }

    XrInputAction { id: rightTrigger; hand: XrInputAction.RightHand; actionId: XrInputAction.TriggerValue }
    XrInputAction { id: rightSqueeze; hand: XrInputAction.RightHand; actionId: XrInputAction.SqueezeValue }
    XrInputAction { id: leftTrigger;  hand: XrInputAction.LeftHand;  actionId: XrInputAction.TriggerValue }
    XrInputAction {
        id: pickAction
        hand: XrInputAction.RightHand
        actionId: XrInputAction.Button1Pressed   // A
        onPressedChanged: if (pressed) xrView.pickAtom()
    }

    // left controller drives the 2D panels like a mouse
    XrVirtualMouse {
        view: xrView
        source: leftHand
        leftMouseButton: leftTrigger.value > 0.5
    }

    // ---- molecule and panels -----------------------------------------------------------------
    VrScene {
        id: scene
        session: vr
        sceneController: controller
        instancing: xrInstancing
        onRecenterRequested: xrView.recenter()
    }

    // Lay the scene out around the head: its position (eye height included, whatever the reference
    // space) and its heading. Done once the headset reports a pose, and on "Recenter".
    function recenter() {
        var forward = headCamera.sceneRotation.times(Qt.vector3d(0, 0, -1))
        scene.headPosition = headCamera.scenePosition
        scene.headYaw = Math.atan2(-forward.x, -forward.z) * 180 / Math.PI
    }
    Timer {
        id: placeOnStart
        interval: 250
        repeat: true
        running: true
        property int tries: 0
        onTriggered: {
            // An exactly untouched pose means tracking has not started yet; keep the default layout
            // until it has (giving up after 10 s). Not a distance test: in the "local" reference
            // space the head starts at the origin.
            var p = headCamera.scenePosition
            var q = headCamera.sceneRotation
            if (p.x !== 0 || p.y !== 0 || p.z !== 0 || q.scalar !== 1) {
                xrView.recenter()
                stop()
            } else if (++tries > 40) {
                stop()
            }
        }
    }

    // ---- right hand: select / grab / rotate ----------------------------------------------------
    property int grabAtom: -1
    property bool triggerDown: rightTrigger.value > 0.5
    property bool squeezeDown: rightSqueeze.value > 0.5
    property bool rotating: squeezeDown || (triggerDown && grabAtom < 0)
    property quaternion lastHandRot: Qt.quaternion(1, 0, 0, 0)

    function aimDirection() {
        return rightHand.sceneRotation.times(Qt.vector3d(0, 0, -1))
    }

    // Atom index under the right-hand ray, -1 if the ray hits no atom. rayPick reports a row of the
    // atom instance table, which skips hidden atoms (hydrogen display, hidden molecule kinds), so
    // the row is mapped back to the atom index.
    function atomUnderRay() {
        var result = xrView.rayPick(rightHand.scenePosition, aimDirection())
        if (result.objectHit !== scene.atomModel)
            return -1
        return controller.atomIndexForInstance(result.instanceIndex)
    }

    function pickAtom() {
        vr.atomPicked(atomUnderRay())
    }

    onTriggerDownChanged: {
        if (triggerDown) {
            var atom = atomUnderRay()
            if (vr.simulationActive && atom >= 0)
                grabAtom = atom
        } else if (grabAtom >= 0) {
            grabAtom = -1
            vr.grabReleased()
        }
    }
    onRotatingChanged: if (rotating) lastHandRot = rightHand.sceneRotation

    Connections {
        target: rightHand
        function onSceneRotationChanged() {
            if (!xrView.rotating)
                return
            // delta = current * inverse(last), applied in world space (the anchor's parent only
            // translates, so its local rotation is its world rotation)
            var d = rightHand.sceneRotation.times(xrView.lastHandRot.inverted())
            scene.moleculeAnchor.rotation = d.times(scene.moleculeAnchor.rotation)
            xrView.lastHandRot = rightHand.sceneRotation
        }
    }

    // Spring target: the point on the ray at the atom's distance, expressed in atom coordinates.
    Timer {
        interval: 33
        repeat: true
        running: xrView.grabAtom >= 0
        onTriggered: {
            var atom = controller.atomPosition(xrView.grabAtom)
            var atomScene = scene.atomsNode.mapPositionToScene(atom)
            var hand = rightHand.scenePosition
            var dist = atomScene.minus(hand).length()
            var target = hand.plus(xrView.aimDirection().times(dist))
            vr.grabForce(xrView.grabAtom, scene.atomsNode.mapPositionFromScene(target).minus(atom))
        }
    }
}
