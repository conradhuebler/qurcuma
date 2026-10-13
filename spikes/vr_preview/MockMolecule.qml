// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Stand-in for the headset's SceneController in the offline preview (Claude Generated 2026): the
// properties VrScene reads (atomInstancing, bondInstancing, sceneExtent, sceneCenter, atomPosition),
// filled with an idealised n-decane (C-C 1.54 Angstrom, zig-zag in the xy plane, CH 1.09 Angstrom)
// drawn ball-and-stick.
import QtQuick
import QtQuick3D

Node {
    id: molecule

    property real sceneExtent: 1
    property vector3d sceneCenter: Qt.vector3d(0, 0, 0)
    property alias atomInstancing: atomList
    property alias bondInstancing: bondList
    property var positions: []

    function atomPosition(index) {
        return index >= 0 && index < positions.length ? positions[index] : Qt.vector3d(0, 0, 0)
    }

    InstanceList { id: atomList }
    InstanceList { id: bondList }

    property Component entry: Component { InstanceListEntry {} }

    Component.onCompleted: {
        var atoms = []   // {p: vector3d, r: draw radius, c: colour}
        var bonds = []   // [i, j]
        var carbons = 10
        var lastCarbon = -1
        for (var i = 0; i < carbons; ++i) {
            var up = i % 2 === 0 ? 1 : -1
            var c = Qt.vector3d(i * 1.258, up * 0.445, 0)
            atoms.push({ p: c, r: 0.36, c: "#909090" })
            var ci = atoms.length - 1
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
        for (var b = 0; b < bonds.length; ++b) {
            var pa = atoms[bonds[b][0]].p, pb = atoms[bonds[b][1]].p
            var d = pb.minus(pa)
            var len = d.length()
            var dir = d.normalized()
            // rotate the cylinder axis (+Y) onto the bond direction
            var axis = Qt.vector3d(0, 1, 0).crossProduct(dir)
            var angle = Math.acos(Math.max(-1, Math.min(1, dir.y)))
            var q = axis.length() < 1e-6 ? Qt.quaternion(1, 0, 0, 0)
                  : Qt.quaternion(Math.cos(angle / 2), axis.normalized().x * Math.sin(angle / 2),
                                  axis.normalized().y * Math.sin(angle / 2), axis.normalized().z * Math.sin(angle / 2))
            blist.push(entry.createObject(molecule, {
                position: pa.plus(pb).times(0.5),
                scale: Qt.vector3d(0.12 / 50, len / 100, 0.12 / 50),   // #Cylinder: radius 50, height 100
                rotation: q, color: "#b0b0b0" }))
        }
        bondList.instances = blist
    }
}
