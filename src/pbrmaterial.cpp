// PBRMaterial Implementation - Claude Generated
// Phase 4A: Physically-Based Rendering Material

#include "pbrmaterial.h"
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QRenderPass>
#include <Qt3DRender/QShaderProgram>
#include <Qt3DRender/QParameter>
#include <Qt3DRender/QFilterKey>
#include <Qt3DRender/QGraphicsApiFilter>
#include <QDebug>

PBRMaterial::PBRMaterial(Qt3DCore::QNode *parent)
    : Qt3DRender::QMaterial(parent)
{
    buildMaterial();
}

PBRMaterial::~PBRMaterial()
{
}

void PBRMaterial::buildMaterial()
{
    // Create effect
    m_effect = new Qt3DRender::QEffect(this);

    // Create technique for OpenGL 3.3+
    m_technique = new Qt3DRender::QTechnique(m_effect);
    m_technique->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGL);
    m_technique->graphicsApiFilter()->setMajorVersion(3);
    m_technique->graphicsApiFilter()->setMinorVersion(3);
    m_technique->graphicsApiFilter()->setProfile(Qt3DRender::QGraphicsApiFilter::CoreProfile);

    // Create render pass
    m_renderPass = new Qt3DRender::QRenderPass(m_technique);

    // Create shader program
    Qt3DRender::QShaderProgram *shaderProgram = new Qt3DRender::QShaderProgram(m_renderPass);

    // Load PBR shaders from resources
    shaderProgram->setVertexShaderCode(Qt3DRender::QShaderProgram::loadSource(QUrl("qrc:/shaders/pbr.vert")));
    shaderProgram->setFragmentShaderCode(Qt3DRender::QShaderProgram::loadSource(QUrl("qrc:/shaders/pbr.frag")));

    m_renderPass->setShaderProgram(shaderProgram);

    // Create and add parameters
    m_baseColorParameter = new Qt3DRender::QParameter("baseColor", m_baseColor, this);
    m_metallicParameter = new Qt3DRender::QParameter("metallic", m_metallic, this);
    m_roughnessParameter = new Qt3DRender::QParameter("roughness", m_roughness, this);
    m_aoParameter = new Qt3DRender::QParameter("ambientOcclusion", m_ambientOcclusion, this);

    // Lighting parameters (will be updated by viewer)
    m_lightPositionParameter = new Qt3DRender::QParameter("lightPosition", QVector3D(100, 100, 100), this);
    m_lightColorParameter = new Qt3DRender::QParameter("lightColor", QVector3D(1.0, 1.0, 1.0), this);
    m_cameraPositionParameter = new Qt3DRender::QParameter("cameraPosition", QVector3D(0, 0, 100), this);

    addParameter(m_baseColorParameter);
    addParameter(m_metallicParameter);
    addParameter(m_roughnessParameter);
    addParameter(m_aoParameter);
    addParameter(m_lightPositionParameter);
    addParameter(m_lightColorParameter);
    addParameter(m_cameraPositionParameter);

    // Add technique to effect
    m_technique->addRenderPass(m_renderPass);
    m_effect->addTechnique(m_technique);

    // Set effect on material
    setEffect(m_effect);

    qDebug() << "PBRMaterial created with Cook-Torrance BRDF shaders";
}

void PBRMaterial::setBaseColor(const QColor &color)
{
    if (m_baseColor != color) {
        m_baseColor = color;
        m_baseColorParameter->setValue(color);
        emit baseColorChanged(color);
    }
}

void PBRMaterial::setMetallic(float value)
{
    value = qBound(0.0f, value, 1.0f);
    if (!qFuzzyCompare(m_metallic, value)) {
        m_metallic = value;
        m_metallicParameter->setValue(value);
        emit metallicChanged(value);
    }
}

void PBRMaterial::setRoughness(float value)
{
    value = qBound(0.0f, value, 1.0f);
    if (!qFuzzyCompare(m_roughness, value)) {
        m_roughness = value;
        m_roughnessParameter->setValue(value);
        emit roughnessChanged(value);
    }
}

void PBRMaterial::setAmbientOcclusion(float value)
{
    value = qBound(0.0f, value, 1.0f);
    if (!qFuzzyCompare(m_ambientOcclusion, value)) {
        m_ambientOcclusion = value;
        m_aoParameter->setValue(value);
        emit ambientOcclusionChanged(value);
    }
}

void PBRMaterial::setMaterialPreset(MaterialPreset preset)
{
    // Claude Generated - Material presets for common material types
    // Reference: Physically-based material guidelines

    switch (preset) {
        case Plastic:
            setMetallic(0.0f);        // Non-metallic
            setRoughness(0.5f);       // Medium roughness
            qDebug() << "Set to Plastic material (metallic=0.0, roughness=0.5)";
            break;

        case Metal:
            setMetallic(1.0f);        // Fully metallic
            setRoughness(0.2f);       // Smooth, polished surface
            qDebug() << "Set to Metal material (metallic=1.0, roughness=0.2)";
            break;

        case Glass:
            setMetallic(0.0f);        // Non-metallic
            setRoughness(0.05f);      // Very smooth, transparent-like
            qDebug() << "Set to Glass material (metallic=0.0, roughness=0.05)";
            break;

        case Rubber:
            setMetallic(0.0f);        // Non-metallic
            setRoughness(0.8f);       // Rough surface
            qDebug() << "Set to Rubber material (metallic=0.0, roughness=0.8)";
            break;

        default:
            break;
    }
}
