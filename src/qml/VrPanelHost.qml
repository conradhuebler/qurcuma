// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Places one 2D panel in the headset scene (Claude Generated 2026). The panel's centre sits on a
// sphere around the head: yaw and pitch in degrees (yaw > 0 = to the left, pitch > 0 = up), distance
// in cm. The two nested rotations turn the panel's face exactly toward the head, so every panel is
// seen head-on (no grazing angles) whatever its height.
//
// The surface is an XrItem, which renders in the XrView and in a plain View3D alike, so the offline
// preview shows exactly what the headset shows. XrItem puts the content's top-left corner at its
// origin (+x right, +y up); the offset below centres it. Panel pixels are millimetres
// (Style.pixelsPerCm).
//
// Headset input does not use XrVirtualMouse: Qt 6.11 ray-picks an XrItem against its infinite plane
// and stops at the first plane that has no item under the hit, so a nearer panel (the menu bar, or
// a hidden one) swallowed clicks meant for the panel behind it. rayHit() below tests the real
// rectangle and hands the button under the ray to the controller (viewerxr.qml Hand); touchHit()
// does the same for a tracked hand's pinch point (VrHandInput.qml).
import QtQuick
import QtQuick3D
import QtQuick3D.Xr
import "VrStyle.js" as Style

Node {
    id: host

    property real yaw: 0
    property real pitch: 0
    property real distance: 60
    property Item panel: null

    eulerRotation.y: yaw

    Node {
        eulerRotation.x: host.pitch

        XrItem {
            id: surface
            visible: host.visible
            width: host.panel ? host.panel.width / Style.pixelsPerCm : 1
            height: host.panel ? host.panel.height / Style.pixelsPerCm : 1
            position: Qt.vector3d(-width / 2, height / 2, -host.distance)
            pixelsPerUnit: Style.pixelsPerCm
            manualPixelsPerUnit: true
            color: "transparent"   // default is white, which would show around the rounded corners
            contentItem: host.panel
        }
    }

    /// Where a scene-space ray (unit direction) meets this panel: { distance (cm), button (the
    /// VrButton under the hit, null if none) }, or null if the ray misses it or the panel is hidden.
    /// The hands use it to tell "pointing at a panel" (trigger = click) from "pointing at the
    /// molecule" (trigger = rotate/grab), and to drive the button. Claude Generated.
    function rayHit(origin, direction) {
        if (!host.visible || !host.panel)
            return null
        // XrItem space: content in x [0, width], y [-height, 0] on the plane z = 0 (no scale on the
        // way up, so local units are scene centimetres)
        var o = surface.mapPositionFromScene(origin)
        var d = surface.mapDirectionFromScene(direction)
        if (Math.abs(d.z) < 1e-6)
            return null
        var t = -o.z / d.z
        if (t <= 0)
            return null
        var x = o.x + t * d.x
        var y = o.y + t * d.y
        if (x < 0 || x > surface.width || y > 0 || y < -surface.height)
            return null
        return { distance: t, button: buttonAt(x * Style.pixelsPerCm, -y * Style.pixelsPerCm) }
    }

    /// Direct touch (hand input): whether a scene point is at this panel, within Style.touchFront in
    /// front of it to Style.touchBehind behind it: { depth (cm, > 0 = in front), button (VrButton at
    /// that spot or null) }, or null. Claude Generated 2026.
    function touchHit(point) {
        if (!host.visible || !host.panel)
            return null
        // XrItem space as in rayHit; the head is on the +z side
        var p = surface.mapPositionFromScene(point)
        if (p.z > Style.touchFront || p.z < -Style.touchBehind)
            return null
        if (p.x < 0 || p.x > surface.width || p.y > 0 || p.y < -surface.height)
            return null
        return { depth: p.z, button: buttonAt(p.x * Style.pixelsPerCm, -p.y * Style.pixelsPerCm) }
    }

    /// Topmost visible VrButton at a point of the panel (panel pixels), null if none.
    function buttonAt(px, py) {
        var item = host.panel
        while (item) {
            var child = item.childAt(px, py)
            if (!child)
                return null
            var p = child.mapFromItem(item, px, py)
            px = p.x
            py = p.y
            if (child.vrRayTarget === true)
                return child
            item = child
        }
        return null
    }
}
