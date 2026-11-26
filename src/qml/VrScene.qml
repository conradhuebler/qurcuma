// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Headset scene content without anything XR-specific (Claude Generated 2026): the molecule and the
// panels, laid out around the head. viewerxr.qml hosts it in the XrView (controllers, input, head
// tracking); spikes/vr_preview hosts it in a plain View3D with mock objects.
//
// The molecule is the desktop viewer's: MoleculeModels.qml with the desktop SceneController for
// flags and bounds and the mirrored instance tables (XrSceneBridge) for the geometry. Its rotation
// is this scene's own (moleculeAnchor, turned by the right hand), never the desktop rootRotation.
//
// Units are centimetres. Layout around the head (yaw > 0 = left, pitch > 0 = up). Nothing covers the
// molecule at its default size (UX test 2026-10, Claude Generated 2026): the pitches below are derived
// from its bounding sphere and the panels' sizes (layoutPitch functions), so they follow text size.
//   molecule        straight ahead, 90 cm, Style.moleculePitch above eye height; moleculeSize is
//                   the diameter of its bounding sphere (default 40 cm)
//   Menu bar        yaw 0, 50 cm, just below the molecule's sphere; its handle is at its right end
//   Structure       yaw 0, 58 cm, below the menu bar     frames, selection, "Open structure…"
//   Simulation      left of Structure, same pitch         MD/Opt/Pause/Stop, temperature, grab strength
//   View            right of Structure, same pitch        size, speed, text size, reset, recenter
//                   (side yaws from the panel widths: edges Style.panelGapDegrees apart, besideYaw;
//                   the console pitch never goes below Style.consoleLowestPitch)
//   How to use      beside the molecule on the left, 60 cm, above the menu bar; open at start
//   Tutorial        same spot, on demand (menu bar, Help, desktop View ▸ Start VR Tutorial)
//   Open structure  same spot; a child of Structure, so it moves along with it
//                   (help, tutorial and file browser share the spot: one closes the others)
//   Surroundings    world-fixed at the floor, does not follow "Recenter" (a room stays a room)
// The three console panels sit below the molecule, where a controller ray reaches them from a relaxed
// arm; help, tutorial and file browser stand next to the molecule, in view while the user works on
// it. The menu bar never hides, so hidden panels are always one click away. Every panel can be moved
// by its handle; "Recenter" puts them back.
// Hand input (session.handInput, Claude Generated 2026) touches instead of pointing, so everything
// moves within arm's reach: molecule 65 cm, console panels 45 cm, side panels 45 cm, menu bar 40 cm.
// The hands can also carry the molecule (moleculeOffset).
import QtQuick
import QtQuick3D
import QtQuick3D.AssetUtils
import "VrStyle.js" as Style

Node {
    id: scene

    /// VrSession (headset) or MockVr (preview): status properties, command(), file browser calls.
    required property var session
    /// SceneController (headset) or MockMolecule (preview): flags, scene bounds, atom positions.
    required property var sceneController
    /// XrSceneBridge (headset) or MockMolecule.tables (preview): the instance tables, under the same
    /// property names as SceneController's. Never the controller itself: its tables belong to the
    /// desktop window.
    required property var instancing

    /// Head position and heading (yaw, degrees) the layout is built around; the XR host sets them
    /// from the tracked head at session start and on "Recenter".
    property vector3d headPosition: Qt.vector3d(0, 120, 0)
    property real headYaw: 0
    property real moleculeSize: 40   // cm
    readonly property real minMoleculeSize: 10
    readonly property real maxMoleculeSize: 150
    /// Tracked hands instead of controllers: panels and molecule within reach (Claude Generated 2026).
    readonly property bool handInput: session.handInput === true
    readonly property real moleculeDistance: handInput ? 65 : 90

    // ---- comfort and layout (Claude Generated 2026) ------------------------------------------------
    /// Text size factor of every panel (Style.textScales[session.textSizeLevel])
    readonly property real textScale: Style.textScales[Math.max(0, Math.min(Style.textScales.length - 1,
                                                                              session.textSizeLevel || 0))]
    readonly property real menuDistance: handInput ? 40 : 50
    readonly property real consoleDistance: handInput ? 45 : 58
    readonly property real infoDistance: handInput ? 45 : 60
    /// Angle (degrees) under which half of a length @p cm appears at @p distance cm.
    function halfAngle(cm, distance) {
        return Math.atan(cm / 2 / distance) * 180 / Math.PI
    }
    /// Half the angle the molecule's bounding sphere at its default size spans: the layout keeps it
    /// clear. A molecule the user enlarges may then reach the panels, which is their choice.
    readonly property real moleculeHalfAngle: Math.asin(Math.min(1, defaultMoleculeSize / 2 / moleculeDistance)) * 180 / Math.PI
    readonly property real moleculeBottomPitch: Style.moleculePitch - moleculeHalfAngle
    /// Menu bar just below the molecule's sphere; the console panels below the menu bar.
    readonly property real menuPitch: moleculeBottomPitch - Style.layoutGapDegrees
                                      - halfAngle(menuHost.sceneHeight, menuDistance)
    readonly property real menuBottomPitch: menuPitch - halfAngle(menuHost.sceneHeight, menuDistance)
    /// Pitch of a console panel: its top edge just below the menu bar, each panel on its own (the tall
    /// Simulation panel sits lower than Structure), never below Style.consoleLowestPitch.
    function consolePitch(host) {
        return Math.max(handInput ? Style.consoleLowestPitchHands : Style.consoleLowestPitch,
                        menuBottomPitch - Style.layoutGapDegrees - halfAngle(host.sceneHeight, consoleDistance))
    }
    /// Help, tutorial, file browser: beside the molecule on the left (yaw > 0), bottom edge above the
    /// menu bar. The yaw keeps the panel's near edge Style.infoGapDegrees from the molecule's sphere.
    function infoYaw(host) {
        return moleculeHalfAngle + Style.infoGapDegrees + halfAngle(host.sceneWidth, infoDistance)
    }
    function infoPitch(host) {
        var half = halfAngle(host.sceneHeight, infoDistance)
        var handle = Math.atan((Style.handleGap + Style.handleHeight) / infoDistance) * 180 / Math.PI   // below the panel
        return Math.max(Style.moleculePitch, menuPitch + halfAngle(menuHost.sceneHeight, menuDistance)
                                             + Style.layoutGapDegrees + handle + half)
    }
    /// Where the hands carried the molecule (cm, scene axes) from its place in the layout; cleared by
    /// "Reset" and "Recenter".
    property vector3d moleculeOffset: Qt.vector3d(0, 0, 0)
    property bool filesVisible: false
    // Claude Generated 2026 - each panel can be hidden from the menu bar, all at once with B / Y
    property bool structureVisible: true
    property bool simulationVisible: true
    property bool viewVisible: true
    /// Help is open when a session starts: most users put on a headset for the first time.
    property bool helpVisible: true
    /// Guided tour (VrPanelTutorial.qml; Claude Generated 2026); startTutorial() opens it at step 1.
    property bool tutorialVisible: false
    /// Surroundings (Claude Generated 2026): a 3D scene file (glTF/GLB/OBJ...) placed around the
    /// user, empty = none; its units in cm (100 = metres, the glTF convention; 1 = centimetres); and
    /// the floor height (cm) it stands on, set by the XR host.
    property url surroundingsSource
    property real surroundingsScale: 100
    property real floorHeight: 0

    readonly property real defaultMoleculeSize: 40
    /// Node the hands rotate (its parent only translates, so world and local rotation agree).
    property alias moleculeAnchor: anchorNode
    /// Atom coordinates (Angstrom) live in this node; used to map grab targets.
    property alias atomsNode: atomNode
    /// Scene centimetres per Angstrom of the molecule as drawn (hand touch margins).
    readonly property real cmPerAngstrom: moleculeRoot.fit
    /// The atom sphere Model; a ray pick on it gives an instance row (map it with atomIndexForInstance).
    readonly property Model atomModel: moleculeModels.atomModel

    signal recenterRequested()

    /// Open the tutorial at its first step (menu bar, Help panel, desktop menu via the session).
    function startTutorial() {
        tutorialPanel.restart()
        tutorialVisible = true
    }
    /// A gesture was done (rotate, carry, zoom, select, walk, turn, panelsShown, panelMove): the
    /// tutorial finishes its step if it waits for it. Called by viewerxr.qml and VrHandInput.qml.
    function reportAction(name) {
        if (tutorialVisible)
            tutorialPanel.actionDone(name)
    }
    /// Scale the molecule about its centre by @p factor, within its size limits (right stick).
    function zoomMolecule(factor) {
        moleculeSize = Math.max(minMoleculeSize, Math.min(maxMoleculeSize, moleculeSize * factor))
    }
    onMoleculeSizeChanged: reportAction("zoom")
    Connections {
        target: scene.session
        ignoreUnknownSignals: true
        function onTutorialRequested() { scene.startTutorial() }
    }

    /// Every panel and the menu bar, for the hit tests; each can be moved by its handle.
    readonly property var panelHosts: [keypadHost, structureHost, simulationHost, viewHost, filesHost, helpHost,
                                       tutorialHost, menuHost]
    /// "Recenter" also brings moved panels back to their place in the layout (Claude Generated 2026).
    onRecenterRequested: {
        for (var i = 0; i < panelHosts.length; ++i)
            panelHosts[i].resetPlacement()
    }

    // ---- atom highlights (Claude Generated 2026) ----------------------------------------------------
    /// Atoms a controller ray or a fingertip is on, per source ("rightController", "leftHand", ...):
    /// { atom, grabbed }; setHighlight() keeps it, highlights is the list drawn (one per atom,
    /// grabbed wins). Plain Models under the atom node (not instancing), so they always carry the
    /// molecule's transform; a frame tick moves them along with MD and trajectory frames.
    property var highlightSources: ({})
    property var highlights: []
    property int highlightTick: 0

    function setHighlight(source, atom, grabbed) {
        var old = highlightSources[source]
        if (old ? (old.atom === atom && old.grabbed === grabbed) : atom < 0)
            return
        if (atom < 0)
            delete highlightSources[source]
        else
            highlightSources[source] = { atom: atom, grabbed: grabbed === true }
        var byAtom = {}
        for (var key in highlightSources) {
            var h = highlightSources[key]
            byAtom[h.atom] = byAtom[h.atom] === true || h.grabbed
        }
        var list = []
        for (var a in byAtom)
            list.push({ atom: Number(a), grabbed: byAtom[a] })
        highlights = list
    }

    FrameAnimation {
        running: scene.highlights.length > 0
        onTriggered: scene.highlightTick++
    }

    // ---- number pad for the thermostat range (Claude Generated 2026) ------------------------------
    /// "min" or "max" while the pad edits that end of the temperature slider, "" = closed.
    property string keypadTarget: ""
    onSimulationVisibleChanged: if (!simulationVisible) keypadTarget = ""

    /// Opacity of the selection and touch shells, pulsing so a selected atom catches the eye.
    property real haloOpacity   // driven by the animation below
    SequentialAnimation on haloOpacity {
        loops: Animation.Infinite
        NumberAnimation { from: 0.25; to: 0.6; duration: 700; easing.type: Easing.InOutSine }
        NumberAnimation { from: 0.6; to: 0.25; duration: 700; easing.type: Easing.InOutSine }
    }

    /// B / Y: hide the three console panels if any is shown, otherwise show all three.
    function togglePanels() {
        var show = !(structureVisible || simulationVisible || viewVisible)
        structureVisible = show
        simulationVisible = show
        viewVisible = show
        if (show)
            reportAction("panelsShown")
    }

    // Help, tutorial and the file browser share the spot beside the molecule: one closes the others.
    onHelpVisibleChanged: if (helpVisible) { filesVisible = false; tutorialVisible = false }
    onFilesVisibleChanged: if (filesVisible) { helpVisible = false; tutorialVisible = false }
    onTutorialVisibleChanged: if (tutorialVisible) { helpVisible = false; filesVisible = false }

    /// Panel (or menu bar, or a handle) a tracked hand's pinch point touches: the VrPanelHost.touchHit
    /// result of the surface closest to the point, null if it touches none. Claude Generated 2026.
    function panelTouch(point) {
        var best = null
        for (var i = 0; i < panelHosts.length; ++i) {
            var hit = panelHosts[i].touchHit(point)
            if (hit && (!best || Math.abs(hit.depth) < Math.abs(best.depth)))
                best = hit
        }
        return best
    }

    /// Nearest visible panel (or the menu bar, or a handle) on a scene-space ray: the VrPanelHost.rayHit
    /// result, null if the ray hits none. Used by both controllers to decide whether the trigger
    /// clicks, drags or moves a panel, or turns the molecule.
    function panelHit(origin, direction) {
        var best = null
        for (var i = 0; i < panelHosts.length; ++i) {
            var hit = panelHosts[i].rayHit(origin, direction)
            if (hit && (!best || hit.distance < best.distance))
                best = hit
        }
        return best
    }

    /// Move the molecule anchor to a scene position, through moleculeOffset (which the layout adds
    /// to the anchor's place). Used by controllers and hands that carry the molecule. Claude Generated 2026.
    function moveMoleculeTo(target) {
        moleculeOffset = moleculeOffset.plus(target.minus(anchorNode.scenePosition))
    }

    /// Yaw (degrees) between the centres of two side-by-side console panels so their edges stay
    /// Style.panelGapDegrees apart: each half width w/2 spans atan(w/2 / r) about the head, with
    /// r = distance * cos(pitch) the horizontal distance of the tilted panel. Claude Generated 2026.
    function besideYaw(widthA, widthB, distance, pitch) {
        var r = distance * Math.cos(pitch * Math.PI / 180)
        var halfA = widthA / 2   // scene cm (VrPanelHost.sceneWidth)
        var halfB = widthB / 2
        return (Math.atan(halfA / r) + Math.atan(halfB / r)) * 180 / Math.PI + Style.panelGapDegrees
    }

    function resetView() {
        anchorNode.rotation = Qt.quaternion(1, 0, 0, 0)
        moleculeSize = defaultMoleculeSize
        moleculeOffset = Qt.vector3d(0, 0, 0)
    }

    // ---- surroundings: loaded at run time, so any scene the user picks works without a rebuild ----
    RuntimeLoader {
        id: surroundings
        source: scene.surroundingsSource
        visible: scene.surroundingsSource.toString() !== ""
        y: scene.floorHeight
        scale: Qt.vector3d(scene.surroundingsScale, scene.surroundingsScale, scene.surroundingsScale)
        onStatusChanged: if (status === RuntimeLoader.Error)
            console.warn("VR surroundings could not be loaded:", errorString)
    }

    DirectionalLight { eulerRotation.x: -40; eulerRotation.y: -50; brightness: 1.0 }
    DirectionalLight { eulerRotation.x: 25; eulerRotation.y: 130; brightness: 0.35 }

    Node {
        id: stage
        position: scene.headPosition

        // ---- molecule ----------------------------------------------------------------------------
        Node {
            id: anchorNode
            readonly property real yawRad: scene.headYaw * Math.PI / 180
            readonly property real pitchRad: Style.moleculePitch * Math.PI / 180
            readonly property real flat: scene.moleculeDistance * Math.cos(pitchRad)
            position: Qt.vector3d(-flat * Math.sin(yawRad), scene.moleculeDistance * Math.sin(pitchRad),
                                  -flat * Math.cos(yawRad)).plus(scene.moleculeOffset)

            Node {
                id: moleculeRoot
                // sceneExtent is the bounding sphere's radius (half the box diagonal, Angstrom);
                // moleculeSize is its diameter in cm (Claude Generated 2026: was fitted to the radius,
                // which drew the molecule twice the size the View panel showed)
                property real fit: scene.moleculeSize / (2 * Math.max(scene.sceneController.sceneExtent, 0.5))
                scale: Qt.vector3d(fit, fit, fit)

                // centres the structure on the anchor; local coordinates here = atom coordinates (Angstrom)
                Node {
                    id: atomNode
                    position: Qt.vector3d(-scene.sceneController.sceneCenter.x,
                                          -scene.sceneController.sceneCenter.y,
                                          -scene.sceneController.sceneCenter.z)
                    MoleculeModels {
                        id: moleculeModels
                        ctl: scene.sceneController
                        inst: scene.instancing
                        atomsPickable: true   // required for rayPick
                    }

                    // Selected atoms (Claude Generated 2026): a pulsing see-through shell around
                    // each, unlit so it reads the same in any light or sky box. Shown only with a
                    // selection, and moved on showing: Qt 6.11 draws a Model that had nothing to
                    // draw at the first sync without its parents' transform (ToggledModel in
                    // MoleculeModels.qml), which put the first shells tiny at the scene origin.
                    Model {
                        visible: scene.sceneController.selectionCount > 0
                        position: visible ? Qt.vector3d(0, 0, 0) : Qt.vector3d(0, 0, 1e-3)
                        source: "#Sphere"
                        instancing: scene.instancing.selectionHaloInstancing
                        pickable: false
                        materials: PrincipledMaterial {
                            baseColor: Style.selectionHalo
                            lighting: PrincipledMaterial.NoLighting
                            alphaMode: PrincipledMaterial.Blend
                            depthDrawMode: Material.NeverDepthDraw
                            opacity: scene.haloOpacity
                        }
                    }

                    // Atom under a controller ray or a fingertip: a bright shell sized from the
                    // radius the atom is drawn with (space-filling, ball-and-stick, atom scale), so
                    // it hugs the atom in every representation; red while that atom is pulled.
                    Repeater3D {
                        model: scene.highlights
                        delegate: Model {
                            required property var modelData
                            readonly property real drawnRadius: scene.highlightTick >= 0
                                ? scene.sceneController.atomDrawnRadius(modelData.atom) : 0
                            readonly property real radius: Math.max(drawnRadius * Style.highlightScale,
                                                                    drawnRadius + Style.highlightMargin)
                            visible: drawnRadius > 0
                            position: scene.highlightTick >= 0 ? scene.sceneController.atomPosition(modelData.atom)
                                                               : Qt.vector3d(0, 0, 0)
                            source: "#Sphere"
                            scale: Qt.vector3d(radius / 50, radius / 50, radius / 50)   // #Sphere: radius 50
                            pickable: false
                            materials: PrincipledMaterial {
                                baseColor: modelData.grabbed ? Style.markerGrab : Style.highlight
                                lighting: PrincipledMaterial.NoLighting
                                alphaMode: PrincipledMaterial.Blend
                                depthDrawMode: Material.NeverDepthDraw
                                opacity: 0.35 + 0.5 * scene.haloOpacity
                            }
                        }
                    }
                }
            }
        }

        // ---- panels ------------------------------------------------------------------------------
        Node {
            eulerRotation.y: scene.headYaw

            VrPanelHost {
                id: structureHost
                visible: scene.structureVisible
                uiScale: scene.textScale
                yaw: 0
                pitch: scene.consolePitch(structureHost)
                distance: scene.consoleDistance
                panel: VrPanelStructure {
                    session: scene.session
                    filesOpen: scene.filesVisible
                    onFilesToggled: scene.filesVisible = !scene.filesVisible
                }
            }

            VrPanelHost {
                id: simulationHost
                visible: scene.simulationVisible
                uiScale: scene.textScale
                yaw: scene.besideYaw(sceneWidth, structureHost.sceneWidth, distance, pitch)
                pitch: scene.consolePitch(simulationHost)
                distance: scene.consoleDistance
                panel: VrPanelSimulation {
                    session: scene.session
                    onTemperatureEditRequested: (which) => scene.keypadTarget = which
                }
            }

            VrPanelHost {
                id: viewHost
                visible: scene.viewVisible
                uiScale: scene.textScale
                yaw: -scene.besideYaw(sceneWidth, structureHost.sceneWidth, distance, pitch)
                pitch: scene.consolePitch(viewHost)
                distance: scene.consoleDistance
                panel: VrPanelView {
                    moleculeSize: scene.moleculeSize
                    handInput: scene.handInput
                    walkSpeedLevel: scene.session.walkSpeedLevel || 0
                    textSizeLevel: scene.session.textSizeLevel || 0
                    onWalkSpeedChosen: (level) => scene.session.walkSpeedLevel = level
                    onTextSizeChosen: (level) => scene.session.textSizeLevel = level
                    onSizeDown: scene.zoomMolecule(0.8)
                    onSizeUp: scene.zoomMolecule(1.25)
                    onInputChosen: (hands) => scene.session.handInput = hands
                    onResetRotation: scene.resetView()
                    onRecenter: scene.recenterRequested()
                }
            }

            // Claude Generated 2026 - child of the Structure panel (its "Open structure…" button):
            // placed relative to it, follows it when it is moved, hidden with it.
            VrPanelHost {
                id: filesHost
                anchorHost: structureHost
                visible: scene.filesVisible && scene.structureVisible
                uiScale: scene.textScale
                // beside the molecule, as offsets from the Structure panel's place in the layout
                yaw: scene.infoYaw(filesHost) - structureHost.yaw
                pitch: scene.infoPitch(filesHost) - structureHost.pitch
                distance: scene.infoDistance - structureHost.distance
                panel: VrPanelFiles {
                    session: scene.session
                    onCloseRequested: scene.filesVisible = false
                }
            }

            VrPanelHost {
                id: helpHost
                visible: scene.helpVisible
                uiScale: scene.textScale
                yaw: scene.infoYaw(helpHost)
                pitch: scene.infoPitch(helpHost)
                distance: scene.infoDistance
                panel: VrPanelHelp {
                    handInput: scene.handInput
                    onCloseRequested: scene.helpVisible = false
                    onTutorialRequested: scene.startTutorial()
                }
            }

            // Claude Generated 2026 - guided tour of the controls, same spot as the help
            VrPanelHost {
                id: tutorialHost
                visible: scene.tutorialVisible
                uiScale: scene.textScale
                yaw: scene.infoYaw(tutorialHost)
                pitch: scene.infoPitch(tutorialHost)
                distance: scene.infoDistance
                panel: VrPanelTutorial {
                    id: tutorialPanel
                    handInput: scene.handInput
                    simulationActive: scene.session.simulationActive === true
                    onCloseRequested: scene.tutorialVisible = false
                    onHelpRequested: scene.helpVisible = true
                }
            }

            // Number pad: in front of the Simulation panel (wherever it was moved), a little closer,
            // so it covers the panel while it is open.
            VrPanelHost {
                id: keypadHost
                visible: scene.keypadTarget !== ""
                uiScale: scene.textScale
                yaw: simulationHost.placeYaw
                pitch: simulationHost.placePitch + 4
                distance: simulationHost.placeDistance - 6
                panel: VrNumberPad {
                    readonly property bool editsMax: scene.keypadTarget === "max"
                    title: editsMax ? "Max temperature" : "Min temperature"
                    unit: "K"
                    initialValue: editsMax ? scene.session.temperatureMax : scene.session.temperatureMin
                    maximumValue: 100000
                    minimumValue: editsMax ? scene.session.temperatureMin + 1 : 0
                    rangeHint: editsMax ? "above the minimum (" + scene.session.temperatureMin.toFixed(0) + " K)"
                                        : "0 K or more"
                    open: scene.keypadTarget !== ""
                    onAccepted: (value) => {
                        if (editsMax)
                            scene.session.command("simTempRange:" + scene.session.temperatureMin.toFixed(0) + ":" + value.toFixed(0))
                        else   // a minimum at or above the maximum pushes the maximum up (as the dock does)
                            scene.session.command("simTempRange:" + value.toFixed(0) + ":"
                                                  + Math.max(scene.session.temperatureMax, value + 1).toFixed(0))
                        scene.keypadTarget = ""
                    }
                    onCancelled: scene.keypadTarget = ""
                }
            }

            // closer than the panels, so its buttons win where it overlaps one
            VrPanelHost {
                id: menuHost
                handleRight: true   // above it is the molecule, below it the console panels
                uiScale: scene.textScale
                yaw: 0
                pitch: scene.menuPitch
                distance: scene.menuDistance
                panel: VrMenuBar {
                    vrScene: scene
                }
            }
        }
    }
}
