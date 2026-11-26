// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Headset panel "Tutorial" (Claude Generated 2026): a short guided tour of the basic controls, one step
// at a time, for controllers or tracked hands (handInput). Started from "Tutorial" in the menu bar,
// the Help panel or the desktop (View ▸ Start VR Tutorial). UX test 2026-10: "too complicated without
// an introduction", 2 of 6 asked for a tutorial.
//
// A step with an action finishes when the user does it: VrScene.reportAction(name) calls actionDone()
// (names: rotate, carry, zoom, select, walk, turn, panelsShown, panelMove; sent by viewerxr.qml and
// VrHandInput.qml). The step then shows a tick and the next one follows after
// Style.tutorialAdvanceMilliseconds. "Skip" passes a step the user cannot or will not do; a step
// without an action (welcome, end) goes on with "Next", which is itself the first exercise: point
// and click. Keep the texts in sync with viewerxr.qml, VrHandInput.qml and VrPanelHelp.qml.
pragma ComponentBehavior: Bound
import QtQuick
import "VrStyle.js" as Style

VrPanel {
    id: panel

    required property bool handInput
    /// A simulation runs: the molecule cannot be carried then (atoms are pulled instead).
    property bool simulationActive: false
    signal closeRequested()
    signal helpRequested()

    readonly property var controllerSteps: [
        { title: "Welcome", action: "",
          text: "This short tour shows the basic controls. Both controllers work the same – use either hand.\n\n"
              + "Try it right away: point the ray at \"Next\" below and pull the trigger (index finger)." },
        { title: "Turn the molecule", action: "rotate",
          text: "Hold the grip button (middle finger) and turn your hand. The molecule turns along." },
        { title: "Move the molecule", action: "carry",
          text: "Point at the molecule, hold the trigger and move the controller. Let go to drop it." },
        { title: "Zoom", action: "zoom",
          text: "Push the right stick forward to make the molecule larger, pull it back to make it smaller.\n\n"
              + "Or hold both grips and pull your hands apart or together." },
        { title: "Select an atom", action: "select",
          text: "Point at an atom – a bright shell shows it – and press A or X. Selected atoms pulse." },
        { title: "Walk", action: "walk",
          text: "Tilt the left stick to move where you look. A slight tilt is slow.\n\n"
              + "Too fast or too slow? View ▸ Speed changes it." },
        { title: "Turn yourself", action: "turn",
          text: "Push the right stick left or right to turn by 45°." },
        { title: "Panels", action: "panelsShown",
          text: "Press B or Y to hide the panels, then once more to bring them back." },
        { title: "Move a panel", action: "panelMove",
          text: "Point at the bar below a panel, hold the trigger and point elsewhere. The panel follows." },
        { title: "Done!", action: "",
          text: "Lost the molecule? View ▸ Recenter puts everything in front of you again.\n\n"
              + "\"Help\" in the menu bar lists all controls." }
    ]
    readonly property var handSteps: [
        { title: "Welcome", action: "",
          text: "This short tour shows the hand controls. Both hands work the same.\n\n"
              + "Try it right away: touch \"Next\" below and pinch (thumb and index finger together)." },
        { title: "Hold the molecule", action: "carry",
          text: "Reach for the molecule, pinch on or near it and hold. It moves and turns with your hand." },
        { title: "Resize", action: "zoom",
          text: "Pinch the molecule with both hands, then pull them apart or together." },
        { title: "Select an atom", action: "select",
          text: "Touch an atom – a bright shell shows it – and pinch quickly. Selected atoms pulse." },
        { title: "Move a panel", action: "panelMove",
          text: "Pinch the bar below a panel and move your hand. The panel follows." },
        { title: "Done!", action: "",
          text: "Lost the molecule? View ▸ Recenter puts everything in front of you again.\n\n"
              + "\"Help\" in the menu bar lists all controls. B or Y on a controller switches back to controllers." }
    ]
    readonly property var steps: handInput ? handSteps : controllerSteps

    property int step: 0
    /// The current step's action was done: tick shown, next step pending.
    property bool stepDone: false
    readonly property var current: steps[Math.min(step, steps.length - 1)]
    readonly property bool lastStep: step >= steps.length - 1
    readonly property bool cannotDoNow: current.action === "carry" && simulationActive

    /// Start the tour from the first step.
    function restart() {
        advanceTimer.stop()
        stepDone = false
        step = 0
    }
    function next() {
        advanceTimer.stop()
        stepDone = false
        if (lastStep)
            closeRequested()
        else
            step++
    }
    function back() {
        advanceTimer.stop()
        stepDone = false
        if (step > 0)
            step--
    }
    /// A gesture was done (VrScene.reportAction); finishes the step if it asks for it.
    function actionDone(name) {
        if (stepDone || current.action === "" || current.action !== name)
            return
        stepDone = true
        advanceTimer.restart()
    }

    onHandInputChanged: restart()   // other steps for the other input mode

    Timer {
        id: advanceTimer
        interval: Style.tutorialAdvanceMilliseconds
        onTriggered: panel.next()
    }

    title: "Tutorial"
    width: 460

    headerItems: [
        Text {
            height: 40
            verticalAlignment: Text.AlignVCenter
            text: (panel.step + 1) + " / " + panel.steps.length
            color: Style.textDim
            font.pixelSize: Style.fontBody
        },
        VrButton {
            height: 40
            text: "Close"
            onClicked: panel.closeRequested()
        }
    ]

    // progress: one dot per step, done ones filled
    Row {
        spacing: 6
        Repeater {
            model: panel.steps.length
            delegate: Rectangle {
                required property int index
                width: 22
                height: 8
                radius: 4
                color: index < panel.step || (index === panel.step && panel.stepDone) ? Style.tutorialDone
                     : index === panel.step ? Style.hoverRing : Style.button
            }
        }
    }

    Text {
        width: parent.width
        text: panel.current.title
        color: Style.title
        font.pixelSize: Style.fontTitle
        font.weight: Font.DemiBold
    }

    Text {
        width: parent.width
        text: panel.current.text
        color: Style.text
        font.pixelSize: Style.fontBody + 1
        lineHeight: 1.15
        wrapMode: Text.Wrap
    }

    // what the step waits for: the tick once done
    Rectangle {
        visible: panel.current.action !== ""
        width: parent.width
        height: 40
        radius: Style.buttonRadius
        color: panel.stepDone ? Style.tutorialDone : "transparent"
        border.width: panel.stepDone ? 0 : 2
        border.color: panel.cannotDoNow ? Style.paused : Style.buttonBorder
        Text {
            anchors.centerIn: parent
            width: parent.width - 2 * Style.spacing
            horizontalAlignment: Text.AlignHCenter
            text: panel.stepDone ? "✓  Well done!"
                : panel.cannotDoNow ? "Not while a simulation runs – Skip"
                : "Your turn – try it now"
            color: panel.stepDone ? "#ffffff" : panel.cannotDoNow ? Style.paused : Style.textDim
            font.pixelSize: Style.fontBody
            font.weight: panel.stepDone ? Font.DemiBold : Font.Normal
            elide: Text.ElideRight
        }
    }

    Row {
        id: navRow
        spacing: Style.spacing
        readonly property real buttonWidth: (parent.width - 2 * spacing) / 3
        VrButton {
            width: navRow.buttonWidth
            text: "◀  Back"
            enabled: panel.step > 0
            onClicked: panel.back()
        }
        VrButton {
            width: navRow.buttonWidth
            text: "Help"
            onClicked: panel.helpRequested()
        }
        VrButton {
            width: navRow.buttonWidth
            text: panel.lastStep ? "Finish" : panel.current.action === "" || panel.stepDone ? "Next  ▶" : "Skip  ▶"
            tone: panel.current.action === "" || panel.stepDone ? "primary" : "normal"
            onClicked: panel.next()
        }
    }
}
