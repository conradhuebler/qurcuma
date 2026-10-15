// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Tracked-hand input of the headset scene (Claude Generated 2026): the alternative to the controller
// rays of viewerxr.qml, chosen by vr.handInput. Direct manipulation only: no ray picks. A pinch acts
// only on what the pinch point touches when the pinch starts, a button (Style.touchFront/Behind of
// its panel) or a drawn atom sphere (Style.atomTouch, SceneController::atomAtPoint); a pinch that
// touches nothing does nothing until it is let go. VrScene moves panels and molecule within reach.
//
// Both hands work the same. A hand's pinch point is the midpoint between thumb tip and index tip, so
// it marks where a pinch lands; a small sphere shows it:
//   pinch on a panel button             = click (on release, on the same button: a mouse click's rule)
//   pinch on the thermostat track, move = drag the temperature
//   pinch on a panel's handle, move     = move the panel (it keeps facing the head)
//   pinch and hold an atom, simulation  = pull it with a spring force (one atom at a time); the
//                                         molecule itself never moves while a simulation runs
//   pinch and hold an atom, otherwise   = hold the molecule: it moves and turns with the hand
//   both hands holding atoms, pull apart = resize the molecule about the point between the hands
//   quick pinch (tap) on an atom        = select it
// A pinch is read from the tracked joints (thumb-to-index distance with hysteresis, Style.pinch*),
// not from a runtime pinch action, so any runtime with XR_EXT_hand_tracking works.
//
// Qt hands the joints over in XrOrigin space (cm), in OpenXR order (XrHandJointEXT): 0 palm, 1 wrist,
// 2-5 thumb (5 = tip), 6-10 index (10 = tip), 11-15 middle, 16-20 ring, 21-25 little. This node is a
// direct child of the XrOrigin, so its local space is that space.
import QtQuick
import QtQuick3D
import QtQuick3D.Xr
import "VrStyle.js" as Style

Node {
    id: root

    required property var vrScene    // VrScene: panels, molecule anchor, atoms
    required property var session    // VrSession: atomPicked, grabForce, grabReleased
    property bool active: false

    /// The hand pulling an atom (null = none); VrSession knows a single grab only.
    property var grabHand: null

    TrackedHand { id: rightHand; side: XrController.RightHand }
    TrackedHand { id: leftHand; side: XrController.LeftHand }

    // ---- both hands holding: resize about the point between them ---------------------------------
    readonly property bool twoHanded: rightHand.holding && leftHand.holding
    property real lastSpread: 0   // 0 = start anew with the next update
    property vector3d lastMiddle
    onTwoHandedChanged: lastSpread = 0

    /// Scale the molecule by the change of the distance between the pinch points, about their
    /// midpoint, which also carries it: anchor' = middle + s (anchor - lastMiddle).
    function spread() {
        var middle = rightHand.pinchPoint.plus(leftHand.pinchPoint).times(0.5)
        var gap = rightHand.pinchPoint.minus(leftHand.pinchPoint).length()
        if (lastSpread > 0 && gap > 0) {
            var oldSize = vrScene.moleculeSize
            var newSize = Math.max(vrScene.minMoleculeSize, Math.min(vrScene.maxMoleculeSize, oldSize * gap / lastSpread))
            var s = newSize / oldSize
            vrScene.moleculeSize = newSize
            carry(middle.plus(vrScene.moleculeAnchor.scenePosition.minus(lastMiddle).times(s)))
        }
        lastMiddle = middle
        lastSpread = gap
    }

    /// Move the molecule anchor to a scene position (through VrScene.moleculeOffset, which the
    /// layout adds to the anchor's place).
    function carry(target) {
        vrScene.moleculeOffset = vrScene.moleculeOffset.plus(target.minus(vrScene.moleculeAnchor.scenePosition))
    }

    // ---- one tracked hand -------------------------------------------------------------------------
    component TrackedHand: Node {
        id: hand

        /// XrController.LeftHand or RightHand
        required property int side

        /// joint data arrived within Style.handLostMilliseconds
        property bool fresh: false
        readonly property bool tracked: root.active && fresh && jointList.length >= 26
        /// joint positions (XrOrigin space), converted from the C++ list once per update
        property var jointList: []

        /// pinch point (scene), the palm's orientation (scene) and thumb-to-index distance (cm)
        property vector3d pinchPoint
        property vector3d pinchPointLocal
        property quaternion palmRotation: Qt.quaternion(1, 0, 0, 0)
        property real pinchGap: 100
        property bool pinching: false

        /// What the pinch point is at: a panel (and its button), or an atom (-1 = none)
        property bool onPanel: false
        property Item hoverButton: null
        /// What the current pinch does: "" (no pinch), "button", "slide" (a drag target such as the
        /// thermostat track), "move" (a panel by its handle), "hold" (the molecule), "atom" (pull)
        /// or "none" (it started touching nothing, or an atom the other hand already pulls)
        property string action: ""
        readonly property bool holding: action === "hold"
        property Item pressedButton: null
        /// last panel touch (VrPanelHost.touchHit), the panel whose handle the pinch point touches,
        /// and the panel a "slide" or "move" works on, with the pinch's offset to the panel centre
        property var panelHitResult: null
        property var hoverHandleHost: null
        property var actionHost: null
        property vector3d moveOffset
        property int hoverAtom: -1
        property int grabAtom: -1
        /// pinch point minus atom at the start of a pull (Angstrom), so the atom is not yanked onto it
        property vector3d grabOffset
        /// tap detection: atom under the pinch when it started, its time and place
        property int tapAtom: -1
        property real pressTime: 0
        property vector3d pressPoint
        /// key of this hand's atom highlight in VrScene.setHighlight
        readonly property string highlightKey: side === XrController.LeftHand ? "leftHand" : "rightHand"
        /// pinch point and palm orientation of the previous update (hold = follow their change)
        property vector3d lastPoint
        property quaternion lastRotation: Qt.quaternion(1, 0, 0, 0)

        XrController {
            id: joints
            controller: hand.side
            onJointPositionsChanged: hand.update()
        }
        Timer {
            id: lostTimer
            interval: Style.handLostMilliseconds
            onTriggered: hand.fresh = false
        }
        onTrackedChanged: if (!tracked) cancel()

        function update() {
            fresh = true
            lostTimer.restart()
            if (!root.active)
                return
            jointList = joints.jointPositions
            if (!tracked)
                return
            var j = jointList
            var thumbTip = j[5]
            var indexTip = j[10]
            pinchGap = thumbTip.minus(indexTip).length()
            pinchPointLocal = thumbTip.plus(indexTip).times(0.5)
            pinchPoint = root.mapPositionToScene(pinchPointLocal)
            var r = joints.jointRotations
            if (r.length > 0)
                palmRotation = root.sceneRotation.times(r[0])

            updateTarget()
            var nowPinching = pinching ? pinchGap < Style.pinchRelease : pinchGap < Style.pinchPress
            if (nowPinching !== pinching) {
                pinching = nowPinching
                if (pinching)
                    press()
                else
                    release(false)
            } else if (pinching) {
                drag()
            }
            if (root.twoHanded && !root.session.simulationActive)
                root.spread()
            updateHighlight()
            lastPoint = pinchPoint
            lastRotation = palmRotation
        }

        // Panel or atom at the pinch point. Lights the button there (rayHoverCount: two hands, and
        // a controller ray, may be on it) and shows a held button as pressed only while the pinch
        // stays on it.
        function updateTarget() {
            var hit = root.vrScene.panelTouch(pinchPoint)
            panelHitResult = hit
            onPanel = hit !== null
            setHoverButton(hit ? hit.button : null)
            setHoverHandle(hit && hit.handle ? hit.host : null)
            if (pressedButton && action === "button")
                pressedButton.rayDown = pressedButton === hoverButton
            hoverAtom = onPanel || action !== "" ? -1 : atomAtPinch()
        }

        // Highlight the atom this hand would take or is pulling (VrScene draws it).
        function updateHighlight() {
            root.vrScene.setHighlight(highlightKey, grabAtom >= 0 ? grabAtom : hoverAtom, grabAtom >= 0)
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

        function setHoverButton(button) {
            if (button === hoverButton)
                return
            if (hoverButton)
                hoverButton.rayHoverCount--
            hoverButton = button
            if (hoverButton)
                hoverButton.rayHoverCount++
        }

        // Atom the pinch point touches (inside its drawn sphere, or within Style.atomTouch of its
        // surface), -1 if none. A plain overlap test in atom coordinates, no ray.
        function atomAtPinch() {
            var point = root.vrScene.atomsNode.mapPositionFromScene(pinchPoint)
            return root.vrScene.sceneController.atomAtPoint(point, Style.atomTouch / root.vrScene.cmPerAngstrom)
        }

        function press() {
            pressTime = Date.now()
            pressPoint = pinchPoint
            tapAtom = -1
            if (onPanel) {
                var hit = panelHitResult
                if (hit.handle) {
                    // hold the panel by its handle: keep the pinch's offset to the panel centre
                    action = "move"
                    actionHost = hit.host
                    moveOffset = actionHost.centreScenePosition().minus(pinchPoint)
                    actionHost.handleHeld = true
                    return
                }
                pressedButton = hoverButton && hoverButton.enabled ? hoverButton : null
                action = !pressedButton ? "none" : pressedButton.vrDragTarget === true ? "slide" : "button"
                if (pressedButton)
                    pressedButton.rayDown = true
                if (action === "slide") {
                    actionHost = hit.host
                    var local = pressedButton.mapFromItem(hit.host.panel, hit.x, hit.y)
                    pressedButton.vrDragStart(local.x, local.y)
                }
                return
            }
            if (hoverAtom < 0) {
                action = "none"   // an accidental pinch in the air
                return
            }
            tapAtom = hoverAtom
            if (root.session.simulationActive) {
                // a running simulation: atoms are pulled, the molecule stays where it is
                if (root.grabHand !== null) {
                    action = "none"
                    return
                }
                action = "atom"
                grabAtom = hoverAtom
                root.grabHand = hand
                var atom = root.vrScene.sceneController.atomPosition(grabAtom)
                grabOffset = root.vrScene.atomsNode.mapPositionFromScene(pinchPoint).minus(atom)
                return
            }
            action = "hold"
        }

        // Hold, one hand: the molecule turns with the palm about the pinch point and moves with it,
        // anchor' = pinch + d (anchor - lastPinch), rotation' = d rotation, d = palm * lastPalm^-1.
        // With both hands holding, spread() moves it instead; a simulation started meanwhile stops it.
        // Slide: the pinch point, projected onto the panel, drives the drag target (also off the
        // track, like a mouse drag). Move: the panel centre follows the pinch point.
        function drag() {
            if (action === "slide" && pressedButton) {
                var p = actionHost.panelPointAt(pinchPoint)
                var local = pressedButton.mapFromItem(actionHost.panel, p.x, p.y)
                pressedButton.vrDragMove(local.x, local.y)
                return
            }
            if (action === "move") {
                actionHost.moveCentreTo(pinchPoint.plus(moveOffset))
                return
            }
            if (action !== "hold" || root.twoHanded || root.session.simulationActive)
                return
            var d = palmRotation.times(lastRotation.inverted())
            var anchor = root.vrScene.moleculeAnchor
            anchor.rotation = d.times(anchor.rotation)
            root.carry(pinchPoint.plus(d.times(anchor.scenePosition.minus(lastPoint))))
        }

        /// End the pinch; @p cancelled (hand lost, mode switched) never clicks or selects.
        function release(cancelled) {
            var tap = !cancelled && tapAtom >= 0 && Date.now() - pressTime < Style.tapMilliseconds
                    && pinchPoint.minus(pressPoint).length() < Style.tapDistance
            var button = pressedButton
            pressedButton = null
            if (button) {
                button.rayDown = false
                if (action === "slide")
                    button.vrDragEnd()   // a drag target is never clicked
                else if (!cancelled && button === hoverButton && button.enabled)
                    button.clicked()
            }
            if (action === "move" && actionHost)
                actionHost.handleHeld = false
            actionHost = null
            if (grabAtom >= 0) {
                grabAtom = -1
                root.grabHand = null
                root.session.grabReleased()
            }
            action = ""
            if (tap)
                root.session.atomPicked(tapAtom)
        }

        function cancel() {
            if (pinching)
                release(true)
            pinching = false
            setHoverButton(null)
            setHoverHandle(null)
            onPanel = false
            hoverAtom = -1
            root.vrScene.setHighlight(highlightKey, -1, false)
        }

        // Spring target of a pull: the pinch point (less the offset at the start), in atom
        // coordinates; same rate and signal as the controller grab.
        Timer {
            interval: 33
            repeat: true
            running: hand.grabAtom >= 0
            onTriggered: {
                var atom = root.vrScene.sceneController.atomPosition(hand.grabAtom)
                var target = root.vrScene.atomsNode.mapPositionFromScene(hand.pinchPoint).minus(hand.grabOffset)
                root.session.grabForce(hand.grabAtom, target.minus(atom))
            }
        }

        // ---- what the user sees: joints and the pinch point -------------------------------------
        // Spheres on the joints rather than XrHandModel: the hand mesh needs a further runtime
        // extension (XR_FB_hand_tracking_mesh), the joints come with every hand tracker.
        Repeater3D {
            model: hand.tracked ? 26 : 0
            delegate: Model {
                position: hand.jointList[index] ?? Qt.vector3d(0, 0, 0)
                source: "#Sphere"
                scale: Qt.vector3d(0.008, 0.008, 0.008)
                materials: PrincipledMaterial {
                    baseColor: Style.handJoint
                    roughness: 0.6
                }
            }
        }
        // Pinch point: red while pulling an atom, orange while holding the molecule, blue on a
        // panel (or moving one), yellow touching an atom, green otherwise (a pinch now does nothing).
        Model {
            visible: hand.tracked
            position: hand.pinchPointLocal
            source: "#Sphere"
            readonly property real size: hand.pinching ? 0.009 : 0.014
            scale: Qt.vector3d(size, size, size)
            materials: PrincipledMaterial {
                baseColor: hand.action === "atom" ? Style.markerGrab
                         : hand.action === "hold" ? Style.markerRotate
                         : hand.onPanel || hand.action === "button" || hand.action === "slide" || hand.action === "move" ? Style.markerPanel
                         : hand.hoverAtom >= 0 ? Style.markerAtom : Style.markerIdle
                lighting: PrincipledMaterial.NoLighting
            }
        }
    }
}
