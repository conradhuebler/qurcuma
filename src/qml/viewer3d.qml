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

            fog: Fog {
                enabled: controller.fogEnabled
                density: controller.fogDensity
                color: controller.backgroundColor
                depthEnabled: true
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
    }
}
