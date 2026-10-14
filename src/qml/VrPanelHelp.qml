// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Headset panel "How to use" (Claude Generated 2026): a short controller cheat sheet for first-time
// VR users. Opens at session start and from "Help" in the menu bar; Close or Help hides it again.
// Keep it in sync with the input handling in viewerxr.qml.
pragma ComponentBehavior: Bound
import QtQuick
import "VrStyle.js" as Style

VrPanel {
    id: panel

    signal closeRequested()

    readonly property var actions: [
        { keys: "Point + Trigger", text: "on a button presses it" },
        { keys: "Hold Trigger or Grip", text: "away from the panels and turn your hand: the molecule turns with it" },
        { keys: "A / X", text: "on an atom selects it (shown in Structure)" },
        { keys: "Hold Trigger", text: "on an atom while a simulation runs pulls the atom" },
        { keys: "B / Y", text: "hides or shows all panels" }
    ]

    title: "How to use"
    width: 420

    headerItems: VrButton {
        height: 36
        text: "Close"
        onClicked: panel.closeRequested()
    }

    Text {
        width: parent.width
        text: "Both controllers work the same – use whichever hand you like."
        color: Style.text
        font.pixelSize: Style.fontBody
        wrapMode: Text.Wrap
    }

    Repeater {
        model: panel.actions
        delegate: Item {
            id: actionRow
            required property var modelData
            width: panel.width - 2 * Style.padding
            height: Math.max(30, description.implicitHeight)

            Rectangle {
                id: keyChip
                width: 130
                height: 26
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

    Text {
        width: parent.width
        text: "Lost the molecule? View › Recenter puts everything in front of you again. "
              + "The bar below the molecule shows or hides each panel and this help."
        color: Style.textDim
        font.pixelSize: Style.fontSmall
        wrapMode: Text.Wrap
    }
}
