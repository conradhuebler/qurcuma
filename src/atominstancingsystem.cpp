// Claude Generated - Phase 3B - GPU Instancing System Implementation
#include "atominstancingsystem.h"
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QBuffer>
#include <QDebug>
#include <cmath>

AtomInstancingSystem::AtomInstancingSystem(Qt3DCore::QEntity *rootEntity, QObject *parent)
    : QObject(parent)
    , m_rootEntity(rootEntity)
{
    // Create the main instanced entity
    m_instancedEntity = new Qt3DCore::QEntity(rootEntity);
    m_instancedEntity->setObjectName("AtomInstancingEntity");
}

AtomInstancingSystem::~AtomInstancingSystem()
{
}

void AtomInstancingSystem::setAtoms(const QVector<QVector3D>& positions,
                                     const QVector<QString>& elements,
                                     const QVector<QColor>& colors,
                                     const QVector<float>& scales)
{
    if (positions.isEmpty()) {
        m_atomInstances.clear();
        return;
    }

    // Create atom instances from data
    m_atomInstances.clear();
    for (int i = 0; i < positions.size(); ++i) {
        AtomInstance instance;
        instance.position = positions[i];
        instance.scale = (i < scales.size()) ? scales[i] : 1.0f;
        instance.color = (i < colors.size()) ? colors[i] : QColor(200, 200, 200);
        instance.atomIndex = i;
        m_atomInstances.append(instance);
    }

    createGeometry();
    setupMaterial();

    emit atomCountChanged(m_atomInstances.size());
    emit renderingModeChanged(true);  // Instancing mode
}

void AtomInstancingSystem::createGeometry()
{
    // Claude Generated - Phase 3B: Create base sphere mesh (shared for all atoms)
    // This is rendered once per frame with instance data

    if (!m_geometry) {
        m_geometry = new Qt3DCore::QGeometry();
    }

    // Create vertex buffer with basic sphere data
    // For simplicity, we'll use a simple icosphere or uv-sphere representation
    // Each atom instance will transform this sphere via instance attributes

    Qt3DRender::QBuffer *vertexDataBuffer = new Qt3DRender::QBuffer();

    // Create simple vertex data for a unit sphere (simplified - 8 vertices of a cube for testing)
    // In production, this would be a full sphere mesh
    float sphereVertices[] = {
        // Positions (simplified cube approximating a sphere)
        -0.5f, -0.5f, -0.5f,
        0.5f, -0.5f, -0.5f,
        0.5f, 0.5f, -0.5f,
        -0.5f, 0.5f, -0.5f,
        -0.5f, -0.5f, 0.5f,
        0.5f, -0.5f, 0.5f,
        0.5f, 0.5f, 0.5f,
        -0.5f, 0.5f, 0.5f,
    };

    QByteArray vertexData;
    vertexData.resize(sizeof(sphereVertices));
    memcpy(vertexData.data(), sphereVertices, sizeof(sphereVertices));

    vertexDataBuffer->setData(vertexData);

    // Position attribute
    Qt3DRender::QAttribute *positionAttribute = new Qt3DRender::QAttribute();
    positionAttribute->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());
    positionAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
    positionAttribute->setVertexSize(3);
    positionAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
    positionAttribute->setBuffer(vertexDataBuffer);
    positionAttribute->setByteStride(3 * sizeof(float));
    positionAttribute->setCount(8);

    m_geometry->addAttribute(positionAttribute);

    // Create instance data buffer (positions, colors, scales)
    Qt3DRender::QBuffer *instanceDataBuffer = new Qt3DRender::QBuffer();
    updateInstanceBuffer();
}

void AtomInstancingSystem::createInstanceBuffer()
{
    // Instance data structure: position (vec3) + scale (float) + color (vec4)
    // Total: 32 bytes per instance
}

void AtomInstancingSystem::updateInstanceBuffer()
{
    // This would update the GPU buffer with new instance data
    // Called when atoms move or change appearance
}

void AtomInstancingSystem::setupMaterial()
{
    // Claude Generated - Phase 3B: Create phong material for instanced rendering
    Qt3DExtras::QPhongMaterial *material = new Qt3DExtras::QPhongMaterial();
    material->setAmbient(QColor(100, 100, 100));
    material->setDiffuse(QColor(200, 200, 200));
    material->setShininess(80);

    m_instancedEntity->addComponent(material);
}

void AtomInstancingSystem::updateAtomPositions(const QVector<QVector3D>& positions)
{
    if (positions.size() != m_atomInstances.size()) {
        return;
    }

    for (int i = 0; i < positions.size(); ++i) {
        m_atomInstances[i].position = positions[i];
    }

    updateInstanceBuffer();
}

void AtomInstancingSystem::updateAtomColors(const QVector<QColor>& colors)
{
    if (colors.size() != m_atomInstances.size()) {
        return;
    }

    for (int i = 0; i < colors.size(); ++i) {
        m_atomInstances[i].color = colors[i];
    }

    updateInstanceBuffer();
}

void AtomInstancingSystem::updateAtomScales(const QVector<float>& scales)
{
    if (scales.size() != m_atomInstances.size()) {
        return;
    }

    for (int i = 0; i < scales.size(); ++i) {
        m_atomInstances[i].scale = scales[i];
    }

    updateInstanceBuffer();
}

int AtomInstancingSystem::raycastAtom(const QVector3D& rayOrigin,
                                      const QVector3D& rayDirection,
                                      const QVector<QVector3D>& atomPositions,
                                      float pickingRadius) const
{
    // Claude Generated - Phase 3B: Custom ray-casting for instanced atom picking
    // Since ObjectPicker doesn't work with instancing, we manually cast rays

    int closestAtom = -1;
    float closestDistance = std::numeric_limits<float>::max();

    for (int i = 0; i < atomPositions.size(); ++i) {
        QVector3D atomPos = atomPositions[i];

        // Vector from ray origin to atom
        QVector3D toAtom = atomPos - rayOrigin;

        // Project onto ray direction
        float t = QVector3D::dotProduct(toAtom, rayDirection);

        if (t < 0) continue;  // Atom is behind ray origin

        // Closest point on ray to atom
        QVector3D closestPoint = rayOrigin + rayDirection * t;

        // Distance from atom to ray
        float distance = (atomPos - closestPoint).length();

        // Check if within picking radius (scaled by atom size)
        float scaledRadius = pickingRadius;
        if (i < m_atomInstances.size()) {
            scaledRadius *= m_atomInstances[i].scale;
        }

        if (distance <= scaledRadius && t < closestDistance) {
            closestDistance = t;
            closestAtom = i;
        }
    }

    return closestAtom;
}

void AtomInstancingSystem::setVisible(bool visible)
{
    if (m_isVisible != visible) {
        m_isVisible = visible;
        if (m_instancedEntity) {
            m_instancedEntity->setEnabled(visible);
        }
    }
}
