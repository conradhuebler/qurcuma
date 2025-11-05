// customframegraph.h - Multi-pass rendering FrameGraph for advanced effects
// Claude Generated - Phase 5A: Custom FrameGraph with SSAO, bloom, and HDR support

#ifndef CUSTOMFRAMEGRAPH_H
#define CUSTOMFRAMEGRAPH_H

#include <Qt3DRender/QFrameGraphNode>
#include <Qt3DRender/QRenderTarget>
#include <Qt3DRender/QRenderTargetOutput>
#include <Qt3DRender/QTexture>
#include <Qt3DRender/QAbstractTexture>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QFilterKey>
#include <Qt3DRender/QEffect>
#include <Qt3DCore/QEntity>
#include <QSize>
#include <memory>

/**
 * CustomFrameGraph - Multi-pass rendering pipeline for advanced effects
 *
 * Rendering passes:
 * Pass 1: Geometry Pass - Render scene to G-buffer (color, depth, normal)
 * Pass 2: SSAO Pass - Compute ambient occlusion using depth+normal
 * Pass 3: SSAO Blur Pass - Blur AO texture for smoothing
 * Pass 4: Composite Pass - Final output with AO applied
 *
 * Pass 5+: Post-processing (bloom, HDR, tone mapping) - Future phases
 *
 * Technical Architecture:
 * - Custom Qt3DRender::QFrameGraphNode hierarchy
 * - Multi-stage render targets with texture outputs
 * - Technique/RenderPass routing for different shader effects
 * - Fullscreen quad rendering for post-processing
 * - Camera frustum setup with viewport management
 */
class CustomFrameGraph : public Qt3DRender::QFrameGraphNode
{
    Q_OBJECT

public:
    explicit CustomFrameGraph(Qt3DCore::QNode *parent = nullptr);
    ~CustomFrameGraph();

    /**
     * Initialize the frame graph with viewport size
     * @param viewportSize Target viewport dimensions (e.g., 1280x720)
     * @param camera Camera entity for rendering
     * @param rootEntity Root scene entity
     */
    void initialize(const QSize &viewportSize, Qt3DRender::QCamera *camera, Qt3DCore::QEntity *rootEntity);

    /**
     * Update viewport size (for window resize events)
     * @param newSize New viewport dimensions
     */
    void updateViewportSize(const QSize &newSize);

    /**
     * Check if frame graph is initialized
     * Claude Generated - For deferred initialization
     */
    bool isInitialized() const { return m_initialized; }

    /**
     * Get render target textures for shader access
     */
    Qt3DRender::QAbstractTexture *getColorTexture() const { return m_colorTexture.data(); }
    Qt3DRender::QAbstractTexture *getDepthTexture() const { return m_depthTexture.data(); }
    Qt3DRender::QAbstractTexture *getNormalTexture() const { return m_normalTexture.data(); }
    Qt3DRender::QAbstractTexture *getAOTexture() const { return m_aoTexture.data(); }

    /**
     * SSAO parameters
     */
    void setSSAOEnabled(bool enabled);
    bool isSSAOEnabled() const { return m_ssaoEnabled; }
    void setSSAOIntensity(float intensity);
    float getSSAOIntensity() const { return m_ssaoIntensity; }
    void setSSAORadius(float radius);
    float getSSAORadius() const { return m_ssaoRadius; }
    void setSSAOBias(float bias);
    float getSSAOBias() const { return m_ssaoBias; }

    /**
     * Bloom parameters (Phase 5B)
     */
    void setBloomEnabled(bool enabled);
    bool isBloomEnabled() const { return m_bloomEnabled; }
    void setBloomThreshold(float threshold);
    float getBloomThreshold() const { return m_bloomThreshold; }
    void setBloomIntensity(float intensity);
    float getBloomIntensity() const { return m_bloomIntensity; }

    /**
     * HDR/Tone mapping parameters (Phase 5B)
     */
    void setHDREnabled(bool enabled);
    bool isHDREnabled() const { return m_hdrEnabled; }
    void setExposure(float exposure);
    float getExposure() const { return m_exposure; }

    /**
     * Post-processing effect toggle
     */
    void setPostProcessingEnabled(bool enabled);
    bool isPostProcessingEnabled() const { return m_postProcessingEnabled; }

private:
    // Rendering parameters
    QSize m_viewportSize;
    bool m_initialized = false;

    // SSAO parameters
    bool m_ssaoEnabled = true;
    float m_ssaoIntensity = 1.0f;      // 0.0-2.0 range
    float m_ssaoRadius = 0.05f;         // AO sample radius
    float m_ssaoBias = 0.025f;          // Bias to prevent artifacts

    // Bloom parameters (Phase 5B)
    bool m_bloomEnabled = true;
    float m_bloomThreshold = 0.8f;      // Brightness threshold (0.5-1.5)
    float m_bloomIntensity = 1.0f;      // Bloom strength (0.0-2.0)

    // HDR parameters (Phase 5B)
    bool m_hdrEnabled = true;
    float m_exposure = 1.0f;            // Exposure compensation (0.5-3.0)

    // Post-processing toggle
    bool m_postProcessingEnabled = true;

    // G-buffer render targets and textures
    QSharedPointer<Qt3DRender::QRenderTarget> m_gBufferTarget;
    QSharedPointer<Qt3DRender::QAbstractTexture> m_colorTexture;      // RGBA16F - Scene color
    QSharedPointer<Qt3DRender::QAbstractTexture> m_depthTexture;      // Depth24Stencil8
    QSharedPointer<Qt3DRender::QAbstractTexture> m_normalTexture;     // RGB16F - World normals

    // Post-processing render targets
    QSharedPointer<Qt3DRender::QRenderTarget> m_ssaoTarget;
    QSharedPointer<Qt3DRender::QAbstractTexture> m_aoTexture;         // R16F - AO values

    QSharedPointer<Qt3DRender::QRenderTarget> m_aoBlurTarget;
    QSharedPointer<Qt3DRender::QAbstractTexture> m_aoBlurredTexture;  // R16F - Blurred AO

    // Bloom targets (Phase 5B)
    QSharedPointer<Qt3DRender::QRenderTarget> m_bloomBrightTarget;
    QSharedPointer<Qt3DRender::QAbstractTexture> m_bloomBrightTexture;

    QSharedPointer<Qt3DRender::QRenderTarget> m_bloomBlurTarget;
    QSharedPointer<Qt3DRender::QAbstractTexture> m_bloomBlurTexture;

    // Filter keys for pass selection
    QSharedPointer<Qt3DRender::QFilterKey> m_geometryPassKey;
    QSharedPointer<Qt3DRender::QFilterKey> m_ssaoPassKey;
    QSharedPointer<Qt3DRender::QFilterKey> m_bloomPassKey;
    QSharedPointer<Qt3DRender::QFilterKey> m_compositePassKey;

    // Helper methods
    void setupGBuffer();
    void setupSSAOTargets();
    void setupBloomTargets();      // Phase 5B
    void setupFrameGraphStructure(Qt3DRender::QCamera *camera, Qt3DCore::QEntity *rootEntity);
    Qt3DRender::QAbstractTexture *createTexture(Qt3DRender::QAbstractTexture::TextureFormat format);
    Qt3DRender::QRenderTargetOutput *createRenderTargetOutput(
        Qt3DRender::QAbstractTexture *texture,
        Qt3DRender::QRenderTargetOutput::AttachmentPoint attachmentPoint
    );
};

#endif // CUSTOMFRAMEGRAPH_H
