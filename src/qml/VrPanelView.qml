// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Headset panel "View" (Claude Generated 2026): molecule size, reset of its orientation, and
// "Recenter", which lays the panels and the molecule out again around the current head pose.
// Pure view state; nothing here reaches the desktop application.
import QtQuick
import "VrStyle.js" as Style

VrPanel {
    id: panel

    required property real moleculeSize
    signal sizeDown()
    signal sizeUp()
    signal resetRotation()
    signal recenter()

    title: "View"
    width: 260

    VrStepper {
        label: "Size"
        value: panel.moleculeSize.toFixed(0) + " cm"
        onDecrease: panel.sizeDown()
        onIncrease: panel.sizeUp()
    }

    Row {
        id: viewRow
        spacing: Style.spacing
        readonly property real buttonWidth: (parent.width - spacing) / 2
        VrButton {
            width: viewRow.buttonWidth
            text: "Reset"
            onClicked: panel.resetRotation()
        }
        VrButton {
            width: viewRow.buttonWidth
            text: "Recenter"
            onClicked: panel.recenter()
        }
    }

    Text {
        width: parent.width
        text: "Trigger or grip (either hand) turns the molecule."
        color: Style.textDim
        font.pixelSize: Style.fontSmall
        wrapMode: Text.Wrap
    }
}
