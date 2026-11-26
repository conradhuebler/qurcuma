// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Headset panel "How to use" (Claude Generated 2026): a short cheat sheet for first-time VR users, for
// controllers or tracked hands (handInput). Opens at session start and from "Help" in the menu bar;
// Close or Help hides it again. "Start tutorial" opens the guided tour (VrPanelTutorial.qml) in its
// place. Keep it in sync with viewerxr.qml (controllers), VrHandInput.qml and VrPanelTutorial.qml.
pragma ComponentBehavior: Bound
import QtQuick
import "VrStyle.js" as Style

VrPanel {
    id: panel

    required property bool handInput
    signal closeRequested()
    signal tutorialRequested()

    readonly property var actions: handInput ? handActions : controllerActions
    // One line per action (UX test 2026-10: short, readable; the tutorial explains in more detail).
    readonly property var handActions: [
        { keys: "Pinch", text: "on a button presses it" },
        { keys: "Pinch molecule", text: "hold: it moves and turns with your hand" },
        { keys: "Both hands", text: "pinch it, pull apart / together: resize" },
        { keys: "Pinch atom (MD)", text: "while a simulation runs: pull it" },
        { keys: "Quick pinch", text: "on an atom selects it" },
        { keys: "Pinch handle", text: "the bar below a panel: move the panel" },
        { keys: "B / Y", text: "on a controller: back to controllers" }
    ]
    readonly property var controllerActions: [
        { keys: "Point + Trigger", text: "on a button presses it" },
        { keys: "Trigger on molecule", text: "hold: move it (no simulation)" },
        { keys: "Hold Grip", text: "turn your hand: the molecule turns" },
        { keys: "Right stick ↕", text: "molecule larger / smaller" },
        { keys: "Both Grips", text: "pull apart / together: resize" },
        { keys: "A / X", text: "selects the atom under the ray" },
        { keys: "Trigger on atom", text: "while a simulation runs: pull it" },
        { keys: "Trigger on handle", text: "the bar below a panel: move it" },
        { keys: "B / Y", text: "hides / shows all panels" },
        { keys: "Left stick", text: "walk where you look (speed: View)" },
        { keys: "Right stick ↔", text: "turn yourself by 45°" }
    ]

    title: "How to use"
    width: 500

    // Compact (Claude Generated 2026): the panel stands beside the molecule above the menu bar, so its
    // height decides how far up the user looks. Tutorial and Close sit in the title row.
    headerItems: [
        VrButton {
            height: 40
            text: "▶ Tutorial"
            tone: "primary"
            onClicked: panel.tutorialRequested()
        },
        VrButton {
            height: 40
            text: "Close"
            onClicked: panel.closeRequested()
        }
    ]

    Text {
        width: parent.width
        text: panel.handInput ? "Both hands work the same. The dot between thumb and index finger shows where a pinch lands."
                              : "Both controllers work the same – use either hand. Only the sticks differ."
        color: Style.textDim
        font.pixelSize: Style.fontSmall
        wrapMode: Text.Wrap
    }

    Column {
        width: parent.width
        spacing: 3
        Repeater {
            model: panel.actions
            delegate: Item {
                id: actionRow
                required property var modelData
                width: panel.width - 2 * Style.padding
                height: Math.max(30, description.implicitHeight)

                Rectangle {
                    id: keyChip
                    width: 165
                    height: 28
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 6
                    color: Style.button
                    border.color: Style.buttonBorder
                    Text {
                        anchors.centerIn: parent
                        text: actionRow.modelData.keys
                        color: Style.title
                        font.pixelSize: Style.fontSmall + 1
                        font.weight: Font.DemiBold
                        width: parent.width - 8
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                }
                Text {
                    id: description
                    anchors.left: keyChip.right
                    anchors.leftMargin: Style.spacing
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: actionRow.modelData.text
                    color: Style.text
                    font.pixelSize: Style.fontBody
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
