// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// qurcuma 3D viewer scene (Qt Quick 3D). "controller" = SceneController context
// property set by view.cpp. All transform/effect state lives in the controller and
// mouse/picking/grab are driven from C++; this file stays declarative. The molecule
// rotates under moleculeRoot (model rotation about sceneCenter) while the camera
// stays axis-aligned, so C++ can replicate the projection for picking/grab without
// the private QQuick3DViewport/QQuick3DCamera headers. Claude Generated.
import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

Item {
    id: root

    View3D {
        id: view3D
        anchors.fill: parent
        camera: camera

        environment: ExtendedSceneEnvironment {
            backgroundMode: SceneEnvironment.Color
            clearColor: controller.backgroundColor
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High

            aoEnabled: controller.ssaoEnabled
            aoStrength: controller.ssaoStrength * 90
            aoDistance: 4.0
            aoSoftness: 30

            glowEnabled: controller.bloomEnabled
            glowStrength: 0.9
            glowIntensity: 0.7
            glowBloom: 0.4

            tonemapMode: controller.tonemapEnabled
                ? SceneEnvironment.TonemapModeFilmic
                : SceneEnvironment.TonemapModeNone

            // Optional depth fog: atoms farther from the camera fade into the
            // background. The near/far band follows the molecule's depth range so
            // the effect stays meaningful as you zoom. density = user-set strength.
            fog: Fog {
                enabled: controller.fogEnabled
                color: controller.backgroundColor
                density: controller.fogDensity   // strength
                depthEnabled: true
                // distance: fogDistance (0..1) slides where the fog starts, from the
                // front of the molecule (0) to its back (1); the far plane sits beyond.
                // distance slides the fog START from the molecule front (0) toward
                // its back (1); the band is kept tight (~0.8 extent) so everything
                // past it sits at full density -> the fade is strong, not washed out.
                depthNear: Math.max(0.1, controller.cameraDistance - controller.sceneExtent
                                         + controller.fogDistance * 1.5 * controller.sceneExtent)
                depthFar: depthNear + Math.max(0.5, controller.sceneExtent * 0.8)
                depthCurve: 1.0
            }
        }

        // Axis-aligned camera node: sits at pivotPosition (sceneCenter + pan), camera
        // child pulled back by cameraDistance. No rotation -> the molecule rotates, not
        // the camera (matches the former Qt3D "Model rotation" default).
        Node {
            id: cameraNode
            position: controller.pivotPosition
            PerspectiveCamera {
                id: camera
                z: controller.cameraDistance
                clipNear: 0.5
                clipFar: controller.cameraDistance * 8.0 + controller.sceneExtent * 8.0 + 1000.0
                fieldOfView: controller.fieldOfView

                // Screen-fixed corner lights (parented to camera; lit zone does not
                // rotate with the molecule) — matches the former Qt3D corner lights.
                DirectionalLight { eulerRotation.x: -30; eulerRotation.y: 30;  brightness: 0.7; visible: controller.cornerLight0; castsShadow: false }
                DirectionalLight { eulerRotation.x: -30; eulerRotation.y: -30; brightness: 0.7; visible: controller.cornerLight1; castsShadow: false }
                DirectionalLight { eulerRotation.x: 30;  eulerRotation.y: 30;  brightness: 0.7; visible: controller.cornerLight2; castsShadow: false }
                DirectionalLight { eulerRotation.x: 30;  eulerRotation.y: -30; brightness: 0.7; visible: controller.cornerLight3; castsShadow: false }
            }
        }

        // World-fixed key light -> world-fixed cast shadows.
        DirectionalLight {
            eulerRotation.x: -40
            eulerRotation.y: -50
            brightness: 0.6
            castsShadow: controller.shadowsEnabled
            shadowMapQuality: Light.ShadowMapQualityHigh
            shadowFactor: 80
        }

        // Floor (world-horizontal) to receive cast shadows.
        Model {
            source: "#Rectangle"
            visible: controller.shadowsEnabled
            position: Qt.vector3d(controller.sceneCenter.x,
                                  controller.sceneCenter.y - controller.sceneExtent * 1.1,
                                  controller.sceneCenter.z)
            eulerRotation.x: -90
            scale: Qt.vector3d(controller.sceneExtent * 0.3, controller.sceneExtent * 0.3, 1)
            materials: PrincipledMaterial { baseColor: "#3a4252"; roughness: 0.9 }
        }

        // Molecule root: rotates about sceneCenter (model rotation).
        Node {
            id: moleculeRoot
            position: controller.sceneCenter
            pivot: controller.sceneCenter
            rotation: controller.rootRotation

            Model {
                id: atomModel
                source: "#Sphere"
                visible: controller.atomsVisible
                instancing: controller.atomInstancing
                materials: PrincipledMaterial {
                    baseColor: "white"
                    metalness: 0.0
                    roughness: 0.4
                    // Opaque by default (clean depth/no seams); blend only when the
                    // user actually requests transparency.
                    alphaMode: controller.blendEnabled ? PrincipledMaterial.Blend
                                                       : PrincipledMaterial.Opaque
                }
            }

            Model {
                id: bondModel
                source: "#Cylinder"
                visible: controller.bondsVisible
                instancing: controller.bondInstancing
                materials: PrincipledMaterial {
                    baseColor: "white"
                    metalness: 0.0
                    roughness: 0.55
                    alphaMode: controller.blendEnabled ? PrincipledMaterial.Blend
                                                       : PrincipledMaterial.Opaque
                }
            }

            // RMSD overlay: second structure, opaque, with HSV-shifted element
            // colours (set per-instance) so it reads as the "other" molecule while
            // staying element-coded. Shares the molecule's rotation.
            Model {
                source: "#Sphere"
                visible: controller.overlayVisible
                instancing: controller.overlayAtomInstancing
                materials: PrincipledMaterial { baseColor: "white"; roughness: 0.4 }
            }
            Model {
                source: "#Cylinder"
                visible: controller.overlayVisible
                instancing: controller.overlayBondInstancing
                materials: PrincipledMaterial { baseColor: "white"; roughness: 0.55 }
            }

            // Confinement-wall wireframe (harmonic walls from the interactive MD
            // config). Edges are in intrinsic atom coordinates, so this sits under
            // moleculeRoot and rotates with the structure. Unlit flat overlay.
            // The per-segment instance colour carries the grey/red RGB AND the
            // alpha (baked from wallOpacity in SceneController::rebuildWall); the
            // material baseColor stays opaque white and only flips to Blend mode
            // when opacity < 1, otherwise PrincipledMaterial defaults to Opaque and
            // silently discards the alpha (the "immer überdeckend" bug).
            Model {
                source: "#Cylinder"
                visible: controller.wallVisible
                instancing: controller.wallInstancing
                materials: PrincipledMaterial {
                    baseColor: "white"
                    lighting: PrincipledMaterial.NoLighting
                    alphaMode: controller.wallOpacity < 0.999
                               ? PrincipledMaterial.Blend
                               : PrincipledMaterial.Opaque
                }
            }

            // Iso-potential gradient shells (optional; enabled via Display panel).
            // 3 inside shells (blue→teal) + 3 outside shells (yellow→red)
            // at force-contour distances from the wall boundary.
            Model {
                source: "#Cylinder"
                visible: controller.wallPotShellsVisible
                instancing: controller.wallPotShellsInstancing
                materials: PrincipledMaterial {
                    baseColor: "white"
                    lighting: PrincipledMaterial.NoLighting
                    alphaMode: PrincipledMaterial.Blend
                }
            }

            // Wall force vector field: arrows sampled on a grid around the boundary.
            // Shafts (cylinder) + tips (cone), colour/length = force magnitude.
            Model {
                source: "#Cylinder"
                visible: controller.wallForceArrowsVisible
                instancing: controller.wallForceShaftsInstancing
                materials: PrincipledMaterial {
                    baseColor: "white"
                    lighting: PrincipledMaterial.NoLighting
                    alphaMode: PrincipledMaterial.Blend
                }
            }
            Model {
                source: "#Cone"
                visible: controller.wallForceArrowsVisible
                instancing: controller.wallForceTipsInstancing
                materials: PrincipledMaterial {
                    baseColor: "white"
                    lighting: PrincipledMaterial.NoLighting
                    alphaMode: PrincipledMaterial.Blend
                }
            }
        }

        // Force-vector arrows (opt-in). C++ computes them in WORLD space (post model
        // rotation), so they live outside moleculeRoot. Unlit so the instance colour
        // reads as a bright, flat overlay.
        Model {
            source: "#Cylinder"
            visible: controller.forceVectorsVisible
            instancing: controller.arrowShaftInstancing
            materials: PrincipledMaterial {
                baseColor: "white"
                lighting: PrincipledMaterial.NoLighting
            }
        }
        Model {
            source: "#Cone"
            visible: controller.forceVectorsVisible
            instancing: controller.arrowTipInstancing
            materials: PrincipledMaterial {
                baseColor: "white"
                lighting: PrincipledMaterial.NoLighting
            }
        }

        // Measurement lines (distance/angle/dihedral) — world space, unlit cyan.
        Model {
            source: "#Cylinder"
            visible: controller.measurementActive
            instancing: controller.measureLineInstancing
            materials: PrincipledMaterial {
                baseColor: "white"
                lighting: PrincipledMaterial.NoLighting
            }
        }
    }

    // Rubber-band (box) selection overlay (2D, structure editing). Rect is in viewport
    // pixels supplied by C++; selection happens on mouse release.
    Rectangle {
        visible: controller.rubberBandActive
        x: controller.rubberBandRect.x
        y: controller.rubberBandRect.y
        width: controller.rubberBandRect.width
        height: controller.rubberBandRect.height
        color: "#2290e0ff"
        border.color: "#cc90e0ff"
        border.width: 1
    }

    // Edit-mode hint bar (2D overlay, bottom-centre). Empty editHint = hidden.
    Rectangle {
        visible: controller.editHint.length > 0
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 10 }
        width: Math.min(editHintText.implicitWidth + 18, root.width - 24)
        height: editHintText.implicitHeight + 8
        radius: 5
        color: "#cc101418"
        border.color: "#5affffff"
        Text {
            id: editHintText
            anchors.centerIn: parent
            width: parent.width - 16
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            color: "#d8e6ff"
            font.pixelSize: 12
            text: controller.editHint
        }
    }

    // Measurement result label (2D overlay).
    Rectangle {
        visible: controller.measurementActive
        anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 8 }
        width: measureText.width + 18
        height: measureText.height + 10
        radius: 6
        color: "#cc101418"
        border.color: "#5a90e0ff"
        Text {
            id: measureText
            anchors.centerIn: parent
            width: Math.min(implicitWidth, root.width - 40)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: "#aef0ff"
            font.pixelSize: 14
            font.bold: true
            font.family: "monospace"
            text: controller.measurementText
        }
    }
}
