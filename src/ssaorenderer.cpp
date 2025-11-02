// Claude Generated - Phase 3A - SSAO Renderer Implementation
#include "ssaorenderer.h"
#include <Qt3DRender/QShaderProgram>
#include <Qt3DRender/QParameter>
#include <Qt3DRender/QFilterKey>
#include <Qt3DRender/QRenderStateSet>
#include <QFile>
#include <QDebug>

SSAORenderer::SSAORenderer(QObject *parent)
    : QObject(parent)
{
    createSSAOMaterial();
}

SSAORenderer::~SSAORenderer()
{
}

void SSAORenderer::createSSAOMaterial()
{
    // Create effect
    m_effect = new Qt3DRender::QEffect();

    // Create technique for forward rendering
    m_technique = new Qt3DRender::QTechnique();
    m_technique->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGL);
    m_technique->graphicsApiFilter()->setMajorVersion(4);
    m_technique->graphicsApiFilter()->setMinorVersion(3);
    m_technique->graphicsApiFilter()->setProfile(Qt3DRender::QGraphicsApiFilter::CoreProfile);

    // Create render pass
    m_renderPass = new Qt3DRender::QRenderPass();

    // Create shader program for SSAO
    Qt3DRender::QShaderProgram *shaderProgram = new Qt3DRender::QShaderProgram();

    // Load shader sources
    QFile vertexShaderFile(":/shaders/ssao.vert");
    QFile fragmentShaderFile(":/shaders/ssao.frag");

    if (vertexShaderFile.open(QIODevice::ReadOnly | QIODevice::Text) &&
        fragmentShaderFile.open(QIODevice::ReadOnly | QIODevice::Text)) {

        QString vertexSource = QString::fromUtf8(vertexShaderFile.readAll());
        QString fragmentSource = QString::fromUtf8(fragmentShaderFile.readAll());

        vertexShaderFile.close();
        fragmentShaderFile.close();

        shaderProgram->setVertexShaderCode(vertexSource.toLatin1());
        shaderProgram->setFragmentShaderCode(fragmentSource.toLatin1());

        // Claude Generated - Phase 3A: SSAO Parameters
        auto *radiusParam = new Qt3DRender::QParameter("ssaoRadius", m_ssaoRadius);
        auto *intensityParam = new Qt3DRender::QParameter("ssaoIntensity", m_ssaoIntensity);
        auto *biasParam = new Qt3DRender::QParameter("ssaoBias", m_ssoaBias);

        shaderProgram->addParameter(radiusParam);
        shaderProgram->addParameter(intensityParam);
        shaderProgram->addParameter(biasParam);

        m_renderPass->setShaderProgram(shaderProgram);

        // Create material
        m_ssaoMaterial = new Qt3DRender::QMaterial();
        m_ssaoMaterial->setEffect(m_effect);

        m_technique->addRenderPass(m_renderPass);
        m_effect->addTechnique(m_technique);

        m_ssaoSupported = true;
        qDebug() << "SSAO Renderer: Shaders loaded successfully";

    } else {
        m_ssaoSupported = false;
        m_lastError = "Failed to load SSAO shader files";
        qWarning() << "SSAO Renderer:" << m_lastError;
        setupFallback();
    }
}

void SSAORenderer::setupShaderProgram()
{
    // This would be called to recompile shaders if needed
    // For now, handled in createSSAOMaterial
}

bool SSAORenderer::compileShaders()
{
    // Shader compilation is handled by Qt3D
    // This method would check compilation status
    return m_ssaoSupported;
}

void SSAORenderer::setupFallback()
{
    // Create a fallback material without SSAO
    // This is a no-op since we'll just disable SSAO if not supported
    qWarning() << "SSAO not supported on this GPU. Falling back to standard rendering.";
}

void SSAORenderer::setEnabled(bool enabled)
{
    if (m_ssaoEnabled != enabled) {
        m_ssaoEnabled = enabled && m_ssaoSupported;
        emit enabledChanged(m_ssaoEnabled);
    }
}

void SSAORenderer::setRadius(float radius)
{
    radius = qBound(0.1f, radius, 2.0f);
    if (m_ssaoRadius != radius) {
        m_ssaoRadius = radius;

        // Update shader parameter if material exists
        if (m_ssaoMaterial && m_renderPass && m_renderPass->shaderProgram()) {
            auto params = m_renderPass->shaderProgram()->parameters();
            for (auto param : params) {
                if (param->name() == "ssaoRadius") {
                    param->setValue(radius);
                    break;
                }
            }
        }

        emit radiusChanged(radius);
    }
}

void SSAORenderer::setIntensity(float intensity)
{
    intensity = qBound(0.0f, intensity, 2.0f);
    if (m_ssaoIntensity != intensity) {
        m_ssaoIntensity = intensity;

        // Update shader parameter if material exists
        if (m_ssaoMaterial && m_renderPass && m_renderPass->shaderProgram()) {
            auto params = m_renderPass->shaderProgram()->parameters();
            for (auto param : params) {
                if (param->name() == "ssaoIntensity") {
                    param->setValue(intensity);
                    break;
                }
            }
        }

        emit intensityChanged(intensity);
    }
}

void SSAORenderer::setSampleCount(int count)
{
    // Clamp to valid values
    int validCount = 8;
    if (count == 16) validCount = 16;
    else if (count == 32) validCount = 32;
    else if (count == 64) validCount = 64;

    if (m_sampleCount != validCount) {
        m_sampleCount = validCount;
        emit sampleCountChanged(validCount);
    }
}

void SSAORenderer::setBias(float bias)
{
    bias = qBound(0.0f, bias, 0.1f);
    if (m_ssoaBias != bias) {
        m_ssoaBias = bias;

        // Update shader parameter if material exists
        if (m_ssaoMaterial && m_renderPass && m_renderPass->shaderProgram()) {
            auto params = m_renderPass->shaderProgram()->parameters();
            for (auto param : params) {
                if (param->name() == "ssaoBias") {
                    param->setValue(bias);
                    break;
                }
            }
        }
    }
}
