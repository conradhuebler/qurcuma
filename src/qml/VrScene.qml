// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Headset scene content without anything XR-specific (Claude Generated 2026): the molecule and the
// panels, laid out around the head. viewerxr.qml hosts it in the XrView (controllers, input, head
// tracking); spikes/vr_preview hosts it in a plain View3D with mock objects.
//
// The molecule is the desktop viewer's: MoleculeModels.qml with the desktop SceneController for
// flags and bounds and the mirrored instance tables (XrSceneBridge) for the geometry. Its rotation
// is this scene's own (moleculeAnchor, turned by the right hand), never the desktop rootRotation.
//
// Units are centimetres. Layout around the head (yaw > 0 = left, pitch > 0 = up):
//   molecule        straight ahead, 80 cm, at eye height, fitted to moleculeSize (default 40 cm)
//   Structure       yaw   0, pitch -28, 58 cm (hands: -32)   frames, selection, "Open structure…"
//   Simulation      yaw ~+45, pitch -28, 58 cm  MD/Opt/Pause/Stop, temperature, grab strength
//   View            yaw ~-35, pitch -28, 58 cm  size, reset, recenter
//                   (side yaws from the panel widths: edges Style.panelGapDegrees apart, besideYaw)
//   Open structure  yaw   0, pitch  +5, 55 cm   on demand, in front of the molecule; a child of
//                                               Structure (offset +33 deg, -3 cm), moves along with it
//   How to use      yaw   0, pitch +17, 55 cm   help, open at start; shares the spot with the files
//   Menu bar        yaw   0, pitch -12, 50 cm   see-through toggles for the panels and Help
//   Surroundings    world-fixed at the floor, does not follow "Recenter" (a room stays a room)
// The three console panels sit below the molecule, where a controller ray reaches them from a relaxed
// arm; the file browser and the help open where the user is already looking. The menu bar never
// hides, so hidden panels are always one click away. Every panel can be moved by the handle below it
// (the menu bar's is above it); "Recenter" puts them back.
// Hand input (session.handInput, Claude Generated 2026) touches instead of pointing, so everything
// moves within arm's reach: molecule 65 cm, console panels 45 cm, file browser and help 45 cm, menu
// bar 40 cm (directions unchanged). The hands can also carry the molecule (moleculeOffset).
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
    readonly property real moleculeDistance: handInput ? 65 : 80
    /// Pitch of the three console panels: lower in hand mode, where the closer panels look taller and
    /// would reach up into the menu bar (Claude Generated 2026).
    readonly property real consolePitch: handInput ? -32 : -28
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

    /// Every panel and the menu bar, for the hit tests; each can be moved by its handle.
    readonly property var panelHosts: [keypadHost, structureHost, simulationHost, viewHost, filesHost, helpHost, menuHost]
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
    }

    // Help and the file browser share the spot in front of the molecule: one closes the other.
    onHelpVisibleChanged: if (helpVisible) filesVisible = false
    onFilesVisibleChanged: if (filesVisible) helpVisible = false

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
        var halfA = widthA / Style.pixelsPerCm / 2
        var halfB = widthB / Style.pixelsPerCm / 2
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
            position: Qt.vector3d(-scene.moleculeDistance * Math.sin(yawRad), 0,
                                  -scene.moleculeDistance * Math.cos(yawRad)).plus(scene.moleculeOffset)

            Node {
                id: moleculeRoot
                property real fit: scene.moleculeSize / Math.max(scene.sceneController.sceneExtent, 1)
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
                yaw: 0
                pitch: scene.consolePitch
                distance: scene.handInput ? 45 : 58
                panel: VrPanelStructure {
                    session: scene.session
                    filesOpen: scene.filesVisible
                    onFilesToggled: scene.filesVisible = !scene.filesVisible
                }
            }

            VrPanelHost {
                id: simulationHost
                visible: scene.simulationVisible
                yaw: scene.besideYaw(panel.width, structureHost.panel.width, distance, pitch)
                pitch: scene.consolePitch
                distance: scene.handInput ? 45 : 58
                panel: VrPanelSimulation {
                    session: scene.session
                    onTemperatureEditRequested: (which) => scene.keypadTarget = which
                }
            }

            VrPanelHost {
                id: viewHost
                visible: scene.viewVisible
                yaw: -scene.besideYaw(panel.width, structureHost.panel.width, distance, pitch)
                pitch: scene.consolePitch
                distance: scene.handInput ? 45 : 58
                panel: VrPanelView {
                    moleculeSize: scene.moleculeSize
                    handInput: scene.handInput
                    onSizeDown: scene.moleculeSize = Math.max(scene.minMoleculeSize, scene.moleculeSize * 0.8)
                    onSizeUp: scene.moleculeSize = Math.min(scene.maxMoleculeSize, scene.moleculeSize * 1.25)
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
                yaw: 0
                pitch: 33   // above the Structure panel and the menu bar: pitch +5 in the layout
                distance: scene.handInput ? 0 : -3
                panel: VrPanelFiles {
                    session: scene.session
                    onCloseRequested: scene.filesVisible = false
                }
            }

            VrPanelHost {
                id: helpHost
                visible: scene.helpVisible
                yaw: 0
                pitch: 17   // taller than the file browser: clears the menu bar
                distance: scene.handInput ? 45 : 55
                panel: VrPanelHelp {
                    handInput: scene.handInput
                    onCloseRequested: scene.helpVisible = false
                }
            }

            // Number pad: in front of the Simulation panel (wherever it was moved), a little closer,
            // so it covers the panel while it is open.
            VrPanelHost {
                id: keypadHost
                visible: scene.keypadTarget !== ""
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
                handleAbove: true   // below it sit the console panels
                yaw: 0
                pitch: -12
                distance: scene.handInput ? 40 : 50
                panel: VrMenuBar {
                    vrScene: scene
                }
            }
        }
    }
}
