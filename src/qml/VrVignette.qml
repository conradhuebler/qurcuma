// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Comfort vignette (Claude Generated 2026): a dark ring around the field of view, shown while the user
// glides with the left stick (UX test 2026-10: 2 of 6 reported motion sickness). Put it into the head
// node (it covers the view from 10 cm in front of the eyes); strength 0 hides it, 1 = darkest
// (Style.vignetteMaxOpacity is applied by the caller). The clear middle keeps about +-35 degrees.
import QtQuick
import QtQuick3D

Node {
    id: vignette

    /// 0 = off, 1 = full
    property real strength: 0

    Model {
        visible: vignette.strength > 0.01
        source: "#Rectangle"            // 100 x 100 units in the xy plane, facing +z
        position: Qt.vector3d(0, 0, -10)
        scale: Qt.vector3d(0.6, 0.6, 1)  // 60 cm square at 10 cm: beyond +-70 degrees
        pickable: false
        castsShadows: false
        receivesShadows: false
        materials: CustomMaterial {
            property real inner: 0.25    // fraction of the half width: clear up to here...
            property real outer: 0.6     // ...black from here on (about +-35 / +-60 degrees)
            property real strength: vignette.strength
            shadingMode: CustomMaterial.Unshaded
            sourceBlend: CustomMaterial.SrcAlpha
            destinationBlend: CustomMaterial.OneMinusSrcAlpha
            // darken the colour only; the target's alpha stays (else a see-through target shows the
            // background behind the view through the dark edge)
            sourceAlphaBlend: CustomMaterial.Zero
            destinationAlphaBlend: CustomMaterial.One
            depthDrawMode: Material.NeverDepthDraw
            cullMode: Material.NoCulling
            vertexShader: "VrVignette.vert"
            fragmentShader: "VrVignette.frag"
        }
    }
}
