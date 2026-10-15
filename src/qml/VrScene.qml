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
//   Structure       yaw   0, pitch -28, 58 cm   frames, selection, "Open structure…"
//   Simulation      yaw +37, pitch -28, 58 cm   MD/Opt/Pause/Stop, temperature, grab strength
//   View            yaw -34, pitch -28, 58 cm   size, reset, recenter
//   Open structure  yaw   0, pitch  +5, 55 cm   on demand, in front of the molecule
//   How to use      yaw   0, pitch  +8, 55 cm   help, open at start; shares the spot with the files
//   Menu bar        yaw   0, pitch -12, 50 cm   see-through toggles for the panels and Help
//   Surroundings    world-fixed at the floor, does not follow "Recenter" (a room stays a room)
// The three console panels sit below the molecule, where a controller ray reaches them from a relaxed
// arm; the file browser and the help open where the user is already looking. The menu bar never
// hides, so hidden panels are always one click away.
// Hand input (session.handInput, Claude Generated 2026) touches instead of pointing, so everything
// moves within arm's reach: molecule 65 cm, console panels 45 cm, file browser and help 45 cm, menu
// bar 40 cm (directions unchanged). The hands can also carry the molecule (moleculeOffset).
import QtQuick
import QtQuick3D
import QtQuick3D.AssetUtils

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

    /// Panel (or menu bar) a tracked hand's pinch point touches: { depth, button } of the panel whose
    /// surface is closest to the point, null if it touches none. Claude Generated 2026.
    function panelTouch(point) {
        var hosts = [structureHost, simulationHost, viewHost, filesHost, helpHost, menuHost]
        var best = null
        for (var i = 0; i < hosts.length; ++i) {
            var hit = hosts[i].touchHit(point)
            if (hit && (!best || Math.abs(hit.depth) < Math.abs(best.depth)))
                best = hit
        }
        return best
    }

    /// Nearest visible panel (or the menu bar) on a scene-space ray: { distance (cm), button (the
    /// VrButton under the ray or null) }, null if the ray hits none. Used by both hands to decide
    /// whether the trigger clicks or turns the molecule, and which button it clicks.
    function panelHit(origin, direction) {
        var hosts = [structureHost, simulationHost, viewHost, filesHost, helpHost, menuHost]
        var best = null
        for (var i = 0; i < hosts.length; ++i) {
            var hit = hosts[i].rayHit(origin, direction)
            if (hit && (!best || hit.distance < best.distance))
                best = hit
        }
        return best
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
                pitch: -28
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
                yaw: 37
                pitch: -28
                distance: scene.handInput ? 45 : 58
                panel: VrPanelSimulation {
                    session: scene.session
                }
            }

            VrPanelHost {
                id: viewHost
                visible: scene.viewVisible
                yaw: -34
                pitch: -28
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

            VrPanelHost {
                id: filesHost
                visible: scene.filesVisible
                yaw: 0
                pitch: 5
                distance: scene.handInput ? 45 : 55
                panel: VrPanelFiles {
                    session: scene.session
                    onCloseRequested: scene.filesVisible = false
                }
            }

            VrPanelHost {
                id: helpHost
                visible: scene.helpVisible
                yaw: 0
                pitch: 8   // taller than the file browser: clears the menu bar
                distance: scene.handInput ? 45 : 55
                panel: VrPanelHelp {
                    handInput: scene.handInput
                    onCloseRequested: scene.helpVisible = false
                }
            }

            // closer than the panels, so its buttons win where it overlaps one
            VrPanelHost {
                id: menuHost
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
