# src/qml: Qt Quick 3D scene

- `viewer3d.qml` is the only scene file; bound to `SceneController` (`../scenecontroller.*`) properties.
- Camera is fixed and axis-aligned; the molecule rotates under `moleculeRoot`. C++ replicates the projection for picking and labels (Quick3D `Viewport`/`Camera` are private headers).
- Atoms and bonds are `QQuick3DInstancing` (`../atominstancing.*`, `../bondinstancing.*`); effects via `ExtendedSceneEnvironment`.
- Transparency of instanced overlays needs the alpha in the instance colour plus `alphaMode: Blend`.
- A QML change is not verifiable from the agent's shell; the operator checks it on screen.
