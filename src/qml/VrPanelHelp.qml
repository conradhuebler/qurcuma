// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Headset panel "How to use" (Claude Generated 2026): a short cheat sheet for first-time VR users, for
// controllers or tracked hands (handInput). Opens at session start and from "Help" in the menu bar;
// Close or Help hides it again. Keep it in sync with viewerxr.qml (controllers) and VrHandInput.qml.
pragma ComponentBehavior: Bound
import QtQuick
import "VrStyle.js" as Style

VrPanel {
    id: panel

    required property bool handInput
    signal closeRequested()

    readonly property var actions: handInput ? handActions : controllerActions
    readonly property var handActions: [
        { keys: "Pinch", text: "on a button presses it. The dot between thumb and index finger shows where the pinch lands; a pinch that touches nothing does nothing" },
        { keys: "Hold atom (MD)", text: "while a simulation runs pinch and hold an atom to pull it" },
        { keys: "Hold atom", text: "otherwise the molecule moves and turns with your hand" },
        { keys: "Both hands", text: "hold atoms with both, then pull apart or together: the molecule grows or shrinks" },
        { keys: "Fingertip on atom", text: "a bright shell shows the atom a pinch takes" },
        { keys: "Quick pinch", text: "on an atom selects it (it gets a pulsing shell)" },
        { keys: "Pinch handle", text: "the bar below a panel, then move your hand: the panel follows" },
        { keys: "B / Y", text: "on a controller switches back to controllers" }
    ]
    readonly property var controllerActions: [
        { keys: "Point + Trigger", text: "on a button presses it" },
        { keys: "Hold Trigger or Grip", text: "away from the panels and turn your hand: the molecule turns with it" },
        { keys: "Ray on atom", text: "a bright shell shows the atom the ray is on" },
        { keys: "A / X", text: "on an atom selects it (it gets a pulsing shell)" },
        { keys: "Trigger on handle", text: "the bar below a panel: hold and point elsewhere to move it" },
        { keys: "Hold Trigger", text: "on an atom while a simulation runs pulls the atom" },
        { keys: "B / Y", text: "hides or shows all panels" },
        { keys: "Left stick", text: "walks where you look" },
        { keys: "Right stick", text: "left / right turns you by 45°" }
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
        text: panel.handInput ? "No controllers needed – both hands work the same. Walk around in the room to move."
                              : "Both controllers work the same – use whichever hand you like. Only the sticks differ."
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
