// fullscreenquad.h - Fullscreen quad entity for post-processing effects
// Claude Generated - Phase 5A: Helper for rendering fullscreen effects

#ifndef FULLSCREENQUAD_H
#define FULLSCREENQUAD_H

#include <Qt3DCore/QEntity>
#include <Qt3DExtras/QPlaneMesh>
#include <Qt3DRender/QMaterial>

/**
 * FullscreenQuad - A simple fullscreen quad for post-processing effects
 *
 * Creates a 2D quad that fills the viewport, used for:
 * - SSAO post-processing
 * - Bloom/blur passes
 * - HDR tone mapping
 * - Any other fullscreen effects
 *
 * The quad is created with normalized coordinates (-1 to 1) and no depth testing.
 */
class FullscreenQuad : public Qt3DCore::QEntity
{
    Q_OBJECT

public:
    explicit FullscreenQuad(Qt3DCore::QEntity *parent = nullptr);
    ~FullscreenQuad();

    /**
     * Set the material for this quad
     * @param material The Qt3D material to use for rendering
     */
    void setMaterial(Qt3DRender::QMaterial *material);

    /**
     * Get the material currently applied
     */
    Qt3DRender::QMaterial *getMaterial() const { return m_material; }

private:
    void setupGeometry();

    Qt3DExtras::QPlaneMesh *m_mesh = nullptr;
    Qt3DRender::QMaterial *m_material = nullptr;
};

#endif // FULLSCREENQUAD_H
