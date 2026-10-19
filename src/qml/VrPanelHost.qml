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
//
// Moving (Claude Generated 2026): a handle below the panel (above for the menu bar), like a window's
// title bar. Holding it
// (trigger or pinch) calls moveCentreTo(), which turns the target point into yaw, pitch and distance
// (placeYaw/placePitch/placeDistance), so a moved panel still faces the head. The layout's own
// yaw/pitch/distance come back with resetPlacement() ("Recenter").
//
// Child panels (Claude Generated 2026): with anchorHost set, yaw/pitch/distance (and a move by the
// handle) are offsets from that panel's place, so the child travels along when the anchor is moved.
import QtQuick
import QtQuick3D
import QtQuick3D.Xr
import "VrStyle.js" as Style

Node {
    id: host

    /// Place in the layout (VrScene)
    property real yaw: 0
    property real pitch: 0
    property real distance: 60
    property Item panel: null
    /// Panel this one belongs to (null = placed on its own); its place is then relative to the anchor's.
    property Node anchorHost: null

    /// Place the user moved the panel to (moved = true), else the layout's
    property bool moved: false
    property real movedYaw: 0
    property real movedPitch: 0
    property real movedDistance: 60
    readonly property real placeYaw: (anchorHost ? anchorHost.placeYaw : 0) + (moved ? movedYaw : yaw)
    readonly property real placePitch: (anchorHost ? anchorHost.placePitch : 0) + (moved ? movedPitch : pitch)
    readonly property real placeDistance: (anchorHost ? anchorHost.placeDistance : 0) + (moved ? movedDistance : distance)

    /// Handle state, driven by the hands and controllers: rays/pinch points on it, and held
    property int handleHoverCount: 0
    property bool handleHeld: false
    /// Handle above the panel instead of below (the menu bar: below it sit the console panels)
    property bool handleAbove: false
    /// Top edge of the handle in surface space (cm; the panel spans y 0 .. -height)
    readonly property real handleTop: handleAbove ? Style.handleGap + Style.handleHeight
                                                  : -surface.height - Style.handleGap

    eulerRotation.y: placeYaw

    Node {
        eulerRotation.x: host.placePitch

        XrItem {
            id: surface
            visible: host.visible
            width: host.panel ? host.panel.width / Style.pixelsPerCm : 1
            height: host.panel ? host.panel.height / Style.pixelsPerCm : 1
            position: Qt.vector3d(-width / 2, height / 2, -host.placeDistance)
            pixelsPerUnit: Style.pixelsPerCm
            manualPixelsPerUnit: true
            color: "transparent"   // default is white, which would show around the rounded corners
            contentItem: host.panel
        }

        // Handle: centred below (or above) the panel in the panel's plane. The item is the target area; the
        // pill inside it is what the user sees, lit while hovered, blue while held.
        XrItem {
            id: handleSurface
            visible: host.visible
            width: Style.handleWidth
            height: Style.handleHeight
            position: Qt.vector3d(-width / 2, surface.height / 2 + host.handleTop, -host.placeDistance)
            pixelsPerUnit: Style.pixelsPerCm
            manualPixelsPerUnit: true
            color: "transparent"
            contentItem: Item {
                width: Style.handleWidth * Style.pixelsPerCm
                height: Style.handleHeight * Style.pixelsPerCm
                Rectangle {
                    readonly property bool lit: host.handleHoverCount > 0 || host.handleHeld
                    anchors.centerIn: parent
                    width: parent.width * (lit ? 0.8 : 0.65)
                    height: lit ? 16 : 11
                    radius: height / 2
                    color: host.handleHeld ? Style.handleHeld : lit ? Style.handleHover : Style.handle
                    border.width: lit ? 2 : 0
                    border.color: Style.hoverRing
                }
            }
        }
    }

    // ---- hit tests ------------------------------------------------------------------------------
    // Both work in XrItem space of the surface: content in x [0, width], y [-height, 0] on the plane
    // z = 0, head on the +z side; no scale on the way up, so local units are scene centimetres. The
    // handle lies in the same plane, below. A hit is { host, handle (bool), button (the VrButton or
    // drag target there, null if none), x, y (panel pixels) } plus distance (ray) or depth (touch).

    /// Where a scene-space ray (unit direction) meets this panel or its handle; null if it misses
    /// them or the panel is hidden. The hands use it to tell "pointing at a panel" (trigger = click)
    /// from "pointing at the molecule" (trigger = rotate/grab), and to drive the button. Claude Generated.
    function rayHit(origin, direction) {
        if (!host.visible || !host.panel)
            return null
        var p = planePointOnRay(origin, direction)
        if (!p)
            return null
        var hit = hitAt(p.x, p.y)
        if (hit)
            hit.distance = p.t
        return hit
    }

    /// Direct touch (hand input): a scene point within Style.touchFront in front of the panel (or its
    /// handle) to Style.touchBehind behind it; depth > 0 = in front. Claude Generated 2026.
    function touchHit(point) {
        if (!host.visible || !host.panel)
            return null
        var p = surface.mapPositionFromScene(point)
        if (p.z > Style.touchFront || p.z < -Style.touchBehind)
            return null
        var hit = hitAt(p.x, p.y)
        if (hit)
            hit.depth = p.z
        return hit
    }

    function hitAt(x, y) {
        if (x >= 0 && x <= surface.width && y <= 0 && y >= -surface.height) {
            var px = x * Style.pixelsPerCm
            var py = -y * Style.pixelsPerCm
            return { host: host, handle: false, button: buttonAt(px, py), x: px, y: py }
        }
        var handleLeft = (surface.width - Style.handleWidth) / 2
        if (x >= handleLeft && x <= handleLeft + Style.handleWidth && y <= host.handleTop && y >= host.handleTop - Style.handleHeight)
            return { host: host, handle: true, button: null, x: 0, y: 0 }
        return null
    }

    /// Ray and panel plane, without the rectangle test: { x, y (cm, surface space), t (distance) }.
    function planePointOnRay(origin, direction) {
        var o = surface.mapPositionFromScene(origin)
        var d = surface.mapDirectionFromScene(direction)
        if (Math.abs(d.z) < 1e-6)
            return null
        var t = -o.z / d.z
        if (t <= 0)
            return null
        return { x: o.x + t * d.x, y: o.y + t * d.y, t: t }
    }

    /// Panel pixels where a ray meets the panel's plane, also outside the panel (a slider drag
    /// that left the track keeps going, like a mouse drag); null if the ray points away from it.
    function panelPointOnRay(origin, direction) {
        var p = planePointOnRay(origin, direction)
        return p ? Qt.point(p.x * Style.pixelsPerCm, -p.y * Style.pixelsPerCm) : null
    }

    /// Panel pixels of a scene point projected onto the panel's plane (hand drags).
    function panelPointAt(point) {
        var p = surface.mapPositionFromScene(point)
        return Qt.point(p.x * Style.pixelsPerCm, -p.y * Style.pixelsPerCm)
    }

    /// Topmost visible VrButton or drag target at a point of the panel (panel pixels), null if none.
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

    // ---- moving ---------------------------------------------------------------------------------
    /// Scene position of the panel's centre.
    function centreScenePosition() {
        return surface.mapPositionToScene(Qt.vector3d(surface.width / 2, -surface.height / 2, 0))
    }

    /// Put the panel's centre at a scene point: with the layout's head-centred rotations,
    /// p = R_y(yaw) R_x(pitch) (0, 0, -d) = (-d cos(pitch) sin(yaw), d sin(pitch), -d cos(pitch) cos(yaw)),
    /// so yaw = atan2(-p.x, -p.z), pitch = asin(p.y / d), in the frame the layout turns in (the parent).
    /// The distance stays within Style.panelMinDistance..panelMaxDistance.
    function moveCentreTo(scenePoint) {
        var p = host.parent.mapPositionFromScene(scenePoint)
        var d = p.length()
        if (d < 1e-3)
            return
        movedYaw = Math.atan2(-p.x, -p.z) * 180 / Math.PI - (anchorHost ? anchorHost.placeYaw : 0)
        movedPitch = Math.asin(Math.max(-1, Math.min(1, p.y / d))) * 180 / Math.PI
                   - (anchorHost ? anchorHost.placePitch : 0)
        movedDistance = Math.max(Style.panelMinDistance, Math.min(Style.panelMaxDistance, d))
                      - (anchorHost ? anchorHost.placeDistance : 0)
        moved = true
    }

    function resetPlacement() {
        moved = false
    }
}
