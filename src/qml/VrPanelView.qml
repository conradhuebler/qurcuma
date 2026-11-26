// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Headset panel "View" (Claude Generated 2026): molecule size, reset of its orientation, and
// "Recenter", which lays the panels and the molecule out again around the current head pose, and the
// input mode, controllers or tracked hands (Claude Generated 2026; the desktop View ▸ VR Input menu
// follows it). Comfort (Claude Generated 2026, UX test 2026-10): walking speed and text size, both
// remembered by the desktop (Settings::vrWalkSpeedLevel / vrTextSizeLevel). Otherwise pure view state.
import QtQuick
import "VrStyle.js" as Style

VrPanel {
    id: panel

    required property real moleculeSize
    required property bool handInput
    /// Index into Style.walkSpeeds / Style.textScales
    required property int walkSpeedLevel
    required property int textSizeLevel
    signal walkSpeedChosen(int level)
    signal textSizeChosen(int level)
    signal sizeDown()
    signal sizeUp()
    signal resetRotation()
    signal recenter()
    /// "Controllers" (false) or "Hands" (true) was pressed.
    signal inputChosen(bool hands)

    title: "View"
    width: 330

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

    // Choices of three: walking speed (controllers' left stick) and text size of all panels.
    // Label on the left, the choices on the right, one row each (keeps the panel low).
    component Choice: Row {
        id: choice
        property string label
        property var names: []
        property int current: 0
        signal chosen(int index)
        width: parent ? parent.width : 268
        spacing: Style.spacing
        Text {
            id: choiceLabel
            width: 62
            height: 40
            verticalAlignment: Text.AlignVCenter
            text: choice.label
            color: Style.textDim
            font.pixelSize: Style.fontSmall
            wrapMode: Text.Wrap
        }
        Row {
            id: choiceRow
            spacing: 4
            readonly property real buttonWidth: (choice.width - choiceLabel.width - choice.spacing
                                                 - (choice.names.length - 1) * spacing) / Math.max(1, choice.names.length)
            Repeater {
                model: choice.names
                delegate: VrButton {
                    required property int index
                    required property string modelData
                    width: choiceRow.buttonWidth
                    height: 40
                    leftPadding: 2
                    rightPadding: 2
                    font.pixelSize: Style.fontSmall + 1
                    text: modelData
                    tone: "ghost"
                    highlighted: index === choice.current
                    onClicked: choice.chosen(index)
                }
            }
        }
    }

    Choice {
        visible: !panel.handInput   // hands walk in the room
        label: "Speed"
        names: Style.walkSpeedNames
        current: panel.walkSpeedLevel
        onChosen: (index) => panel.walkSpeedChosen(index)
    }
    Choice {
        label: "Text size"
        names: Style.textScaleNames
        current: panel.textSizeLevel
        onChosen: (index) => panel.textSizeChosen(index)
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
}
