// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Headset thermostat slider (Claude Generated 2026), the desktop TemperatureSlider
// (src/widgets/temperatureslider.*) for the headset: max on top, a vertical thermal track
// (blue = min at the bottom -> cyan -> green -> yellow -> red = max at the top) with a handle tinted
// in the value's colour, min below, and the value. Linear: value = min + fraction * (max - min).
//
// Min and max are the dock slider's own (session.temperatureMin/Max), changed in Style.tempRangeStep
// steps through "simTempRange:<min>:<max>", or typed: pressing the value field emits editRequested()
// and the scene opens a number pad (VrNumberPad.qml). A drag sends "simTemp:<K>" (whole kelvin, only
// on a change), which acts like a drag of the dock's slider, also live during a run.
//
// The track is a drag target for the controller rays and the hands (vrDragTarget: viewerxr.qml Hand
// and VrHandInput.qml call vrDragStart / vrDragMove / vrDragEnd with track coordinates); a mouse
// drives it too (offline preview).
import QtQuick
import "VrStyle.js" as Style

Column {
    id: slider

    required property var session
    /// The min or max field was pressed: "min" / "max"; VrScene opens the number pad for it.
    signal editRequested(string which)

    readonly property real minimum: session.temperatureMin
    readonly property real maximum: Math.max(session.temperatureMax, minimum + 1)
    /// While a drag is on: the value sent last (the session's echo lags a frame behind)
    property bool dragging: false
    property real dragValue: 0
    readonly property real value: dragging ? dragValue : session.temperature

    width: 132
    spacing: 6

    /// Thermal colour of a temperature, as ThermoTrack::colorAt.
    function colorAt(kelvin) {
        var stops = [ [0.00, 30, 60, 255], [0.25, 0, 200, 255], [0.50, 30, 200, 30],
                      [0.75, 255, 210, 0], [1.00, 235, 40, 30] ]
        var t = Math.max(0, Math.min(1, (kelvin - minimum) / (maximum - minimum)))
        for (var i = 1; i < stops.length; ++i) {
            if (t <= stops[i][0]) {
                var f = (t - stops[i - 1][0]) / (stops[i][0] - stops[i - 1][0])
                return Qt.rgba((stops[i - 1][1] + f * (stops[i][1] - stops[i - 1][1])) / 255,
                               (stops[i - 1][2] + f * (stops[i][2] - stops[i - 1][2])) / 255,
                               (stops[i - 1][3] + f * (stops[i][3] - stops[i - 1][3])) / 255, 1)
            }
        }
        return Qt.rgba(235 / 255, 40 / 255, 30 / 255, 1)
    }

    function setRange(minimumK, maximumK) {
        minimumK = Math.max(0, minimumK)
        maximumK = Math.max(maximumK, minimumK + Style.tempRangeStep)
        session.command("simTempRange:" + minimumK.toFixed(0) + ":" + maximumK.toFixed(0))
    }

    Text {
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        text: "Temperature"
        color: Style.textDim
        font.pixelSize: Style.fontSmall
    }

    RangeRow {
        label: "max"
        kelvin: slider.maximum
        onEdit: slider.editRequested("max")
        onDecrease: slider.setRange(slider.minimum, slider.maximum - Style.tempRangeStep)
        onIncrease: slider.setRange(slider.minimum, slider.maximum + Style.tempRangeStep)
    }

    // ---- the track: drag target --------------------------------------------------------------
    Item {
        id: track

        /// VrPanelHost.buttonAt looks for vrRayTarget; vrDragTarget makes the hands and controllers
        /// call the vrDrag* functions instead of clicked(). Hover/press state as on VrButton.
        readonly property bool vrRayTarget: true
        readonly property bool vrDragTarget: true
        property int rayHoverCount: 0
        property bool rayDown: false

        readonly property real barWidth: 22
        readonly property real margin: 10      // room for the handle at both ends
        readonly property real barHeight: height - 2 * margin

        width: parent.width
        height: Style.tempTrackHeight

        function valueAt(y) {
            var fraction = Math.max(0, Math.min(1, (margin + barHeight - y) / barHeight))
            return Math.round(slider.minimum + fraction * (slider.maximum - slider.minimum))
        }
        function vrDragStart(x, y) {
            slider.dragging = true
            slider.dragValue = -1
            vrDragMove(x, y)
        }
        function vrDragMove(x, y) {
            var kelvin = valueAt(y)
            if (kelvin === slider.dragValue)
                return
            slider.dragValue = kelvin
            slider.session.command("simTemp:" + kelvin)
        }
        function vrDragEnd() {
            slider.dragging = false
        }

        Rectangle {
            id: bar
            x: (track.width - track.barWidth) / 2
            y: track.margin
            width: track.barWidth
            height: track.barHeight
            radius: 6
            border.width: track.rayHoverCount > 0 || track.rayDown ? 3 : 1
            border.color: track.rayHoverCount > 0 || track.rayDown ? Style.hoverRing : Style.buttonBorder
            gradient: Gradient {
                GradientStop { position: 0.00; color: Qt.rgba(235 / 255, 40 / 255, 30 / 255, 1) }
                GradientStop { position: 0.25; color: Qt.rgba(255 / 255, 210 / 255, 0, 1) }
                GradientStop { position: 0.50; color: Qt.rgba(30 / 255, 200 / 255, 30 / 255, 1) }
                GradientStop { position: 0.75; color: Qt.rgba(0, 200 / 255, 255 / 255, 1) }
                GradientStop { position: 1.00; color: Qt.rgba(30 / 255, 60 / 255, 255 / 255, 1) }
            }
        }
        // handle: a bar across the track at the value, in the value's colour
        Rectangle {
            readonly property real fraction: Math.max(0, Math.min(1, (slider.value - slider.minimum)
                                                                     / (slider.maximum - slider.minimum)))
            x: bar.x - 10
            y: track.margin + (1 - fraction) * track.barHeight - height / 2
            width: track.barWidth + 20
            height: 14
            radius: 4
            color: slider.colorAt(slider.value)
            border.width: 2
            border.color: Qt.darker(color, 1.6)
        }
        MouseArea {
            anchors.fill: parent
            onPressed: (mouse) => track.vrDragStart(mouse.x, mouse.y)
            onPositionChanged: (mouse) => track.vrDragMove(mouse.x, mouse.y)
            onReleased: track.vrDragEnd()
        }
    }

    RangeRow {
        label: "min"
        kelvin: slider.minimum
        onEdit: slider.editRequested("min")
        onDecrease: slider.setRange(slider.minimum - Style.tempRangeStep, slider.maximum)
        onIncrease: slider.setRange(Math.min(slider.minimum + Style.tempRangeStep,
                                             slider.maximum - Style.tempRangeStep), slider.maximum)
    }

    Text {
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        text: slider.session.temperature > 0 || slider.dragging ? slider.value.toFixed(0) + " K" : "–"
        color: Qt.lighter(slider.colorAt(slider.value), 1.2)
        font.pixelSize: Style.fontTitle
        font.weight: Font.Bold
    }

    // label, then − [value field] + for min and max; the field opens the number pad
    component RangeRow: Column {
        id: row
        property string label
        property real kelvin
        signal decrease()
        signal increase()
        signal edit()

        width: parent ? parent.width : 132
        spacing: 2

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: row.label
            color: Style.textDim
            font.pixelSize: Style.fontSmall - 1
        }
        Row {
            spacing: 4
            VrButton {
                width: 30
                height: 36
                text: "−"
                font.pixelSize: 16
                onClicked: row.decrease()
            }
            VrButton {
                width: row.width - 2 * 30 - 2 * 4
                height: 36
                tone: "field"
                text: row.kelvin.toFixed(0) + " K"
                font.pixelSize: Style.fontSmall + 1
                leftPadding: 2
                rightPadding: 2
                onClicked: row.edit()
            }
            VrButton {
                width: 30
                height: 36
                text: "+"
                font.pixelSize: 16
                onClicked: row.increase()
            }
        }
    }
}
