// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Headset button (Claude Generated 2026): a large ray target with a hover ring, so the user sees
// which button a controller ray is on before pulling the trigger.
import QtQuick
import QtQuick.Templates as T
import "VrStyle.js" as Style

T.Button {
    id: control

    /// "normal", "primary" (starts something), "danger" (stops something), "ghost" (see-through
    /// toggle of the menu bar; highlighted = the thing it toggles is shown) or "field" (a value that
    /// opens the number pad when pressed, drawn like an edit field)
    property string tone: "normal"
    /// Text alignment of the label; file list rows use Text.AlignLeft.
    property int alignment: Text.AlignHCenter
    /// Controller rays (viewerxr.qml Hand) drive the button directly, not through mouse events:
    /// rays on it (both hands count) and a trigger held on it. Claude Generated 2026.
    property int rayHoverCount: 0
    property bool rayDown: false
    /// Marks the target VrPanelHost.buttonAt looks for.
    readonly property bool vrRayTarget: true

    implicitWidth: Math.max(56, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Style.buttonHeight
    leftPadding: 6
    rightPadding: 6
    hoverEnabled: true
    font.pixelSize: Style.fontButton

    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? Style.textDisabled
             : control.tone === "ghost" && !control.highlighted ? Style.textDim : Style.text
        horizontalAlignment: control.alignment
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideMiddle
    }

    background: Rectangle {
        readonly property bool lit: (control.hovered || control.rayHoverCount > 0) && control.enabled
        radius: Style.buttonRadius
        color: !control.enabled ? Style.buttonDisabled
             : control.down || control.rayDown ? Style.buttonDown
             : control.tone === "primary" ? (lit ? Style.primaryHover : Style.primary)
             : control.tone === "danger" ? (lit ? Style.dangerHover : Style.danger)
             : control.tone === "ghost" ? (control.highlighted ? (lit ? Style.ghostOnHover : Style.ghostOn)
                                                               : (lit ? Style.ghostHover : Style.ghost))
             : control.tone === "field" ? (lit ? Style.fieldHover : Style.field)
             : (lit ? Style.buttonHover : Style.button)
        border.width: lit ? 3 : 1
        border.color: lit ? Style.hoverRing : control.tone === "ghost" ? Style.barBorder
                    : control.tone === "field" ? Style.fieldBorder : Style.buttonBorder
    }
}
