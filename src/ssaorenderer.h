// Claude Generated - Phase 3A - SSAO Renderer with Post-Processing
#ifndef SSAORENDERER_H
#define SSAORENDERER_H

#include <Qt3DCore/QEntity>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QRenderPass>
#include <QObject>

/**
 * @brief SSAO (Screen-Space Ambient Occlusion) Renderer
 *
 * Manages SSAO post-processing shader material with:
 * - Configurable radius, intensity, and sample count
 * - Graceful fallback for unsupported GPUs
 * - Blur pass for noise reduction
 */
class SSAORenderer : public QObject
{
    Q_OBJECT

public:
    explicit SSAORenderer(QObject *parent = nullptr);
    ~SSAORenderer();

    // SSAO Control
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_ssaoEnabled; }

    // SSAO Parameters
    void setRadius(float radius);
    float getRadius() const { return m_ssaoRadius; }

    void setIntensity(float intensity);
    float getIntensity() const { return m_ssaoIntensity; }

    void setSampleCount(int count);
    int getSampleCount() const { return m_sampleCount; }

    void setBias(float bias);
    float getBias() const { return m_ssoaBias; }

    // Material for 3D rendering with SSAO
    Qt3DRender::QMaterial* getSSAOMaterial() const { return m_ssaoMaterial; }

    // Check if SSAO is supported on this GPU
    bool isSupported() const { return m_ssaoSupported; }

    // Error message if shader compilation failed
    QString getLastError() const { return m_lastError; }

signals:
    void enabledChanged(bool enabled);
    void radiusChanged(float radius);
    void intensityChanged(float intensity);
    void sampleCountChanged(int count);

private:
    void createSSAOMaterial();
    void setupShaderProgram();
    bool compileShaders();
    void setupFallback();

    // Material components
    Qt3DRender::QMaterial *m_ssaoMaterial = nullptr;
    Qt3DRender::QEffect *m_effect = nullptr;
    Qt3DRender::QTechnique *m_technique = nullptr;
    Qt3DRender::QRenderPass *m_renderPass = nullptr;

    // SSAO Parameters
    bool m_ssaoEnabled = false;
    float m_ssaoRadius = 0.5f;      // Screen-space radius (0.1-2.0)
    float m_ssaoIntensity = 1.0f;   // Occlusion strength (0.0-2.0)
    float m_ssoaBias = 0.025f;      // Depth bias for artifacts prevention
    int m_sampleCount = 16;         // Number of samples (8, 16, 32, 64)

    // Status
    bool m_ssaoSupported = true;
    QString m_lastError = "";
};

#endif // SSAORENDERER_H
