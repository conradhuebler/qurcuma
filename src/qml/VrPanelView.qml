// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Headset panel "View" (Claude Generated 2026): molecule size, reset of its orientation, and
// "Recenter", which lays the panels and the molecule out again around the current head pose, and the
// input mode, controllers or tracked hands (Claude Generated 2026; the desktop View ▸ VR Input menu
// follows it). Otherwise pure view state; nothing else here reaches the desktop application.
import QtQuick
import "VrStyle.js" as Style

VrPanel {
    id: panel

    required property real moleculeSize
    required property bool handInput
    signal sizeDown()
    signal sizeUp()
    signal resetRotation()
    signal recenter()
    /// "Controllers" (false) or "Hands" (true) was pressed.
    signal inputChosen(bool hands)

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

    // Each mode clicks with its own gesture, so either button is reachable from the other mode.
    Row {
        id: inputRow
        spacing: Style.spacing
        readonly property real buttonWidth: (parent.width - spacing) / 2
        VrButton {
            width: inputRow.buttonWidth
            text: "Controllers"
            tone: "ghost"
            highlighted: !panel.handInput
            onClicked: panel.inputChosen(false)
        }
        VrButton {
            width: inputRow.buttonWidth
            text: "Hands"
            tone: "ghost"
            highlighted: panel.handInput
            onClicked: panel.inputChosen(true)
        }
    }

    Text {
        width: parent.width
        text: panel.handInput ? "Pinch on or near the molecule to carry and turn it (in a simulation: pull an atom, both hands at once); hold with both hands to resize."
                              : "Trigger or grip (either hand) turns the molecule."
        color: Style.textDim
        font.pixelSize: Style.fontSmall
        wrapMode: Text.Wrap
    }
}
