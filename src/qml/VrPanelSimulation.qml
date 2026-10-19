// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Headset panel "Simulation" (Claude Generated 2026): the Simulation dock's start/pause/stop, the
// live thermostat target (VrTemperatureSlider, the dock's slider with its min/max) and the grab strength, with the run state as in the dock (Running, Paused,
// Finished, Aborted) and the last run's end line ("Finished: ..." / "Aborted: ...").
//   session: VrSession (or MockVr): simulationActive, simulationPaused, simulationMessage,
//            simulationMode, temperature, temperatureMin/Max, method, grabStrength, handInput, command()
import QtQuick
import "VrStyle.js" as Style

VrPanel {
    id: panel

    required property var session
    /// A min / max field of the thermostat slider was pressed ("min" / "max").
    signal temperatureEditRequested(string which)

    readonly property bool active: session.simulationActive
    readonly property string message: session.simulationMessage
    /// The Simulation dock's states: Running, Paused, and after a run Finished or Aborted.
    readonly property string runState: active ? (session.simulationPaused ? "Paused" : "Running")
                                     : message.indexOf("Aborted") === 0 ? "Aborted"
                                     : message !== "" ? "Finished" : "Idle"

    title: "Simulation"
    width: 470

    headerItems: Rectangle {
        width: stateText.implicitWidth + 20
        height: 26
        radius: 13
        color: panel.runState === "Running" ? Style.running
             : panel.runState === "Paused" ? Style.paused
             : panel.runState === "Aborted" ? Style.aborted : Style.idle
        Text {
            id: stateText
            anchors.centerIn: parent
            text: panel.runState
            color: "#ffffff"
            font.pixelSize: Style.fontSmall + 1
            font.weight: Font.DemiBold
        }
    }

    // Controls on the left, the thermostat slider (as in the Simulation dock) on the right.
    Row {
        width: parent.width
        spacing: Style.spacing

        Column {
            width: parent.width - thermostat.width - parent.spacing
            spacing: Style.spacing

            Row {
                id: runRow
                spacing: Style.spacing
                readonly property real buttonWidth: (parent.width - 3 * spacing) / 4
                VrButton {
                    width: runRow.buttonWidth
                    text: "MD"
                    tone: "primary"
                    selected: panel.active && panel.session.simulationMode === "md"
                    enabled: !panel.active
                    onClicked: panel.session.command("simStartMD")
                }
                VrButton {
                    width: runRow.buttonWidth
                    text: "Opt"
                    tone: "primary"
                    selected: panel.active && panel.session.simulationMode === "opt"
                    enabled: !panel.active
                    onClicked: panel.session.command("simStartOpt")
                }
                VrButton {
                    width: runRow.buttonWidth
                    text: panel.session.simulationPaused ? "Resume" : "Pause"
                    enabled: panel.active
                    onClicked: panel.session.command("simPause")
                }
                VrButton {
                    width: runRow.buttonWidth
                    text: "Stop"
                    tone: "danger"
                    enabled: panel.active
                    onClicked: panel.session.command("simStop")
                }
            }

            Row {
                id: methodRow
                spacing: Style.spacing
                readonly property var methods: [
                    { key: "gfnff", label: "GFN-FF" }, { key: "uff", label: "UFF" },
                    { key: "gfn2", label: "GFN2" }, { key: "gfn1", label: "GFN1" } ]
                readonly property real buttonWidth: (parent.width - 3 * spacing) / 4
                Repeater {
                    model: methodRow.methods
                    delegate: VrButton {
                        required property var modelData
                        width: methodRow.buttonWidth
                        text: modelData.label
                        tone: panel.session.method === modelData.key ? "primary" : "normal"
                        selected: panel.session.method === modelData.key
                        enabled: !panel.active   // the dock locks the method during a run
                        onClicked: panel.session.command("simMethod:" + modelData.key)
                    }
                }
            }

            VrStepper {
                label: "Grab strength"
                value: panel.session.grabStrength.toFixed(3)
                onDecrease: panel.session.command("grabDown")
                onIncrease: panel.session.command("grabUp")
            }

            Text {
                width: parent.width
                text: panel.active ? (panel.session.handInput ? "Pinch and hold an atom to pull it." : "Trigger on an atom pulls it.")
                                   : panel.message !== "" ? panel.message
                                   : "MD or Opt starts with the Simulation dock's settings."
                color: panel.runState === "Aborted" ? Style.aborted : Style.textDim
                font.pixelSize: Style.fontSmall
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }

        VrTemperatureSlider {
            id: thermostat
            session: panel.session
            onEditRequested: (which) => panel.temperatureEditRequested(which)
        }
    }
}
