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
// The three console panels sit below the molecule, where a left-hand ray reaches them from a relaxed
// arm; the file browser opens where the user is already looking.
import QtQuick
import QtQuick3D

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
    property bool filesVisible: false

    readonly property real defaultMoleculeSize: 40
    /// Node the right hand rotates (its parent only translates, so world and local rotation agree).
    property alias moleculeAnchor: anchorNode
    /// Atom coordinates (Angstrom) live in this node; used to map grab targets.
    property alias atomsNode: atomNode
    /// The atom sphere Model; a ray pick on it gives an instance row (map it with atomIndexForInstance).
    readonly property Model atomModel: moleculeModels.atomModel

    signal recenterRequested()

    function resetView() {
        anchorNode.rotation = Qt.quaternion(1, 0, 0, 0)
        moleculeSize = defaultMoleculeSize
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
            position: Qt.vector3d(-80 * Math.sin(yawRad), 0, -80 * Math.cos(yawRad))

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
                yaw: 0
                pitch: -28
                distance: 58
                panel: VrPanelStructure {
                    session: scene.session
                    filesOpen: scene.filesVisible
                    onFilesToggled: scene.filesVisible = !scene.filesVisible
                }
            }

            VrPanelHost {
                yaw: 37
                pitch: -28
                distance: 58
                panel: VrPanelSimulation {
                    session: scene.session
                }
            }

            VrPanelHost {
                yaw: -34
                pitch: -28
                distance: 58
                panel: VrPanelView {
                    moleculeSize: scene.moleculeSize
                    onSizeDown: scene.moleculeSize = Math.max(10, scene.moleculeSize * 0.8)
                    onSizeUp: scene.moleculeSize = Math.min(150, scene.moleculeSize * 1.25)
                    onResetRotation: scene.resetView()
                    onRecenter: scene.recenterRequested()
                }
            }

            VrPanelHost {
                visible: scene.filesVisible
                yaw: 0
                pitch: 5
                distance: 55
                panel: VrPanelFiles {
                    session: scene.session
                    onCloseRequested: scene.filesVisible = false
                }
            }
        }
    }
}
