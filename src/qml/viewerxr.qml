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
//   trigger on a panel              = click the button under the ray (each hand clicks on its own),
//                                     or drag the thermostat track; on a handle: move that panel
//   trigger on an atom (simulation) = grab it with a spring force; both hands may pull at once,
//                                     different atoms or the same one (Claude Generated 2026)
//   trigger on the molecule, no simulation = carry it: it follows the ray point, keeping its offset
//                                     (Claude Generated 2026)
//   grip                            = rotate the molecule with the hand (also while carrying it)
//   trigger elsewhere               = rotate the molecule with the hand
//   A / X                           = select the atom under the ray
//   B / Y                           = hide / show the panels
// The thumbsticks are the one exception (Claude Generated 2026), they move the user, not the scene:
//   left stick                      = walk (horizontal, relative to where the head looks)
//   right stick left / right        = snap turn by 45 degrees about the head
// The help panel (VrPanelHelp.qml) explains the same; keep both in sync.
//
// Input mode (vr.handInput, Claude Generated 2026): the above is the controller mode. In hand mode the
// tracked hands (VrHandInput.qml) pinch and touch instead; the controllers' rays, triggers and sticks
// are off then, except B / Y, which switches back to controllers (a way out when the runtime does
// not track hands).
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

        // Tracked hands; joint data arrive in XrOrigin space, so they live in it (Claude Generated 2026)
        VrHandInput {
            vrScene: scene
            session: vr
            active: vr.handInput
        }
    }

    // ---- locomotion: left stick walks, right stick snap-turns (Claude Generated 2026) -----------
    // The user moves, not the scene: both act on the XrOrigin, which carries head and hands, so the
    // molecule, the panels and the surroundings stay where they are. Qt Quick 3D is y-up, -z forward;
    // walking stays in the horizontal x/z plane.
    // Controller mode only: a hand holding the molecule must not be turned under it.
    XrInputAction { id: walkX; enabled: !vr.handInput; hand: XrInputAction.LeftHand; actionId: XrInputAction.ThumbstickX }
    XrInputAction { id: walkY; enabled: !vr.handInput; hand: XrInputAction.LeftHand; actionId: XrInputAction.ThumbstickY }
    XrInputAction { id: turnX; enabled: !vr.handInput; hand: XrInputAction.RightHand; actionId: XrInputAction.ThumbstickX }

    /// Stick deflection below this is ignored (resting sticks never read exactly 0).
    readonly property real stickDeadzone: 0.15
    /// True while a snap turn rotates the origin: the hands must not read it as a rotate gesture.
    property bool turning: false

    FrameAnimation {
        running: !vr.handInput && Math.hypot(walkX.value, walkY.value) > xrView.stickDeadzone
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
    readonly property int turnDirection: vr.handInput ? 0 : turnX.value > 0.6 ? 1 : turnX.value < -0.6 ? -1 : 0
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
        scene.moleculeOffset = Qt.vector3d(0, 0, 0)   // carried by the hands: back to its place
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

        /// Controller mode (Claude Generated 2026): in hand mode the ray, the trigger and the grip
        /// are off; triggerDown dropping to false releases whatever the trigger held.
        readonly property bool active: !vr.handInput
        visible: active

        /// distance (cm) to the panel under the ray, -1 = not on a panel
        property real panelDistance: -1
        readonly property bool pointsAtPanel: panelDistance >= 0
        /// VrButton (or drag target, e.g. the thermostat track) under the ray, and the one the
        /// trigger went down on (typed: nulls itself when a file list row is destroyed under the ray)
        property Item hoverButton: null
        property Item pressedButton: null
        /// panel host of the pressed drag target: its plane turns the ray into track coordinates
        property var pressedHost: null
        /// panel whose handle is under the ray, and the one the trigger holds by its handle; while
        /// held, the panel centre follows the ray point at the grab distance plus the grab offset
        property var hoverHandleHost: null
        property var movingHost: null
        property real moveDistance: 0
        property vector3d moveOffset
        /// last panel hit (VrPanelHost.rayHit), for the press
        property var panelHitResult: null
        /// atom under the ray (-1 = none), highlighted by VrScene under this key (Claude Generated 2026)
        property int hoverAtom: -1
        readonly property string highlightKey: inputHand === XrInputAction.LeftHand ? "leftController" : "rightController"
        /// grab source of this hand for VrSession.grabForce (1 = right, 2 = left; Claude Generated 2026)
        readonly property int grabSource: inputHand === XrInputAction.LeftHand ? 2 : 1
        /// trigger pulled while on a panel: a click in progress, never a rotation
        property bool pressing: false
        property int grabAtom: -1
        /// Carrying the molecule (trigger on it, no simulation; Claude Generated 2026): it follows the
        /// ray point at the grab distance, plus the anchor's offset from that point at the start.
        property bool movingMolecule: false
        property real moleculeGrabDistance: 0
        property vector3d moleculeGrabOffset
        readonly property bool triggerDown: active && trigger.value > 0.5
        readonly property bool squeezeDown: active && squeeze.value > 0.5
        readonly property bool rotating: !pressing && grabAtom < 0 && (squeezeDown || (triggerDown && !movingMolecule))
        property quaternion lastRotation: Qt.quaternion(1, 0, 0, 0)

        XrInputAction { id: trigger; enabled: thisHand.active; hand: thisHand.inputHand; actionId: XrInputAction.TriggerValue }
        XrInputAction { id: squeeze; enabled: thisHand.active; hand: thisHand.inputHand; actionId: XrInputAction.SqueezeValue }
        XrInputAction {
            enabled: thisHand.active
            hand: thisHand.inputHand
            actionId: XrInputAction.Button1Pressed   // A (right) / X (left)
            onPressedChanged: if (pressed) vr.atomPicked(thisHand.atomUnderRay())
        }
        XrInputAction {
            hand: thisHand.inputHand
            actionId: XrInputAction.Button2Pressed   // B (right) / Y (left), in both modes
            onPressedChanged: {
                if (!pressed)
                    return
                if (vr.handInput)
                    vr.handInput = false   // picked up a controller: back to controller input
                else
                    thisHand.vrScene.togglePanels()
            }
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

        // Distance along the ray to the molecule, -1 if the ray misses it (Claude Generated 2026): an
        // atom hit, else the ray passing within half the molecule's fitted radius of its centre
        // (sparse molecules have gaps between the atoms). Panels in front are handled by the caller.
        function moleculeDistanceOnRay() {
            var origin = thisHand.scenePosition
            var direction = aimDirection()
            var result = thisHand.view.rayPick(origin, direction, thisHand.vrScene.atomModel)
            if (result.objectHit === thisHand.vrScene.atomModel)
                return result.distance
            var toCentre = thisHand.vrScene.moleculeAnchor.scenePosition.minus(origin)
            var along = toCentre.dotProduct(direction)
            if (along <= 0)
                return -1
            var miss = toCentre.minus(direction.times(along)).length()
            return miss < 0.5 * thisHand.vrScene.moleculeSize ? along : -1
        }

        /// Molecule follows the ray point while carried (from the pose updates).
        function carryMolecule() {
            var target = thisHand.scenePosition.plus(aimDirection().times(moleculeGrabDistance))
            thisHand.vrScene.moveMoleculeTo(target.plus(moleculeGrabOffset))
        }

        // Panel and button under the ray. Lights the button (rayHoverCount: both hands may point at
        // it), and shows a held button as pressed only while the ray stays on it, like a mouse. A
        // held drag target or panel handle follows the ray (Claude Generated 2026).
        function updatePanelHit() {
            var hit = active ? thisHand.vrScene.panelHit(thisHand.scenePosition, aimDirection()) : null
            panelHitResult = hit
            panelDistance = hit ? hit.distance : -1
            setHoverHandle(hit && hit.handle ? hit.host : null)
            if (movingHost) {
                var target = thisHand.scenePosition.plus(aimDirection().times(moveDistance))
                movingHost.moveCentreTo(target.plus(moveOffset))
            } else if (pressedButton && pressedHost) {
                var p = pressedHost.panelPointOnRay(thisHand.scenePosition, aimDirection())
                if (p) {
                    var local = pressedButton.mapFromItem(pressedHost.panel, p.x, p.y)
                    pressedButton.vrDragMove(local.x, local.y)
                }
            }
            // Atom under the ray, not while a panel is in front of it or the trigger works a panel.
            // One ray pick per pose update, against the atom model only.
            hoverAtom = active && !pressing ? atomUnderRay() : -1
            thisHand.vrScene.setHighlight(highlightKey, grabAtom >= 0 ? grabAtom : hoverAtom, grabAtom >= 0)
            var button = hit ? hit.button : null
            if (button === hoverButton)
                return
            if (hoverButton)
                hoverButton.rayHoverCount--
            hoverButton = button
            if (hoverButton)
                hoverButton.rayHoverCount++
            if (pressedButton && !pressedHost)
                pressedButton.rayDown = pressedButton === hoverButton
        }

        function setHoverHandle(panelHost) {
            if (panelHost === hoverHandleHost)
                return
            if (hoverHandleHost)
                hoverHandleHost.handleHoverCount--
            hoverHandleHost = panelHost
            if (hoverHandleHost)
                hoverHandleHost.handleHoverCount++
        }
        onScenePositionChanged: {
            updatePanelHit()
            if (movingMolecule)
                carryMolecule()
        }
        onActiveChanged: updatePanelHit()   // off: drops the hover light of the button under the ray
        onSceneRotationChanged: {
            updatePanelHit()
            if (movingMolecule)
                carryMolecule()
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
                    var hit = panelHitResult
                    if (hit && hit.handle) {
                        // hold the panel by its handle: keep the ray point's offset to its centre
                        movingHost = hit.host
                        moveDistance = hit.distance
                        var grabPoint = thisHand.scenePosition.plus(aimDirection().times(hit.distance))
                        moveOffset = movingHost.centreScenePosition().minus(grabPoint)
                        movingHost.handleHeld = true
                        return
                    }
                    pressedButton = hoverButton && hoverButton.enabled ? hoverButton : null
                    if (pressedButton)
                        pressedButton.rayDown = true
                    if (pressedButton && pressedButton.vrDragTarget === true && hit) {
                        pressedHost = hit.host
                        var local = pressedButton.mapFromItem(hit.host.panel, hit.x, hit.y)
                        pressedButton.vrDragStart(local.x, local.y)
                    }
                    return
                }
                if (!vr.simulationActive) {
                    // carry the molecule, keeping the anchor's offset from the ray point
                    var distance = moleculeDistanceOnRay()
                    if (distance >= 0) {
                        moleculeGrabDistance = distance
                        var grabAt = thisHand.scenePosition.plus(aimDirection().times(distance))
                        moleculeGrabOffset = thisHand.vrScene.moleculeAnchor.scenePosition.minus(grabAt)
                        movingMolecule = true
                    }
                    return
                }
                var atom = atomUnderRay()
                if (atom >= 0)
                    grabAtom = atom
            } else {
                // Click = trigger released on the button it went down on (a mouse click's rule).
                var button = pressedButton
                var dragged = pressedHost !== null
                pressedButton = null
                pressedHost = null
                pressing = false
                movingMolecule = false
                if (movingHost) {
                    movingHost.handleHeld = false
                    movingHost = null
                }
                if (button) {
                    button.rayDown = false
                    if (dragged)
                        button.vrDragEnd()   // a drag target is never clicked
                    else if (button === hoverButton && button.enabled && active)   // not when the mode switched mid-press
                        button.clicked()
                }
                if (grabAtom >= 0) {
                    grabAtom = -1
                    vr.grabReleased(thisHand.grabSource)
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
                vr.grabForce(thisHand.grabSource, thisHand.grabAtom, thisHand.vrScene.atomsNode.mapPositionFromScene(target).minus(atom))
            }
        }

        // Aim ray along -Z (Cylinder axis is Y, 100 units tall). It ends on the panel it points at,
        // so the user sees exactly which button the trigger will press.
        Model {
            readonly property real length: thisHand.movingHost ? thisHand.moveDistance
                                         : thisHand.movingMolecule ? thisHand.moleculeGrabDistance
                                         : thisHand.pointsAtPanel ? thisHand.panelDistance : 150
            source: "#Cylinder"
            position: Qt.vector3d(0, 0, -length / 2)
            eulerRotation.x: 90
            scale: Qt.vector3d(0.004, length / 100, 0.004)
            materials: PrincipledMaterial {
                baseColor: thisHand.pointsAtPanel ? Style.rayPanel : Style.rayIdle
                lighting: PrincipledMaterial.NoLighting
            }
        }
        // Marker: red while holding an atom, purple while carrying the molecule, orange while rotating,
        // blue on a panel, green idle.
        Model {
            source: "#Sphere"
            scale: Qt.vector3d(0.03, 0.03, 0.03)
            materials: PrincipledMaterial {
                baseColor: thisHand.grabAtom >= 0 ? Style.markerGrab
                         : thisHand.movingMolecule ? Style.markerMove
                         : thisHand.rotating ? Style.markerRotate
                         : thisHand.pointsAtPanel || thisHand.pressing || thisHand.movingHost ? Style.markerPanel : Style.markerIdle
                lighting: PrincipledMaterial.NoLighting
            }
        }
    }
}
