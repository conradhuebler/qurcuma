// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Headset number pad (Claude Generated 2026): typing a whole number where the headset has no
// keyboard, e.g. the min / max of the thermostat slider. An edit field with a caret, digits, ⌫
// (delete the last digit) and C (clear), then Cancel or OK. Like a selected text field, the first
// digit replaces the value shown at opening. OK is offered only for a value within
// minimumValue..maximumValue; otherwise the field turns red and rangeHint says what fits.
import QtQuick
import "VrStyle.js" as Style

VrPanel {
    id: pad

    property string unit: ""
    property real initialValue: 0
    property real minimumValue: 0
    property real maximumValue: 1e9
    /// What a valid value is, shown when the entry is out of range ("above the minimum (1 K)")
    property string rangeHint: ""
    /// True while the pad is shown; each opening starts from initialValue
    property bool open: false

    signal accepted(real value)
    signal cancelled()

    /// The digits typed so far, and whether the next digit replaces them (just opened)
    property string entry: ""
    property bool replaceOnType: true
    readonly property real value: entry === "" ? NaN : Number(entry)
    readonly property bool valid: !isNaN(value) && value >= minimumValue && value <= maximumValue

    onOpenChanged: if (open) reset()
    onInitialValueChanged: if (open && replaceOnType) reset()
    function reset() {
        entry = initialValue.toFixed(0)
        replaceOnType = true
    }

    function type(digit) {
        if (replaceOnType)
            entry = ""
        replaceOnType = false
        if (entry.length >= Style.padMaxDigits)
            return
        entry = entry === "0" ? digit : entry + digit   // no leading zeros
    }

    width: 300

    headerItems: VrButton {
        height: 36
        text: "Cancel"
        onClicked: pad.cancelled()
    }

    // the edit field
    Rectangle {
        width: parent.width
        height: 52
        radius: Style.buttonRadius
        color: Style.field
        border.width: 2
        border.color: pad.valid ? Style.fieldBorder : Style.aborted

        Row {
            anchors.centerIn: parent
            spacing: 2
            Text {
                id: entryText
                // just opened: the value shown as if selected, as a desktop field after a click
                text: pad.entry
                color: pad.replaceOnType ? Style.textDim : Style.title
                font.pixelSize: 24
                font.weight: Font.DemiBold
            }
            Rectangle {
                id: caret
                width: 2
                height: 26
                anchors.verticalCenter: entryText.verticalCenter
                color: Style.hoverRing
                SequentialAnimation on opacity {
                    running: pad.open
                    loops: Animation.Infinite
                    NumberAnimation { to: 0; duration: 500 }
                    NumberAnimation { to: 1; duration: 500 }
                }
            }
            Text {
                anchors.baseline: entryText.baseline
                leftPadding: 6
                text: pad.unit
                color: Style.textDim
                font.pixelSize: Style.fontBody + 2
            }
        }
    }

    Text {
        width: parent.width
        text: pad.valid || pad.entry === "" ? "Type a value, then OK." : "Out of range: " + pad.rangeHint
        color: pad.valid || pad.entry === "" ? Style.textDim : Style.aborted
        font.pixelSize: Style.fontSmall
        wrapMode: Text.Wrap
    }

    Grid {
        id: keys
        columns: 3
        spacing: Style.spacing
        readonly property real keyWidth: (parent.width - 2 * spacing) / 3
        Repeater {
            model: ["7", "8", "9", "4", "5", "6", "1", "2", "3", "C", "0", "⌫"]
            delegate: VrButton {
                required property string modelData
                width: keys.keyWidth
                text: modelData
                font.pixelSize: 18
                onClicked: {
                    if (modelData === "C") {
                        pad.entry = ""
                        pad.replaceOnType = false
                    } else if (modelData === "⌫") {
                        pad.entry = pad.replaceOnType ? "" : pad.entry.slice(0, -1)
                        pad.replaceOnType = false
                    } else {
                        pad.type(modelData)
                    }
                }
            }
        }
    }

    VrButton {
        width: parent.width
        text: "OK"
        tone: "primary"
        enabled: pad.valid
        onClicked: pad.accepted(pad.value)
    }
}
