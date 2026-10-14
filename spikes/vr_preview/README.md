# VR preview (no headset)

Renders the headset scene `src/qml/VrScene.qml` (molecule + panels, the same files qurcuma uses in
the XrView) in a plain window, driven by `MockVr.qml` (stands in for `VrSession`) and
`MockMolecule.qml` (n-decane instead of the desktop `SceneController`; its `tables` object stands in
for the `XrSceneBridge` that mirrors the controller's instance tables). The camera is the head: at the
origin, looking along -Z, horizontal field of view 100° by default. No stereo, no lens distortion.

QML is read from the source tree, so edits to `src/qml/*.qml` need no rebuild.

```sh
cmake -S spikes/vr_preview -B spikes/vr_preview/build -G Ninja
cmake --build spikes/vr_preview/build

# PNG without a window (offscreen; Qt Quick 3D runs on an OpenGL core context there)
QT_QPA_PLATFORM=offscreen spikes/vr_preview/build/vr_preview --state files --pitch -8 --shot /tmp/vr_files.png

# interactive: left click = panels, right drag = look around, wheel = field of view
spikes/vr_preview/build/vr_preview --state running
```

- `--state`: `idle`, `trajectory`, `running`, `paused`, `aborted`, `files` (file browser open),
  `help` (help panel open), `hidden` (panels hidden, only the menu bar),
  `overlays` (three picked carbons with measurement lines, as the desktop draws them)
- `--yaw` / `--pitch`: head direction in degrees (yaw > 0 = left, pitch > 0 = up; default 0 / -12).
  The Simulation panel sits at yaw 37, the View panel at yaw -34, both at pitch -28.
- `--fov`, `--size WxH`: horizontal field of view, window size
- `--click x,y` (repeatable, 400 ms apart, before `--shot`): left clicks at window positions
- `--qml file`: load another QML file instead of `Preview.qml`

The warning "Could not find XrView for XrItem" is expected here: each panel's XrItem looks for an
enclosing XrView, and outside the headset there is none. Qt sends log output to the journal when
stderr is not a terminal; `QT_FORCE_STDERR_LOGGING=1` keeps it on stderr.
