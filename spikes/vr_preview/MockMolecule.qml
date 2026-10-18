// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Stand-in for the desktop SceneController in the offline preview (Claude Generated 2026), split the
// way VrScene reads it: the mock itself carries what VrScene/MoleculeModels read from the controller
// (flags, sceneExtent, sceneCenter, atomPosition, atomIndexForInstance), and `tables` stands in for
// the XrSceneBridge (the instance tables under SceneController's property names). Filled with an
// idealised n-decane (C-C 1.54 Angstrom, zig-zag in the xy plane, CH 1.09 Angstrom), ball-and-stick.
// showOverlays adds what the desktop draws for a three-atom measurement: the picked carbons in the
// selection colour and cyan measurement lines between them.
import QtQuick
import QtQuick3D

Node {
    id: molecule

    property bool showOverlays: false

    // ---- SceneController stand-in --------------------------------------------------------------
    property real sceneExtent: 1
    property vector3d sceneCenter: Qt.vector3d(0, 0, 0)
    property color backgroundColor: "#1c2230"
    property bool atomsVisible: true
    property bool bondsVisible: true
    property bool blendEnabled: false
    property bool overlayVisible: false
    property bool wallVisible: false
    property real wallOpacity: 0.6
    property bool wallPotShellsVisible: false
    property bool wallForceArrowsVisible: false
    property bool nciVisible: false
    property bool forceVectorsVisible: false
    property bool measurementActive: showOverlays
    property var positions: []

    function atomPosition(index) {
        return index >= 0 && index < positions.length ? positions[index] : Qt.vector3d(0, 0, 0)
    }
    // no hidden atoms in the mock: instance row = atom index
    function atomIndexForInstance(instance) {
        return instance >= 0 && instance < positions.length ? instance : -1
    }

    // ---- XrSceneBridge stand-in ----------------------------------------------------------------
    property QtObject tables: QtObject {
        property var atomInstancing: atomList
        property var bondInstancing: bondList
        property var measureLineInstancing: measureList
        property var arrowShaftInstancing: null
        property var arrowTipInstancing: null
        property var overlayAtomInstancing: null
        property var overlayBondInstancing: null
        property var wallInstancing: null
        property var wallPotShellsInstancing: null
        property var wallForceShaftsInstancing: null
        property var wallForceTipsInstancing: null
        property var nciInstancing: null
    }

    InstanceList { id: atomList }
    InstanceList { id: bondList }
    InstanceList { id: measureList }

    property Component entry: Component { InstanceListEntry {} }

    // #Cylinder instance from pa to pb (radius 50, height 100, axis +Y), like SceneController's segments.
    function segment(pa, pb, radius, color) {
        var d = pb.minus(pa)
        var len = d.length()
        var dir = d.normalized()
        var axis = Qt.vector3d(0, 1, 0).crossProduct(dir)
        var angle = Math.acos(Math.max(-1, Math.min(1, dir.y)))
        var q = axis.length() < 1e-6 ? Qt.quaternion(1, 0, 0, 0)
              : Qt.quaternion(Math.cos(angle / 2), axis.normalized().x * Math.sin(angle / 2),
                              axis.normalized().y * Math.sin(angle / 2), axis.normalized().z * Math.sin(angle / 2))
        return entry.createObject(molecule, {
            position: pa.plus(pb).times(0.5),
            scale: Qt.vector3d(radius / 50, len / 100, radius / 50),
            rotation: q, color: color })
    }

    Component.onCompleted: {
        var atoms = []   // {p: vector3d, r: draw radius, c: colour}
        var bonds = []   // [i, j]
        var carbons = 10
        var lastCarbon = -1
        var carbonIndex = []
        for (var i = 0; i < carbons; ++i) {
            var up = i % 2 === 0 ? 1 : -1
            var c = Qt.vector3d(i * 1.258, up * 0.445, 0)
            atoms.push({ p: c, r: 0.36, c: "#909090" })
            var ci = atoms.length - 1
            carbonIndex.push(ci)
            if (i > 0)
                bonds.push([ci, lastCarbon])
            // two hydrogens out of the chain plane, pointing away from the chain
            for (var s = -1; s <= 1; s += 2) {
                atoms.push({ p: c.plus(Qt.vector3d(0, up * 0.63, s * 0.89)), r: 0.24, c: "#f0f0f0" })
                bonds.push([ci, atoms.length - 1])
            }
            if (i === 0 || i === carbons - 1) {   // methyl end: third hydrogen along the chain
                atoms.push({ p: c.plus(Qt.vector3d(i === 0 ? -1.03 : 1.03, up * 0.36, 0)), r: 0.24, c: "#f0f0f0" })
                bonds.push([ci, atoms.length - 1])
            }
            lastCarbon = ci
        }

        // a three-atom measurement on carbons 2-5-7 (not bonded, so the lines are not hidden inside
        // bonds): selection colour + lines (desktop look)
        var picked = showOverlays ? [carbonIndex[2], carbonIndex[5], carbonIndex[7]] : []
        for (var k = 0; k < picked.length; ++k)
            atoms[picked[k]].c = "#ff00ff"

        var lo = Qt.vector3d(1e9, 1e9, 1e9), hi = Qt.vector3d(-1e9, -1e9, -1e9)
        var list = [], pos = []
        for (var a = 0; a < atoms.length; ++a) {
            var p = atoms[a].p
            lo = Qt.vector3d(Math.min(lo.x, p.x), Math.min(lo.y, p.y), Math.min(lo.z, p.z))
            hi = Qt.vector3d(Math.max(hi.x, p.x), Math.max(hi.y, p.y), Math.max(hi.z, p.z))
            var s50 = atoms[a].r / 50   // #Sphere has radius 50
            list.push(entry.createObject(molecule, { position: p, scale: Qt.vector3d(s50, s50, s50), color: atoms[a].c }))
            pos.push(p)
        }
        atomList.instances = list
        positions = pos
        sceneCenter = lo.plus(hi).times(0.5)
        sceneExtent = Math.max(hi.x - lo.x, hi.y - lo.y, hi.z - lo.z) + 1.0

        var blist = []
        for (var b = 0; b < bonds.length; ++b)
            blist.push(segment(atoms[bonds[b][0]].p, atoms[bonds[b][1]].p, 0.12, "#b0b0b0"))
        bondList.instances = blist

        var mlist = []
        for (var m = 0; m + 1 < picked.length; ++m)
            mlist.push(segment(atoms[picked[m]].p, atoms[picked[m + 1]].p, 0.06, "#5adcff"))
        measureList.instances = mlist
    }
}
