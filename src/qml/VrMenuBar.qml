// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// See-through menu bar of the headset scene (Claude Generated 2026): one toggle per panel plus Help.
// It stays visible when the panels are hidden, so the way back is always in sight. A lit button means
// the panel is shown. B / Y on either controller hides or shows all panels at once (viewerxr.qml).
//   vrScene: VrScene (structureVisible, simulationVisible, viewVisible, helpVisible)
import QtQuick
import "VrStyle.js" as Style

Rectangle {
    id: bar

    required property var vrScene

    readonly property real buttonWidth: 88

    width: row.implicitWidth + 2 * Style.spacing
    height: Style.buttonHeight + 2 * Style.spacing
    radius: Style.radius
    color: Style.barColor
    border.color: Style.barBorder
    border.width: 2

    Row {
        id: row
        x: Style.spacing
        y: Style.spacing
        spacing: Style.spacing

        VrButton {
            width: bar.buttonWidth
            text: "Structure"
            tone: "ghost"
            highlighted: bar.vrScene.structureVisible
            onClicked: bar.vrScene.structureVisible = !bar.vrScene.structureVisible
        }
        VrButton {
            width: bar.buttonWidth
            text: "Simulation"
            tone: "ghost"
            highlighted: bar.vrScene.simulationVisible
            onClicked: bar.vrScene.simulationVisible = !bar.vrScene.simulationVisible
        }
        VrButton {
            width: bar.buttonWidth
            text: "View"
            tone: "ghost"
            highlighted: bar.vrScene.viewVisible
            onClicked: bar.vrScene.viewVisible = !bar.vrScene.viewVisible
        }
        Rectangle {
            width: 2
            height: Style.buttonHeight
            color: Style.barBorder
        }
        VrButton {
            width: bar.buttonWidth
            text: "?  Help"
            tone: "ghost"
            highlighted: bar.vrScene.helpVisible
            onClicked: bar.vrScene.helpVisible = !bar.vrScene.helpVisible
        }
    }
}
