// PBRMaterial - Claude Generated
// Phase 4A: Physically-Based Rendering Material Wrapper
// Encapsulates Cook-Torrance BRDF shaders with Qt3D material system

#pragma once

#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QRenderPass>
#include <Qt3DRender/QShaderProgram>
#include <Qt3DRender/QParameter>
#include <QVector3D>
#include <QColor>
#include <QUrl>

namespace Qt3DRender {
    class QParameter;
    class QEffect;
    class QMaterial;
}

class PBRMaterial : public Qt3DRender::QMaterial
{
    Q_OBJECT

    // Material parameters (Qt properties for QML/animation support)
    Q_PROPERTY(QColor baseColor READ baseColor WRITE setBaseColor NOTIFY baseColorChanged)
    Q_PROPERTY(float metallic READ metallic WRITE setMetallic NOTIFY metallicChanged)
    Q_PROPERTY(float roughness READ roughness WRITE setRoughness NOTIFY roughnessChanged)
    Q_PROPERTY(float ambientOcclusion READ ambientOcclusion WRITE setAmbientOcclusion NOTIFY ambientOcclusionChanged)

public:
    explicit PBRMaterial(Qt3DCore::QNode *parent = nullptr);
    ~PBRMaterial();

    // Property accessors
    QColor baseColor() const { return m_baseColor; }
    float metallic() const { return m_metallic; }
    float roughness() const { return m_roughness; }
    float ambientOcclusion() const { return m_ambientOcclusion; }

    // Setters
    void setBaseColor(const QColor &color);
    void setMetallic(float value);
    void setRoughness(float value);
    void setAmbientOcclusion(float value);

    // Preset materials
    enum MaterialPreset {
        Plastic,      // Low metallic, medium roughness
        Metal,        // High metallic, low roughness
        Glass,        // Low metallic, very low roughness
        Rubber        // Low metallic, high roughness
    };

    void setMaterialPreset(MaterialPreset preset);

    // Claude Generated 2026 - Phase 1 Fog
    void setFogEnabled(bool enabled);
    void setFogColor(const QColor &color);
    void setFogDensity(float density);

signals:
    void baseColorChanged(const QColor &color);
    void metallicChanged(float value);
    void roughnessChanged(float value);
    void ambientOcclusionChanged(float value);

private:
    void buildMaterial();

    // Parameters
    Qt3DRender::QParameter *m_baseColorParameter = nullptr;
    Qt3DRender::QParameter *m_metallicParameter = nullptr;
    Qt3DRender::QParameter *m_roughnessParameter = nullptr;
    Qt3DRender::QParameter *m_aoParameter = nullptr;
    Qt3DRender::QParameter *m_lightPositionParameter = nullptr;
    Qt3DRender::QParameter *m_lightColorParameter = nullptr;
    Qt3DRender::QParameter *m_cameraPositionParameter = nullptr;

    // Claude Generated 2026 - Phase 1 Fog
    Qt3DRender::QParameter *m_fogEnabledParameter = nullptr;
    Qt3DRender::QParameter *m_fogColorParameter = nullptr;
    Qt3DRender::QParameter *m_fogDensityParameter = nullptr;

    // Material state
    QColor m_baseColor = QColor(200, 200, 200);
    float m_metallic = 0.1f;
    float m_roughness = 0.5f;
    float m_ambientOcclusion = 1.0f;

    // Shader and technique
    Qt3DRender::QEffect *m_effect = nullptr;
    Qt3DRender::QTechnique *m_technique = nullptr;
    Qt3DRender::QRenderPass *m_renderPass = nullptr;
};
