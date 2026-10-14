// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Qurcuma VR view (OpenXR), hosted by VrSession. Context: "controller" = the desktop viewer's
// SceneController, "xrInstancing" = XrSceneBridge (its instance tables, mirrored into this window),
// "vr" = VrSession. Claude Generated.
//
// This file holds what only exists in a headset: the XrView, the tracked head and hands, controller
// input and the grab/rotate logic. The molecule and the panels are in VrScene.qml, which the offline
// preview (spikes/vr_preview) renders without a headset. Units are centimetres.
//
// Both hands work the same (Hand below), so left- and right-handed users need nothing to learn:
//   trigger on a panel              = click the button under the ray (each hand clicks on its own)
//   trigger on an atom (simulation) = grab it with a spring force (one atom at a time)
//   trigger elsewhere, or grip      = rotate the molecule with the hand
//   A / X                           = select the atom under the ray
//   B / Y                           = hide / show the panels
// The help panel (VrPanelHelp.qml) explains the same; keep both in sync.
import QtQuick
import QtQuick3D
import QtQuick3D.Xr
import "VrStyle.js" as Style

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

        Hand {
            id: rightHand
            controller: XrController.ControllerRight
            inputHand: XrInputAction.RightHand
            view: xrView
            vrScene: scene
        }
        Hand {
            id: leftHand
            controller: XrController.ControllerLeft
            inputHand: XrInputAction.LeftHand
            view: xrView
            vrScene: scene
        }
    }

    /// The hand holding an atom (null = none); VrSession knows a single grab only.
    property Hand grabHand: null

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

    // ---- one controller: ray, panel click, select / grab / rotate (Claude Generated 2026) ------
    component Hand: XrController {
        id: thisHand

        /// XrInputAction.LeftHand or RightHand, for the buttons of this controller
        required property int inputHand
        required property var view   // the XrView (var: it carries grabHand)
        required property var vrScene

        poseSpace: XrController.AimPose

        /// distance (cm) to the panel under the ray, -1 = not on a panel
        property real panelDistance: -1
        readonly property bool pointsAtPanel: panelDistance >= 0
        /// VrButton under the ray, and the one the trigger went down on (typed: nulls itself when a
        /// file list row is destroyed under the ray)
        property VrButton hoverButton: null
        property VrButton pressedButton: null
        /// trigger pulled while on a panel: a click in progress, never a rotation
        property bool pressing: false
        property int grabAtom: -1
        readonly property bool triggerDown: trigger.value > 0.5
        readonly property bool squeezeDown: squeeze.value > 0.5
        readonly property bool rotating: !pressing && grabAtom < 0 && (triggerDown || squeezeDown)
        property quaternion lastRotation: Qt.quaternion(1, 0, 0, 0)

        XrInputAction { id: trigger; hand: thisHand.inputHand; actionId: XrInputAction.TriggerValue }
        XrInputAction { id: squeeze; hand: thisHand.inputHand; actionId: XrInputAction.SqueezeValue }
        XrInputAction {
            hand: thisHand.inputHand
            actionId: XrInputAction.Button1Pressed   // A (right) / X (left)
            onPressedChanged: if (pressed) vr.atomPicked(thisHand.atomUnderRay())
        }
        XrInputAction {
            hand: thisHand.inputHand
            actionId: XrInputAction.Button2Pressed   // B (right) / Y (left)
            onPressedChanged: if (pressed) thisHand.vrScene.togglePanels()
        }

        function aimDirection() {
            return thisHand.sceneRotation.times(Qt.vector3d(0, 0, -1))
        }

        // Atom index under the ray, -1 if the ray hits no atom or a panel is in front of it. The
        // pick is limited to the atom model: an unrestricted rayPick also reports every panel's
        // infinite plane (Qt 6.11) and those lie in front of the molecule. It reports a row of the
        // atom instance table, which skips hidden atoms (hydrogen display, hidden molecule kinds),
        // so the row is mapped back to the atom index.
        function atomUnderRay() {
            var result = thisHand.view.rayPick(thisHand.scenePosition, aimDirection(), thisHand.vrScene.atomModel)
            if (result.objectHit !== thisHand.vrScene.atomModel)
                return -1
            if (pointsAtPanel && panelDistance < result.distance)
                return -1
            return thisHand.vrScene.sceneController.atomIndexForInstance(result.instanceIndex)
        }

        // Panel and button under the ray. Lights the button (rayHoverCount: both hands may point at
        // it), and shows a held button as pressed only while the ray stays on it, like a mouse.
        function updatePanelHit() {
            var hit = thisHand.vrScene.panelHit(thisHand.scenePosition, aimDirection())
            panelDistance = hit ? hit.distance : -1
            var button = hit ? hit.button : null
            if (button === hoverButton)
                return
            if (hoverButton)
                hoverButton.rayHoverCount--
            hoverButton = button
            if (hoverButton)
                hoverButton.rayHoverCount++
            if (pressedButton)
                pressedButton.rayDown = pressedButton === hoverButton
        }
        onScenePositionChanged: updatePanelHit()
        onSceneRotationChanged: {
            updatePanelHit()
            if (!rotating)
                return
            // delta = current * inverse(last), applied in world space (the anchor's parent only
            // translates, so its local rotation is its world rotation)
            var d = thisHand.sceneRotation.times(thisHand.lastRotation.inverted())
            thisHand.vrScene.moleculeAnchor.rotation = d.times(thisHand.vrScene.moleculeAnchor.rotation)
            thisHand.lastRotation = thisHand.sceneRotation
        }
        onRotatingChanged: if (rotating) lastRotation = thisHand.sceneRotation

        onTriggerDownChanged: {
            if (triggerDown) {
                if (pointsAtPanel) {
                    pressing = true
                    pressedButton = hoverButton && hoverButton.enabled ? hoverButton : null
                    if (pressedButton)
                        pressedButton.rayDown = true
                    return
                }
                var atom = atomUnderRay()
                if (vr.simulationActive && atom >= 0 && thisHand.view.grabHand === null) {
                    grabAtom = atom
                    thisHand.view.grabHand = thisHand
                }
            } else {
                // Click = trigger released on the button it went down on (a mouse click's rule).
                var button = pressedButton
                pressedButton = null
                pressing = false
                if (button) {
                    button.rayDown = false
                    if (button === hoverButton && button.enabled)
                        button.clicked()
                }
                if (grabAtom >= 0) {
                    grabAtom = -1
                    thisHand.view.grabHand = null
                    vr.grabReleased()
                }
            }
        }

        // Spring target: the point on the ray at the atom's distance, expressed in atom coordinates.
        Timer {
            interval: 33
            repeat: true
            running: thisHand.grabAtom >= 0
            onTriggered: {
                var atom = thisHand.vrScene.sceneController.atomPosition(thisHand.grabAtom)
                var atomScene = thisHand.vrScene.atomsNode.mapPositionToScene(atom)
                var origin = thisHand.scenePosition
                var dist = atomScene.minus(origin).length()
                var target = origin.plus(thisHand.aimDirection().times(dist))
                vr.grabForce(thisHand.grabAtom, thisHand.vrScene.atomsNode.mapPositionFromScene(target).minus(atom))
            }
        }

        // Aim ray along -Z (Cylinder axis is Y, 100 units tall). It ends on the panel it points at,
        // so the user sees exactly which button the trigger will press.
        Model {
            readonly property real length: thisHand.pointsAtPanel ? thisHand.panelDistance : 150
            source: "#Cylinder"
            position: Qt.vector3d(0, 0, -length / 2)
            eulerRotation.x: 90
            scale: Qt.vector3d(0.004, length / 100, 0.004)
            materials: PrincipledMaterial {
                baseColor: thisHand.pointsAtPanel ? Style.rayPanel : Style.rayIdle
                lighting: PrincipledMaterial.NoLighting
            }
        }
        // Marker: red while holding an atom, orange while rotating, blue on a panel, green idle.
        Model {
            source: "#Sphere"
            scale: Qt.vector3d(0.03, 0.03, 0.03)
            materials: PrincipledMaterial {
                baseColor: thisHand.grabAtom >= 0 ? Style.markerGrab
                         : thisHand.rotating ? Style.markerRotate
                         : thisHand.pointsAtPanel || thisHand.pressing ? Style.markerPanel : Style.markerIdle
                lighting: PrincipledMaterial.NoLighting
            }
        }
    }
}
