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
}
