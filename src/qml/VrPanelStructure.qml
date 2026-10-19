// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Headset panel "Structure" (Claude Generated 2026): trajectory transport, the selected atom and the
// way into the file browser. Mirrors the desktop viewer bar's frame navigation.
//   session: VrSession (or the preview's MockVr): frame, frameCount, playing, selectionText,
//            structureFile, structureTitle, command()
import QtQuick
import "VrStyle.js" as Style

VrPanel {
    id: panel

    required property var session
    /// The file browser is open (the button then closes it).
    property bool filesOpen: false
    signal filesToggled()

    readonly property bool hasTrajectory: session.frameCount > 1

    title: "Structure"
    width: 320

    headerItems: Text {
        text: panel.session.frameCount < 1 ? "nothing loaded"
            : panel.hasTrajectory ? "Frame " + (panel.session.frame + 1) + " / " + panel.session.frameCount
            : "single structure"
        color: Style.textDim
        font.pixelSize: Style.fontBody
    }

    // Claude Generated 2026 - which structure this is: the molecule's name from the file, else the
    // file name; the file name below it when both exist.
    Column {
        width: parent.width
        readonly property string fileName: panel.session.structureFile || ""
        readonly property string moleculeName: panel.session.structureTitle || ""
        Text {
            width: parent.width
            text: parent.moleculeName !== "" ? parent.moleculeName
                : parent.fileName !== "" ? parent.fileName
                : panel.session.frameCount > 0 ? "unsaved structure" : "no structure"
            color: parent.moleculeName !== "" || parent.fileName !== "" ? Style.title : Style.textDim
            font.pixelSize: Style.fontButton
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            visible: parent.moleculeName !== "" && parent.fileName !== ""
            text: parent.fileName
            color: Style.textDim
            font.pixelSize: Style.fontSmall
            elide: Text.ElideMiddle
        }
    }

    Row {
        id: transport
        spacing: Style.spacing
        readonly property real buttonWidth: (parent.width - 2 * spacing) / 3
        VrButton {
            width: transport.buttonWidth
            text: "◀  Prev"
            enabled: panel.hasTrajectory
            onClicked: panel.session.command("previousFrame")
        }
        VrButton {
            width: transport.buttonWidth
            text: panel.session.playing ? "Pause" : "Play"
            enabled: panel.hasTrajectory
            onClicked: panel.session.command("playPause")
        }
        VrButton {
            width: transport.buttonWidth
            text: "Next  ▶"
            enabled: panel.hasTrajectory
            onClicked: panel.session.command("nextFrame")
        }
    }

    Item {
        width: parent.width
        height: 30
        Text {
            id: selectedLabel
            anchors.verticalCenter: parent.verticalCenter
            text: "Selected"
            color: Style.textDim
            font.pixelSize: Style.fontBody
        }
        Text {
            anchors.left: selectedLabel.right
            anchors.leftMargin: Style.spacing
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignRight
            text: panel.session.selectionText !== "" ? panel.session.selectionText : "none (A / X on an atom)"
            color: panel.session.selectionText !== "" ? Style.text : Style.textDim
            font.pixelSize: Style.fontBody
            elide: Text.ElideRight
        }
    }

    VrButton {
        width: parent.width
        text: panel.filesOpen ? "Close file browser" : "Open structure…"
        tone: panel.filesOpen ? "normal" : "primary"
        onClicked: panel.filesToggled()
    }
}
