// customframegraph.cpp - Multi-pass rendering FrameGraph implementation
// Claude Generated - Phase 5A: Custom FrameGraph with SSAO, bloom, and HDR support

#include "customframegraph.h"
#include <Qt3DRender/QRenderSurfaceSelector>
#include <Qt3DRender/QViewport>
#include <Qt3DRender/QCameraSelector>
#include <Qt3DRender/QLayerFilter>
#include <Qt3DRender/QTechniqueFilter>
#include <Qt3DRender/QRenderPassFilter>
#include <Qt3DRender/QClearBuffers>
#include <Qt3DRender/QBlitFramebuffer>
#include <Qt3DRender/QNoDraw>
#include <Qt3DRender/QTexture>
#include <Qt3DRender/QTextureImage>
#include <Qt3DRender/QRenderTargetSelector>
#include <Qt3DRender/QFilterKey>
#include <Qt3DRender/QRenderStateSet>
#include <Qt3DRender/QCullFace>
#include <Qt3DRender/QColorMask>
#include <Qt3DRender/QDepthTest>
#include <Qt3DRender/QBlendEquation>
#include <Qt3DRender/QBlendEquationArguments>
#include <Qt3DRender/QMemoryBarrier>
#include <QSurfaceFormat>

CustomFrameGraph::CustomFrameGraph(Qt3DCore::QNode *parent)
    : Qt3DRender::QFrameGraphNode(parent)
{
    // Initialize with default sizes - will be set properly in initialize()
    m_viewportSize = QSize(1280, 720);
}

CustomFrameGraph::~CustomFrameGraph()
{
}

void CustomFrameGraph::initialize(const QSize &viewportSize, Qt3DRender::QCamera *camera, Qt3DCore::QEntity *rootEntity)
{
    if (m_initialized) return;

    m_viewportSize = viewportSize;

    // Setup render targets and textures
    setupGBuffer();
    setupSSAOTargets();
    setupBloomTargets();  // Phase 5B setup

    // Setup the frame graph structure
    setupFrameGraphStructure(camera, rootEntity);

    m_initialized = true;
}

void CustomFrameGraph::updateViewportSize(const QSize &newSize)
{
    if (m_viewportSize == newSize) return;

    m_viewportSize = newSize;

    // Update texture sizes if needed - can add resizing logic here
    // For now, textures are created at initialization size
}

void CustomFrameGraph::setupGBuffer()
{
    // Create G-buffer render target with color, depth, and normal outputs
    m_gBufferTarget = QSharedPointer<Qt3DRender::QRenderTarget>::create();

    // Color texture (RGBA16F for HDR)
    m_colorTexture = QSharedPointer<Qt3DRender::QAbstractTexture>(
        createTexture(Qt3DRender::QAbstractTexture::RGBA16F)
    );

    // Depth texture (D24S8)
    m_depthTexture = QSharedPointer<Qt3DRender::QAbstractTexture>(
        createTexture(Qt3DRender::QAbstractTexture::D24S8)
    );

    // Normal texture (RGB16F)
    m_normalTexture = QSharedPointer<Qt3DRender::QAbstractTexture>(
        createTexture(Qt3DRender::QAbstractTexture::RGB16F)
    );

    // Attach to render target
    m_gBufferTarget->addOutput(
        createRenderTargetOutput(m_colorTexture.data(),
                               Qt3DRender::QRenderTargetOutput::Color0)
    );
    m_gBufferTarget->addOutput(
        createRenderTargetOutput(m_depthTexture.data(),
                               Qt3DRender::QRenderTargetOutput::DepthStencil)
    );
    m_gBufferTarget->addOutput(
        createRenderTargetOutput(m_normalTexture.data(),
                               Qt3DRender::QRenderTargetOutput::Color1)
    );
}

void CustomFrameGraph::setupSSAOTargets()
{
    // AO texture for SSAO pass (R16F)
    m_aoTexture = QSharedPointer<Qt3DRender::QAbstractTexture>(
        createTexture(Qt3DRender::QAbstractTexture::R16F)
    );

    m_ssaoTarget = QSharedPointer<Qt3DRender::QRenderTarget>::create();
    m_ssaoTarget->addOutput(
        createRenderTargetOutput(m_aoTexture.data(),
                               Qt3DRender::QRenderTargetOutput::Color0)
    );

    // Blurred AO texture (R16F)
    m_aoBlurredTexture = QSharedPointer<Qt3DRender::QAbstractTexture>(
        createTexture(Qt3DRender::QAbstractTexture::R16F)
    );

    m_aoBlurTarget = QSharedPointer<Qt3DRender::QRenderTarget>::create();
    m_aoBlurTarget->addOutput(
        createRenderTargetOutput(m_aoBlurredTexture.data(),
                               Qt3DRender::QRenderTargetOutput::Color0)
    );
}

void CustomFrameGraph::setupBloomTargets()
{
    // Bloom bright pass texture (RGBA16F)
    m_bloomBrightTexture = QSharedPointer<Qt3DRender::QAbstractTexture>(
        createTexture(Qt3DRender::QAbstractTexture::RGBA16F)
    );

    m_bloomBrightTarget = QSharedPointer<Qt3DRender::QRenderTarget>::create();
    m_bloomBrightTarget->addOutput(
        createRenderTargetOutput(m_bloomBrightTexture.data(),
                               Qt3DRender::QRenderTargetOutput::Color0)
    );

    // Bloom blur texture (RGBA16F)
    m_bloomBlurTexture = QSharedPointer<Qt3DRender::QAbstractTexture>(
        createTexture(Qt3DRender::QAbstractTexture::RGBA16F)
    );

    m_bloomBlurTarget = QSharedPointer<Qt3DRender::QRenderTarget>::create();
    m_bloomBlurTarget->addOutput(
        createRenderTargetOutput(m_bloomBlurTexture.data(),
                               Qt3DRender::QRenderTargetOutput::Color0)
    );
}

void CustomFrameGraph::setupFrameGraphStructure(Qt3DRender::QCamera *camera, Qt3DCore::QEntity *rootEntity)
{
    // Create filter keys for pass selection
    m_geometryPassKey = QSharedPointer<Qt3DRender::QFilterKey>::create();
    m_geometryPassKey->setName(QStringLiteral("pass"));
    m_geometryPassKey->setValue(QStringLiteral("geometry"));

    m_ssaoPassKey = QSharedPointer<Qt3DRender::QFilterKey>::create();
    m_ssaoPassKey->setName(QStringLiteral("pass"));
    m_ssaoPassKey->setValue(QStringLiteral("ssao"));

    m_bloomPassKey = QSharedPointer<Qt3DRender::QFilterKey>::create();
    m_bloomPassKey->setName(QStringLiteral("pass"));
    m_bloomPassKey->setValue(QStringLiteral("bloom"));

    m_compositePassKey = QSharedPointer<Qt3DRender::QFilterKey>::create();
    m_compositePassKey->setName(QStringLiteral("pass"));
    m_compositePassKey->setValue(QStringLiteral("composite"));

    // Create render surface selector (top-level node)
    auto *surfaceSelector = new Qt3DRender::QRenderSurfaceSelector(this);

    // Create viewport
    auto *viewport = new Qt3DRender::QViewport(surfaceSelector);
    viewport->setNormalizedRect(QRectF(0.0f, 0.0f, 1.0f, 1.0f));

    // Create camera selector
    auto *cameraSelector = new Qt3DRender::QCameraSelector(viewport);
    cameraSelector->setCamera(camera);

    // =====================================================
    // PASS 1: GEOMETRY PASS - Render scene to G-buffer
    // =====================================================
    auto *gBufferSelector = new Qt3DRender::QRenderTargetSelector(cameraSelector);
    gBufferSelector->setTarget(m_gBufferTarget.data());

    // Clear G-buffer
    auto *clearGBuffer = new Qt3DRender::QClearBuffers(gBufferSelector);
    clearGBuffer->setBuffers(Qt3DRender::QClearBuffers::ColorDepthBuffer);
    clearGBuffer->setClearColor(QColor(0, 0, 0, 1));

    // Filter for geometry pass - render normal geometry
    auto *geometryFilter = new Qt3DRender::QTechniqueFilter(clearGBuffer);
    geometryFilter->addMatch(m_geometryPassKey.data());

    // =====================================================
    // PASS 2: SSAO PASS - Compute ambient occlusion
    // =====================================================
    auto *ssaoTargetSelector = new Qt3DRender::QRenderTargetSelector(cameraSelector);
    ssaoTargetSelector->setTarget(m_ssaoTarget.data());

    auto *clearSSAO = new Qt3DRender::QClearBuffers(ssaoTargetSelector);
    clearSSAO->setBuffers(Qt3DRender::QClearBuffers::ColorBuffer);
    clearSSAO->setClearColor(QColor(255, 255, 255, 255));  // White = full brightness (no occlusion)

    auto *ssaoFilter = new Qt3DRender::QTechniqueFilter(clearSSAO);
    ssaoFilter->addMatch(m_ssaoPassKey.data());

    // Render state for SSAO - disable depth test and writing
    auto *ssaoRenderState = new Qt3DRender::QRenderStateSet(ssaoFilter);
    auto *ssaoDepthTest = new Qt3DRender::QDepthTest(ssaoRenderState);
    ssaoDepthTest->setDepthFunction(Qt3DRender::QDepthTest::Always);
    ssaoRenderState->addRenderState(ssaoDepthTest);

    // =====================================================
    // PASS 3: SSAO BLUR PASS - Smooth AO texture
    // =====================================================
    auto *blurTargetSelector = new Qt3DRender::QRenderTargetSelector(cameraSelector);
    blurTargetSelector->setTarget(m_aoBlurTarget.data());

    auto *clearBlur = new Qt3DRender::QClearBuffers(blurTargetSelector);
    clearBlur->setBuffers(Qt3DRender::QClearBuffers::ColorBuffer);
    clearBlur->setClearColor(QColor(255, 255, 255, 255));

    auto *blurFilter = new Qt3DRender::QTechniqueFilter(clearBlur);
    auto *blurPassKey = new Qt3DRender::QFilterKey(blurFilter);
    blurPassKey->setName(QStringLiteral("pass"));
    blurPassKey->setValue(QStringLiteral("ssaoBlur"));
    blurFilter->addMatch(blurPassKey);

    auto *blurRenderState = new Qt3DRender::QRenderStateSet(blurFilter);
    auto *blurDepthTest = new Qt3DRender::QDepthTest(blurRenderState);
    blurDepthTest->setDepthFunction(Qt3DRender::QDepthTest::Always);
    blurRenderState->addRenderState(blurDepthTest);

    // =====================================================
    // PASS 4: COMPOSITE PASS - Final output to screen
    // =====================================================
    // Note: No explicit render target selector means render to screen
    auto *clearScreen = new Qt3DRender::QClearBuffers(cameraSelector);
    clearScreen->setBuffers(Qt3DRender::QClearBuffers::ColorDepthBuffer);
    clearScreen->setClearColor(QColor(0, 0, 0, 1));

    auto *compositeFilter = new Qt3DRender::QTechniqueFilter(clearScreen);
    compositeFilter->addMatch(m_compositePassKey.data());

    // Render state for composite - normal depth testing
    auto *compositeRenderState = new Qt3DRender::QRenderStateSet(compositeFilter);
    auto *compositeDepthTest = new Qt3DRender::QDepthTest(compositeRenderState);
    compositeDepthTest->setDepthFunction(Qt3DRender::QDepthTest::Less);
    compositeRenderState->addRenderState(compositeDepthTest);
}

Qt3DRender::QAbstractTexture *CustomFrameGraph::createTexture(Qt3DRender::QAbstractTexture::TextureFormat format)
{
    auto *texture = new Qt3DRender::QTexture2D();
    texture->setFormat(format);
    texture->setWidth(m_viewportSize.width());
    texture->setHeight(m_viewportSize.height());
    texture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
    texture->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
    texture->setWrapMode(Qt3DRender::QTextureWrapMode(Qt3DRender::QTextureWrapMode::ClampToEdge));
    texture->setGenerateMipMaps(false);

    return texture;
}

Qt3DRender::QRenderTargetOutput *CustomFrameGraph::createRenderTargetOutput(
    Qt3DRender::QAbstractTexture *texture,
    Qt3DRender::QRenderTargetOutput::AttachmentPoint attachmentPoint
)
{
    auto *output = new Qt3DRender::QRenderTargetOutput();
    output->setAttachmentPoint(attachmentPoint);
    output->setTexture(texture);

    return output;
}

// =====================================================
// SSAO Parameter Setters
// =====================================================

void CustomFrameGraph::setSSAOEnabled(bool enabled)
{
    if (m_ssaoEnabled != enabled) {
        m_ssaoEnabled = enabled;
        // TODO: Update shader uniforms
    }
}

void CustomFrameGraph::setSSAOIntensity(float intensity)
{
    m_ssaoIntensity = qBound(0.0f, intensity, 2.0f);
    // TODO: Update shader uniform
}

void CustomFrameGraph::setSSAORadius(float radius)
{
    m_ssaoRadius = qBound(0.01f, radius, 0.2f);
    // TODO: Update shader uniform
}

void CustomFrameGraph::setSSAOBias(float bias)
{
    m_ssaoBias = qBound(0.0f, bias, 0.1f);
    // TODO: Update shader uniform
}

// =====================================================
// Bloom Parameter Setters (Phase 5B)
// =====================================================

void CustomFrameGraph::setBloomEnabled(bool enabled)
{
    if (m_bloomEnabled != enabled) {
        m_bloomEnabled = enabled;
        // TODO: Update shader uniforms
    }
}

void CustomFrameGraph::setBloomThreshold(float threshold)
{
    m_bloomThreshold = qBound(0.5f, threshold, 1.5f);
    // TODO: Update shader uniform
}

void CustomFrameGraph::setBloomIntensity(float intensity)
{
    m_bloomIntensity = qBound(0.0f, intensity, 2.0f);
    // TODO: Update shader uniform
}

// =====================================================
// HDR Parameter Setters (Phase 5B)
// =====================================================

void CustomFrameGraph::setHDREnabled(bool enabled)
{
    if (m_hdrEnabled != enabled) {
        m_hdrEnabled = enabled;
        // TODO: Update shader uniforms
    }
}

void CustomFrameGraph::setExposure(float exposure)
{
    m_exposure = qBound(0.5f, exposure, 3.0f);
    // TODO: Update shader uniform
}

// =====================================================
// Post-Processing Toggle
// =====================================================

void CustomFrameGraph::setPostProcessingEnabled(bool enabled)
{
    if (m_postProcessingEnabled != enabled) {
        m_postProcessingEnabled = enabled;
        // TODO: Toggle post-processing passes on/off
    }
}
