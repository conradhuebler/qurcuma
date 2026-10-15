// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// One look for all headset panels (Claude Generated 2026). Colours follow the desktop dark theme
// (stylesheets/dark.qss: backgrounds #1e1e1e/#252525/#2d2d2d, border #3d3d3d, text #e0e0e0, buttons
// #0d47a1 -> #1565c0 on hover) and the state colours of the Simulation dock.
//
// Panel pixels are millimetres: VrPanelHost maps 10 px to 1 cm. Panels hang 55-60 cm from the head,
// where 1 degree is about 10 mm, so body text is about 1.2 degrees tall and a button about 4 degrees,
// a target a hand-held controller ray can hold.
.pragma library

var pixelsPerCm = 10

// ---- colours ---------------------------------------------------------------------------------
var panelColor = "#1e1e1e"
var panelBorder = "#3d3d3d"
var separator = "#333333"
var title = "#ffffff"
var text = "#e0e0e0"
var textDim = "#a0a0a0"
var textDisabled = "#666666"
var button = "#2d2d2d"
var buttonHover = "#3a3a3a"
var buttonBorder = "#555555"
var buttonDown = "#1565c0"
var buttonDisabled = "#242424"
var primary = "#0d47a1"
var primaryHover = "#1565c0"
var danger = "#8e2a20"
var dangerHover = "#c0392b"
var hoverRing = "#64b5f6"
var folder = "#90caf9"
// see-through menu bar and its "ghost" buttons (#AARRGGBB), so it never hides the molecule
var barColor = "#661e1e1e"
var barBorder = "#553d3d3d"
var ghost = "#402d2d2d"
var ghostHover = "#803a3a3a"
var ghostOn = "#990d47a1"
var ghostOnHover = "#cc1565c0"
// controller rays and markers (same for both hands)
var rayIdle = "#80c0ff"
var rayPanel = "#64b5f6"
var markerIdle = "#40ff80"
var markerPanel = "#64b5f6"
var markerRotate = "#ffa030"
var markerGrab = "#ff4040"
// run states, as in the Simulation dock
var running = "#27ae60"
var paused = "#d4a017"
var aborted = "#c0392b"
var idle = "#7f8c8d"

// ---- metrics (px = mm) -----------------------------------------------------------------------
var fontTitle = 15
var fontBody = 12
var fontSmall = 10
var fontButton = 13
var padding = 14
var spacing = 8
var radius = 12
var headerHeight = 24
var buttonHeight = 44
var buttonRadius = 8
var listRowHeight = 36

// ---- locomotion and surroundings (Claude Generated 2026) ----------------------------------------
var walkSpeed = 150          // cm/s at full left-stick tilt, a brisk walk
var snapTurnDegrees = 45     // right-stick snap turn
var eyeHeight = 160          // cm, floor below the starting head when the runtime has no floor

// ---- hand input: pinch and direct touch (Claude Generated 2026) ---------------------------------
// Distances in cm between OpenXR joint centres; the tip joints sit about 1 cm apart when the pads
// touch. Two thresholds (hysteresis) so a pinch held near the limit does not flicker.
var pinchPress = 2.0         // thumb tip to index tip below this: pinch starts
var pinchRelease = 3.0       // above this: pinch ends
var touchFront = 1.5         // the pinch point touches a panel this close in front of its surface...
var touchBehind = 1.5        // ...or this far behind it (hands pass through panels)
var atomTouch = 0.5          // an atom's drawn surface this close to the pinch point (or the point inside it) is touched
var tapMilliseconds = 300    // a pinch shorter than this...
var tapDistance = 1.5        // ...that moved less than this (cm) is a tap: select
var handLostMilliseconds = 250  // no joint data for this long: the hand is not tracked
var markerAtom = "#ffe040"   // pinch point over an atom
var handJoint = "#c8d4e0"
