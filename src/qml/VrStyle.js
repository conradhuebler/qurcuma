// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// One look for all headset panels (Claude Generated 2026). Colours follow the desktop dark theme
// (stylesheets/dark.qss: backgrounds #1e1e1e/#252525/#2d2d2d, border #3d3d3d, text #e0e0e0, buttons
// #0d47a1 -> #1565c0 on hover) and the state colours of the Simulation dock.
//
// Panel pixels are millimetres at text size "Normal": VrPanelHost maps 10 px to 1 cm, divided by the
// text size factor (textScales). Panels hang 55-60 cm from the head, where 1 degree is about 10 mm,
// so body text is about 1.5 degrees tall (UX test 2026-10: 1.2 degrees was hard to read, above all for
// short-sighted users) and a button about 5 degrees, a target a hand-held controller ray can hold.
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
var selected = "#0d47a1"      // button in use (running MD/Opt, chosen method), also while disabled
var selectedRing = "#90caf9"
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
var markerMove = "#c080ff"      // carrying the molecule (Claude Generated 2026)
// run states, as in the Simulation dock
var running = "#27ae60"
var paused = "#d4a017"
var aborted = "#c0392b"
var idle = "#7f8c8d"

// ---- metrics (px = mm) -----------------------------------------------------------------------
var fontTitle = 18
var fontBody = 15
var fontSmall = 13
var fontButton = 16
var padding = 16
var spacing = 8
var radius = 12
var headerHeight = 28
var buttonHeight = 48
var buttonRadius = 8
var panelGapDegrees = 3      // gap between the edges of neighbouring console panels (Claude Generated 2026)
var listRowHeight = 36

// ---- locomotion and surroundings (Claude Generated 2026) ----------------------------------------
// UX test 2026-10: 150 cm/s at full tilt carried users through the molecule "suddenly and
// exhausting"; ratings split 2/2/2, so the speed is a choice (VrSession.walkSpeedLevel) with a
// cautious default, the stick answers with tilt^walkCurve (fine control near the centre) and the
// speed eases in and out over walkEaseSeconds instead of jumping.
var walkSpeeds = [15, 35, 90]               // cm/s at full tilt: Slow, Normal (default), Fast
var walkSpeedNames = ["Slow", "Normal", "Fast"]
var walkCurve = 2
var walkEaseSeconds = 0.35   // time constant of the speed's approach to the stick's target
var vignetteMaxOpacity = 0.75   // comfort vignette at full walking speed (0 = off)
var snapTurnDegrees = 45     // right-stick snap turn
// Zoom at the molecule (controllers): right stick up/down scales it about its centre, at most by
// e^zoomRate per second; both grips held and pulled apart scale it like a pinch with both hands.
var zoomRate = 1.2
var eyeHeight = 160          // cm, floor below the starting head when the runtime has no floor

// ---- hand input: pinch and direct touch (Claude Generated 2026) ---------------------------------
// Distances in cm between OpenXR joint centres; the tip joints sit about 1 cm apart when the pads
// touch. Two thresholds (hysteresis) so a pinch held near the limit does not flicker.
var pinchPress = 2.0         // thumb tip to index tip below this: pinch starts
var pinchRelease = 3.0       // above this: pinch ends
var touchFront = 1.5         // the pinch point touches a panel this close in front of its surface...
var touchBehind = 1.5        // ...or this far behind it (hands pass through panels)
var moleculeTouch = 4.0      // pinch point this close (cm) to an atom's surface holds the molecule (no simulation)
var atomTouch = 0.5          // an atom's drawn surface this close to the pinch point (or the point inside it) is touched
var tapMilliseconds = 300    // a pinch shorter than this...
var tapDistance = 1.5        // ...that moved less than this (cm) is a tap: select
var handLostMilliseconds = 250  // no joint data for this long: the hand is not tracked
var markerAtom = "#ffe040"
var selectionHalo = "#ff66ff"   // shell around selected atoms (the desktop selection is magenta)
var highlight = "#fff4b0"       // bright shell around the atom under a ray or fingertip
var highlightScale = 1.3        // highlight radius = max(scale x drawn radius, drawn radius + margin)
var highlightMargin = 0.2       // Angstrom   // pinch point over an atom
var handJoint = "#c8d4e0"

// ---- thermostat slider (Claude Generated 2026) ---------------------------------------------------
var tempTrackHeight = 150    // px = mm at text size Normal: a 15 cm track (was 20; keeps the Simulation panel low enough)
var tempRangeStep = 50       // K per press of the min / max buttons

// ---- text size (Claude Generated 2026) -----------------------------------------------------------
// Panel scale factors for VrSession.textSizeLevel: everything on the panels grows together.
var textScales = [1.0, 1.2, 1.4]
var textScaleNames = ["Normal", "Large", "XL"]

// ---- layout (Claude Generated 2026) ---------------------------------------------------------------
// UX test 2026-10: "no overlap of molecule and control panels in the default layout". VrScene derives
// the panel pitches from the molecule's bounding sphere at its default size and the panels' heights.
var moleculePitch = 3        // degrees above eye height (the space below goes to the panels)
var layoutGapDegrees = 2     // between the molecule's sphere, the menu bar and the console panels
var consoleLowestPitch = -40 // the console panels never go lower; a big molecule may then reach them
var consoleLowestPitchHands = -48   // hand input: 45 cm away, chest height, where relaxed arms reach
var infoGapDegrees = 4       // between the molecule's sphere and help / tutorial / file browser

// ---- tutorial (Claude Generated 2026) -------------------------------------------------------------
var tutorialDone = "#27ae60"
var tutorialAdvanceMilliseconds = 1200   // a finished step shows its tick this long, then the next one

// ---- movable panels (Claude Generated 2026) ------------------------------------------------------
var handleWidth = 16         // cm: target area of a panel's handle (the pill drawn inside is smaller)
var handleHeight = 3
var handleGap = 1            // cm between panel edge and handle
var handle = "#80a0a0a0"
var handleHover = "#e0e0e0"
var handleHeld = "#1565c0"
var panelMinDistance = 25    // cm from the head a moved panel may come...
var panelMaxDistance = 200   // ...or go

// ---- number pad and edit fields (Claude Generated 2026) ------------------------------------------
var field = "#121212"
var fieldHover = "#1c2430"
var fieldBorder = "#5a7fa8"
var padMaxDigits = 6
