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
// The thumbsticks are the one exception (Claude Generated 2026), they move the user, not the scene:
//   left stick                      = walk (horizontal, relative to where the head looks)
//   right stick left / right        = snap turn by 45 degrees about the head
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

    // Surroundings (Claude Generated 2026): a 360 degree panorama (equirectangular image, e.g. .hdr
    // or .jpg) becomes the sky box; without one the desktop background colour fills the view. Qt
    // folds the panorama into a cube map and also lights the scene with it (image-based lighting).
    environment: SceneEnvironment {
        backgroundMode: vr.panorama.toString() !== "" ? SceneEnvironment.SkyBox : SceneEnvironment.Color
        clearColor: controller.backgroundColor
        lightProbe: vr.panorama.toString() !== "" ? panoramaTexture : null
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.Medium
    }
    Texture {
        id: panoramaTexture
        source: vr.panorama
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

    // ---- locomotion: left stick walks, right stick snap-turns (Claude Generated 2026) -----------
    // The user moves, not the scene: both act on the XrOrigin, which carries head and hands, so the
    // molecule, the panels and the surroundings stay where they are. Qt Quick 3D is y-up, -z forward;
    // walking stays in the horizontal x/z plane.
    XrInputAction { id: walkX; hand: XrInputAction.LeftHand; actionId: XrInputAction.ThumbstickX }
    XrInputAction { id: walkY; hand: XrInputAction.LeftHand; actionId: XrInputAction.ThumbstickY }
    XrInputAction { id: turnX; hand: XrInputAction.RightHand; actionId: XrInputAction.ThumbstickX }

    /// Stick deflection below this is ignored (resting sticks never read exactly 0).
    readonly property real stickDeadzone: 0.15
    /// True while a snap turn rotates the origin: the hands must not read it as a rotate gesture.
    property bool turning: false

    FrameAnimation {
        running: Math.hypot(walkX.value, walkY.value) > xrView.stickDeadzone
        onTriggered: xrView.walk(frameTime)
    }

    /// Move the origin by the left stick for @p dt seconds: forward = where the head looks,
    /// flattened to the floor; speed rises from 0 at the deadzone to Style.walkSpeed at full tilt.
    function walk(dt) {
        var x = walkX.value
        var y = walkY.value
        var tilt = Math.hypot(x, y)
        if (tilt <= stickDeadzone)
            return
        var look = headCamera.sceneRotation.times(Qt.vector3d(0, 0, -1))
        var forward = Qt.vector3d(look.x, 0, look.z)
        if (forward.length() < 1e-3)   // looking straight up or down: no heading
            return
        forward = forward.normalized()
        var right = Qt.vector3d(-forward.z, 0, forward.x)
        var speed = Style.walkSpeed * Math.min(1, (tilt - stickDeadzone) / (1 - stickDeadzone)) / tilt
        origin.position = origin.position.plus(forward.times(y * speed * dt).plus(right.times(x * speed * dt)))
    }

    // One turn per push: the stick has to come back towards the centre before the next one.
    readonly property int turnDirection: turnX.value > 0.6 ? 1 : turnX.value < -0.6 ? -1 : 0
    onTurnDirectionChanged: if (turnDirection !== 0) snapTurn(-turnDirection * Style.snapTurnDegrees)

    /// Turn the user by @p degrees (> 0 = left) about the vertical axis through the head, so the
    /// head stays in place: origin' = head + R (origin - head), rotation' = R rotation.
    function snapTurn(degrees) {
        var r = Quaternion.fromEulerAngles(0, degrees, 0)
        var head = headCamera.scenePosition
        turning = true
        origin.position = head.plus(r.times(origin.position.minus(head)))
        origin.rotation = r.times(origin.rotation)
        turning = false
        rightHand.lastRotation = rightHand.sceneRotation
        leftHand.lastRotation = leftHand.sceneRotation
    }

    // ---- molecule and panels -----------------------------------------------------------------
    VrScene {
        id: scene
        session: vr
        sceneController: controller
        instancing: xrInstancing
        surroundingsSource: vr.surroundings
        surroundingsScale: vr.surroundingsScale
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
                // Floor for the surroundings: y = 0 in a floor-based reference space; in "local"
                // space the origin is the starting head, so the floor is one eye height below it.
                var floorBased = xrView.referenceSpace === XrView.ReferenceSpaceStage
                              || xrView.referenceSpace === XrView.ReferenceSpaceLocalFloor
                scene.floorHeight = floorBased ? 0 : p.y - Style.eyeHeight
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
            if (!rotating || thisHand.view.turning)
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
