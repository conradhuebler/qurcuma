// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Headset button (Claude Generated 2026): a large ray target with a hover ring, so the user sees
// which button the left controller ray is on before pulling the trigger.
import QtQuick
import QtQuick.Templates as T
import "VrStyle.js" as Style

T.Button {
    id: control

    /// "normal", "primary" (starts something) or "danger" (stops something)
    property string tone: "normal"
    /// Text alignment of the label; file list rows use Text.AlignLeft.
    property int alignment: Text.AlignHCenter

    implicitWidth: Math.max(56, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Style.buttonHeight
    leftPadding: 6
    rightPadding: 6
    hoverEnabled: true
    font.pixelSize: Style.fontButton

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? Style.text : Style.textDisabled
        horizontalAlignment: control.alignment
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideMiddle
    }

    background: Rectangle {
        readonly property bool lit: control.hovered && control.enabled
        radius: Style.buttonRadius
        color: !control.enabled ? Style.buttonDisabled
             : control.down ? Style.buttonDown
             : control.tone === "primary" ? (lit ? Style.primaryHover : Style.primary)
             : control.tone === "danger" ? (lit ? Style.dangerHover : Style.danger)
             : (lit ? Style.buttonHover : Style.button)
        border.width: lit ? 3 : 1
        border.color: lit ? Style.hoverRing : Style.buttonBorder
    }
}
