# WP: VR usability after the UX test of 2026-10-20

Source: survey after the VR demo (6 complete answers, LimeSurvey). Tendencies only, no statistics.
Status of every change below: 🤖 AI-generated, compiles, panels checked in the offline preview
(`spikes/vr_preview`); **not tried on the headset**.

## Findings and what was changed

| Finding (survey) | Change |
|---|---|
| 5/6 want better controls first: speed, sensitivity, zoom | speed levels Slow / Normal / Fast (15 / 35 / 90 cm/s, was 150), stick response tilt², speed eases in and out (0.35 s); right stick ↕ zooms the molecule, both grips resize it |
| Speed ratings split 2 / 2 / 2; "extremely fast through the molecule" (with motion sickness) | default Normal (35 cm/s); comfort vignette darkens the view edge while walking (`VrVignette.qml`) |
| "Zoom more intuitively right at the molecule" | zoom about the molecule's centre instead of flying through it |
| Molecule drawn twice the size shown ("40 cm" was its radius) | `moleculeSize` is now the bounding-sphere diameter (`sceneExtent` is a radius) |
| "No overlap of molecule and control panels in the default layout" | help, tutorial and file browser beside the molecule; menu bar and console pitches derived from the molecule's sphere and the panel heights (`VrScene.consolePitch`, `infoPitch`) |
| 3/6 eye strain, "short-sighted, hard to see" | fonts about +25 % (body ≈ 1.5° at 58 cm); text size Normal / Large / XL (panel scale 1 / 1.2 / 1.4) |
| "Too complicated without an introduction" | Tutorial (menu bar, Help, desktop View ▸ Start VR Tutorial): one step per control, ticks off when done |

Desktop View menu: VR Movement Speed, VR Text Size (remembered in `Settings`), Start VR Tutorial.

## Trade-offs

- Console panels stand lower than before (Structure −34°, Simulation at the −40° limit; hands −48°):
  the price of keeping the molecule free. At text size XL the panels reach the menu bar.
- The layout keeps the molecule's *bounding sphere* at its default size free; a user who enlarges the
  molecule may make it reach the panels.

## Open

- Operator test on the headset: speeds, vignette strength, zoom rate, readability, tutorial flow.
- Not code: briefing before demos (IPD, glasses spacer, breaks); next test round with SUS and SSQ.
- Larger topics from the survey (ROADMAP material): re-joining atoms/bonds, chirality and reaction
  mechanism scenarios.
