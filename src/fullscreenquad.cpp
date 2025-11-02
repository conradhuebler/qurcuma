// fullscreenquad.cpp - Fullscreen quad entity implementation
// Claude Generated - Phase 5A: Helper for rendering fullscreen effects

#include "fullscreenquad.h"
#include <Qt3DCore/QTransform>

FullscreenQuad::FullscreenQuad(Qt3DCore::QEntity *parent)
    : Qt3DCore::QEntity(parent)
    , m_material(nullptr)
{
    // Create and configure plane mesh (2x2 units, centered at origin)
    m_mesh = new Qt3DExtras::QPlaneMesh(this);
    m_mesh->setWidth(2.0f);   // Full width (-1 to 1)
    m_mesh->setHeight(2.0f);  // Full height (-1 to 1)

    // Add mesh to entity
    addComponent(m_mesh);

    // Create transform (identity - no transformation needed)
    auto *transform = new Qt3DCore::QTransform(this);
    addComponent(transform);
}

FullscreenQuad::~FullscreenQuad()
{
}

void FullscreenQuad::setMaterial(Qt3DRender::QMaterial *material)
{
    if (m_material) {
        removeComponent(m_material);
    }

    m_material = material;

    if (m_material) {
        addComponent(m_material);
    }
}
