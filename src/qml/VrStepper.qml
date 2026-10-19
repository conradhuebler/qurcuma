// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Labelled value with − / + buttons (Claude Generated 2026), used for temperature, grab strength and
// molecule size, so every adjustable number in the headset looks and works the same.
import QtQuick
import "VrStyle.js" as Style

Item {
    id: stepper

    property string label
    property string value
    signal decrease()
    signal increase()

    width: parent ? parent.width : 272
    height: Style.buttonHeight

    Text {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - plus.width - minus.width - valueText.width - 3 * Style.spacing
        text: stepper.label
        color: Style.textDim
        font.pixelSize: Style.fontBody
        elide: Text.ElideRight
    }
    VrButton {
        id: minus
        anchors.right: valueText.left
        anchors.rightMargin: Style.spacing
        width: 52
        text: "−"
        font.pixelSize: 18
        onClicked: stepper.decrease()
    }
    Text {
        id: valueText
        anchors.right: plus.left
        anchors.rightMargin: Style.spacing
        anchors.verticalCenter: parent.verticalCenter
        width: 70
        horizontalAlignment: Text.AlignHCenter
        text: stepper.value
        color: Style.text
        font.pixelSize: Style.fontBody + 1
        font.weight: Font.DemiBold
    }
    VrButton {
        id: plus
        anchors.right: parent.right
        width: 52
        text: "+"
        font.pixelSize: 18
        onClicked: stepper.increase()
    }
}
