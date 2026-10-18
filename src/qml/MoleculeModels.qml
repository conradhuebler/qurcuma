// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Instanced molecule content shared by the desktop scene (viewer3d.qml) and the headset scene
// (VrScene.qml), so both show the same atoms, bonds and overlays. Claude Generated 2026.
//
// Every instance table is in model-local (intrinsic atom) coordinates, Angstrom; the parent node
// places and rotates it (desktop: moleculeRoot with controller.rootRotation; headset: the
// hand-rotated anchor, independent of the desktop rotation). Two objects feed it:
//   ctl  - SceneController: visibility flags, transparency, wall opacity
//   inst - the instance tables: the SceneController itself on the desktop, the XrSceneBridge
//          (one MirrorInstancing per table, src/tableinstancing.h) in the headset window
import QtQuick
import QtQuick3D

Node {
    id: content

    required property var ctl
    required property var inst
    /// Make the atom spheres hit by ray picks (headset); the desktop picks in C++.
    property bool atomsPickable: false
    /// The atom sphere Model, to tell an atom hit from other pick results.
    readonly property Model atomModel: atomSpheres

    // A Model whose `visible` is switched at run time. Qt Quick 3D 6.11 draws a Model that was
    // hidden when its window first synced without its parents' transform once it is shown, until
    // the Model's own transform changes (measured offscreen: a wall or measurement lines switched on
    // after a rotation came out unrotated, and in the headset scene they did not appear at all).
    // The position below changes whenever the Model is shown, which makes Qt compute the transform;
    // the other value only applies while it is hidden. Pinned by test_instancing_mirror.
    component ToggledModel: Model {
        position: visible ? Qt.vector3d(0, 0, 0) : Qt.vector3d(0, 0, 1e-3)
    }

    ToggledModel {
        id: atomSpheres
        source: "#Sphere"
        visible: content.ctl.atomsVisible
        pickable: content.atomsPickable
        instancing: content.inst.atomInstancing
        materials: PrincipledMaterial {
            baseColor: "white"
            metalness: 0.0
            roughness: 0.4
            // Opaque by default (clean depth/no seams); blend only when the
            // user actually requests transparency.
            alphaMode: content.ctl.blendEnabled ? PrincipledMaterial.Blend
                                                : PrincipledMaterial.Opaque
        }
    }

    ToggledModel {
        id: bondCylinders
        source: "#Cylinder"
        visible: content.ctl.bondsVisible
        instancing: content.inst.bondInstancing
        materials: PrincipledMaterial {
            baseColor: "white"
            metalness: 0.0
            roughness: 0.55
            alphaMode: content.ctl.blendEnabled ? PrincipledMaterial.Blend
                                                : PrincipledMaterial.Opaque
        }
    }

    // RMSD overlay: second structure, opaque, with HSV-shifted element
    // colours (set per-instance) so it reads as the "other" molecule while
    // staying element-coded. Shares the molecule's rotation.
    ToggledModel {
        source: "#Sphere"
        visible: content.ctl.overlayVisible
        instancing: content.inst.overlayAtomInstancing
        materials: PrincipledMaterial { baseColor: "white"; roughness: 0.4 }
    }
    ToggledModel {
        source: "#Cylinder"
        visible: content.ctl.overlayVisible
        instancing: content.inst.overlayBondInstancing
        materials: PrincipledMaterial { baseColor: "white"; roughness: 0.55 }
    }

    // Confinement-wall wireframe (harmonic walls from the interactive MD
    // config). Edges are in intrinsic atom coordinates. Unlit flat overlay.
    // The per-segment instance colour carries the grey/red RGB AND the
    // alpha (baked from wallOpacity in SceneController::rebuildWall); the
    // material baseColor stays opaque white and only flips to Blend mode
    // when opacity < 1, otherwise PrincipledMaterial defaults to Opaque and
    // silently discards the alpha (the "immer überdeckend" bug).
    ToggledModel {
        source: "#Cylinder"
        visible: content.ctl.wallVisible
        instancing: content.inst.wallInstancing
        materials: PrincipledMaterial {
            baseColor: "white"
            lighting: PrincipledMaterial.NoLighting
            alphaMode: content.ctl.wallOpacity < 0.999
                       ? PrincipledMaterial.Blend
                       : PrincipledMaterial.Opaque
        }
    }

    // Iso-potential gradient shells (optional; enabled via Display panel).
    // 3 inside shells (blue→teal) + 3 outside shells (yellow→red)
    // at force-contour distances from the wall boundary.
    ToggledModel {
        source: "#Cylinder"
        visible: content.ctl.wallPotShellsVisible
        instancing: content.inst.wallPotShellsInstancing
        materials: PrincipledMaterial {
            baseColor: "white"
            lighting: PrincipledMaterial.NoLighting
            alphaMode: PrincipledMaterial.Blend
        }
    }

    // Wall force vector field: arrows sampled on a grid around the boundary.
    // Shafts (cylinder) + tips (cone), colour/length = force magnitude.
    ToggledModel {
        source: "#Cylinder"
        visible: content.ctl.wallForceArrowsVisible
        instancing: content.inst.wallForceShaftsInstancing
        materials: PrincipledMaterial {
            baseColor: "white"
            lighting: PrincipledMaterial.NoLighting
            alphaMode: PrincipledMaterial.Blend
        }
    }
    ToggledModel {
        source: "#Cone"
        visible: content.ctl.wallForceArrowsVisible
        instancing: content.inst.wallForceTipsInstancing
        materials: PrincipledMaterial {
            baseColor: "white"
            lighting: PrincipledMaterial.NoLighting
            alphaMode: PrincipledMaterial.Blend
        }
    }

    // Non-covalent interaction overlay: dashed contact lines (hydrogen
    // bonds, halogen bonds, pi stacking, close contacts). Each dash is a
    // short #Cylinder built by SceneController::rebuildNci().
    // The per-instance colour carries RGB *and* alpha (strength-coded), so
    // the material is always Blend - Opaque would silently discard it.
    ToggledModel {
        source: "#Cylinder"
        visible: content.ctl.nciVisible
        instancing: content.inst.nciInstancing
        materials: PrincipledMaterial {
            baseColor: "white"
            lighting: PrincipledMaterial.NoLighting
            alphaMode: PrincipledMaterial.Blend
        }
    }

    // Grab force arrows (opt-in), from the grabbed atom along the force. Unlit
    // so the instance colour reads as a bright, flat overlay.
    ToggledModel {
        source: "#Cylinder"
        visible: content.ctl.forceVectorsVisible
        instancing: content.inst.arrowShaftInstancing
        materials: PrincipledMaterial {
            baseColor: "white"
            lighting: PrincipledMaterial.NoLighting
        }
    }
    ToggledModel {
        source: "#Cone"
        visible: content.ctl.forceVectorsVisible
        instancing: content.inst.arrowTipInstancing
        materials: PrincipledMaterial {
            baseColor: "white"
            lighting: PrincipledMaterial.NoLighting
        }
    }

    // Claude Generated 2026 - Unit cell of a loaded cif: the parallelepiped of the lattice vectors,
    // a/b/c edges red/green/blue, in the molecule's own frame so it turns with the crystal. Unlit, opaque.
    ToggledModel {
        source: "#Cylinder"
        visible: content.ctl.cellVisible
        instancing: content.inst.cellInstancing
        materials: PrincipledMaterial {
            baseColor: "white"
            lighting: PrincipledMaterial.NoLighting
        }
    }

    // Measurement lines (distance/angle/dihedral), unlit cyan.
    ToggledModel {
        source: "#Cylinder"
        visible: content.ctl.measurementActive
        instancing: content.inst.measureLineInstancing
        materials: PrincipledMaterial {
            baseColor: "white"
            lighting: PrincipledMaterial.NoLighting
        }
    }
}
