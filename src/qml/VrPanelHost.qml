// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Places one 2D panel in the headset scene (Claude Generated 2026). The panel's centre sits on a
// sphere around the head: yaw and pitch in degrees (yaw > 0 = to the left, pitch > 0 = up), distance
// in cm. The two nested rotations turn the panel's face exactly toward the head, so every panel is
// seen head-on (no grazing angles) whatever its height.
//
// The surface is an XrItem, which renders and takes pointer input in the XrView (XrVirtualMouse)
// and in a plain View3D alike, so the offline preview shows exactly what the headset shows.
// XrItem puts the content's top-left corner at its origin (+x right, +y up); the offset below
// centres it. Panel pixels are millimetres (Style.pixelsPerCm).
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

    /// Distance (cm) along a scene-space ray (unit direction) to this panel, -1 if the ray misses it
    /// or the panel is hidden. The hands use it to tell "pointing at a panel" (trigger = click) from
    /// "pointing at the molecule" (trigger = rotate/grab). Claude Generated.
    function rayDistance(origin, direction) {
        if (!host.visible || !host.panel)
            return -1
        // XrItem space: content in x [0, width], y [-height, 0] on the plane z = 0 (no scale on the
        // way up, so local units are scene centimetres)
        var o = surface.mapPositionFromScene(origin)
        var d = surface.mapDirectionFromScene(direction)
        if (Math.abs(d.z) < 1e-6)
            return -1
        var t = -o.z / d.z
        if (t <= 0)
            return -1
        var x = o.x + t * d.x
        var y = o.y + t * d.y
        return (x >= 0 && x <= surface.width && y <= 0 && y >= -surface.height) ? t : -1
    }
}
