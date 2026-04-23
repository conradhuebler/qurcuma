// Claude Generated - Phase 5D - GPU Instancing System Implementation
#include "atominstancingsystem.h"
#include <Qt3DCore/QTransform>
#include <Qt3DCore/QGeometry>
#include <Qt3DCore/QAttribute>
#include <Qt3DCore/QBuffer>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QFilterKey>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QGraphicsApiFilter>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QRenderPass>
#include <Qt3DRender/QShaderProgram>
#include <Qt3DRender/QTechnique>
#include <QDebug>
#include <QUrl>
#include <cmath>
#include <limits>

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
    createInstanceBuffer();
    setupMaterial();

    emit atomCountChanged(m_atomInstances.size());
    emit renderingModeChanged(true);  // Instancing mode
}

// Generate UV sphere vertices with configurable rings and slices
static void generateSphereMesh(int rings, int slices, QVector<QVector3D>& vertices, QVector<QVector3D>& normals, QVector<uint>& indices)
{
    vertices.clear();
    normals.clear();
    indices.clear();

    // Generate sphere vertices (UV sphere)
    for (int ring = 0; ring <= rings; ++ring) {
        float theta = (float)ring / rings * M_PI;  // 0 to π
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);

        for (int slice = 0; slice <= slices; ++slice) {
            float phi = (float)slice / slices * 2.0f * M_PI;  // 0 to 2π
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            // Position on unit sphere
            QVector3D pos(sinTheta * cosPhi, cosTheta, sinTheta * sinPhi);
            vertices.append(pos);
            normals.append(pos);  // For sphere, normal == position (unit sphere)
        }
    }

    // Generate indices for triangle strips
    for (int ring = 0; ring < rings; ++ring) {
        for (int slice = 0; slice < slices; ++slice) {
            uint a = ring * (slices + 1) + slice;
            uint b = a + 1;
            uint c = a + (slices + 1);
            uint d = c + 1;

            // First triangle
            indices.append(a);
            indices.append(b);
            indices.append(c);

            // Second triangle
            indices.append(b);
            indices.append(d);
            indices.append(c);
        }
    }
}

void AtomInstancingSystem::createGeometry()
{
    // Claude Generated - Phase 5D: Create base sphere mesh (shared for all atoms)
    // This is rendered once per frame with instance data via Qt3D instancing

    if (m_geometry) {
        delete m_geometry;
    }
    m_geometry = new Qt3DCore::QGeometry();

    // Generate sphere mesh with 16 rings/slices (Balanced quality)
    // Can be made configurable via PerformanceOptimizer in future
    QVector<QVector3D> sphereVertices, sphereNormals;
    QVector<uint> sphereIndices;
    generateSphereMesh(16, 16, sphereVertices, sphereNormals, sphereIndices);

    // Create vertex buffer with sphere positions
    Qt3DCore::QBuffer *vertexDataBuffer = new Qt3DCore::QBuffer(m_geometry);
    QByteArray vertexData;
    vertexData.resize(sphereVertices.size() * sizeof(QVector3D));
    memcpy(vertexData.data(), sphereVertices.data(), vertexData.size());
    vertexDataBuffer->setData(vertexData);

    // Position attribute
    Qt3DCore::QAttribute *positionAttribute = new Qt3DCore::QAttribute();
    positionAttribute->setName(Qt3DCore::QAttribute::defaultPositionAttributeName());
    positionAttribute->setVertexBaseType(Qt3DCore::QAttribute::Float);
    positionAttribute->setVertexSize(3);
    positionAttribute->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
    positionAttribute->setBuffer(vertexDataBuffer);
    positionAttribute->setByteStride(sizeof(QVector3D));
    positionAttribute->setByteOffset(0);
    positionAttribute->setCount(sphereVertices.size());
    m_geometry->addAttribute(positionAttribute);

    // Create normal buffer
    Qt3DCore::QBuffer *normalDataBuffer = new Qt3DCore::QBuffer(m_geometry);
    QByteArray normalData;
    normalData.resize(sphereNormals.size() * sizeof(QVector3D));
    memcpy(normalData.data(), sphereNormals.data(), normalData.size());
    normalDataBuffer->setData(normalData);

    // Normal attribute
    Qt3DCore::QAttribute *normalAttribute = new Qt3DCore::QAttribute();
    normalAttribute->setName(Qt3DCore::QAttribute::defaultNormalAttributeName());
    normalAttribute->setVertexBaseType(Qt3DCore::QAttribute::Float);
    normalAttribute->setVertexSize(3);
    normalAttribute->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
    normalAttribute->setBuffer(normalDataBuffer);
    normalAttribute->setByteStride(sizeof(QVector3D));
    normalAttribute->setByteOffset(0);
    normalAttribute->setCount(sphereNormals.size());
    m_geometry->addAttribute(normalAttribute);

    // Create index buffer
    Qt3DCore::QBuffer *indexBuffer = new Qt3DCore::QBuffer(m_geometry);
    QByteArray indexData;
    indexData.resize(sphereIndices.size() * sizeof(uint));
    memcpy(indexData.data(), sphereIndices.data(), indexData.size());
    indexBuffer->setData(indexData);

    // Index attribute — do NOT reuse position-attribute name; Qt3D IndexAttribute
    // is identified by attribute type, and a duplicate name confuses attribute lookup.
    Qt3DCore::QAttribute *indexAttribute = new Qt3DCore::QAttribute();
    indexAttribute->setVertexBaseType(Qt3DCore::QAttribute::UnsignedInt);
    indexAttribute->setAttributeType(Qt3DCore::QAttribute::IndexAttribute);
    indexAttribute->setBuffer(indexBuffer);
    indexAttribute->setCount(sphereIndices.size());
    m_geometry->addAttribute(indexAttribute);
}

void AtomInstancingSystem::createInstanceBuffer()
{
    // Claude Generated - Phase 3.1: Build buffer + attributes ONCE; later updates
    // just re-upload data via setData on the cached buffer (no attribute recreation).
    // Layout per instance: position (vec3) + scale (float) + color (vec4) = 32 bytes.

    if (m_atomInstances.isEmpty()) {
        return;
    }

    m_instanceBuffer = new Qt3DCore::QBuffer(m_geometry);
    updateInstanceBuffer();  // Uploads data

    auto *instancePositionAttribute = new Qt3DCore::QAttribute();
    instancePositionAttribute->setName("instancePosition");
    instancePositionAttribute->setVertexBaseType(Qt3DCore::QAttribute::Float);
    instancePositionAttribute->setVertexSize(3);
    instancePositionAttribute->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
    instancePositionAttribute->setBuffer(m_instanceBuffer);
    instancePositionAttribute->setByteStride(8 * sizeof(float));
    instancePositionAttribute->setByteOffset(0);
    instancePositionAttribute->setCount(m_atomInstances.size());
    instancePositionAttribute->setDivisor(1);
    m_geometry->addAttribute(instancePositionAttribute);

    auto *instanceScaleAttribute = new Qt3DCore::QAttribute();
    instanceScaleAttribute->setName("instanceScale");
    instanceScaleAttribute->setVertexBaseType(Qt3DCore::QAttribute::Float);
    instanceScaleAttribute->setVertexSize(1);
    instanceScaleAttribute->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
    instanceScaleAttribute->setBuffer(m_instanceBuffer);
    instanceScaleAttribute->setByteStride(8 * sizeof(float));
    instanceScaleAttribute->setByteOffset(3 * sizeof(float));
    instanceScaleAttribute->setCount(m_atomInstances.size());
    instanceScaleAttribute->setDivisor(1);
    m_geometry->addAttribute(instanceScaleAttribute);

    auto *instanceColorAttribute = new Qt3DCore::QAttribute();
    instanceColorAttribute->setName("instanceColor");
    instanceColorAttribute->setVertexBaseType(Qt3DCore::QAttribute::Float);
    instanceColorAttribute->setVertexSize(4);
    instanceColorAttribute->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
    instanceColorAttribute->setBuffer(m_instanceBuffer);
    instanceColorAttribute->setByteStride(8 * sizeof(float));
    instanceColorAttribute->setByteOffset(4 * sizeof(float));
    instanceColorAttribute->setCount(m_atomInstances.size());
    instanceColorAttribute->setDivisor(1);
    m_geometry->addAttribute(instanceColorAttribute);

    if (!m_geometryRenderer) {
        m_geometryRenderer = new Qt3DRender::QGeometryRenderer(m_instancedEntity);
        m_instancedEntity->addComponent(m_geometryRenderer);
    }
    m_geometryRenderer->setGeometry(m_geometry);
    m_geometryRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);
    m_geometryRenderer->setInstanceCount(m_atomInstances.size());
}

void AtomInstancingSystem::updateInstanceBuffer()
{
    // Claude Generated - Phase 3.1: Hot path for per-frame updates.
    // Re-serialize m_atomInstances to byte array, upload via QBuffer::setData.
    // No attribute or renderer churn.

    if (m_atomInstances.isEmpty() || !m_instanceBuffer) {
        return;
    }

    QByteArray instanceData;
    instanceData.resize(m_atomInstances.size() * sizeof(float) * 8);
    float *dataPtr = reinterpret_cast<float*>(instanceData.data());
    for (int i = 0; i < m_atomInstances.size(); ++i) {
        const AtomInstance& inst = m_atomInstances[i];
        dataPtr[i * 8 + 0] = inst.position.x();
        dataPtr[i * 8 + 1] = inst.position.y();
        dataPtr[i * 8 + 2] = inst.position.z();
        dataPtr[i * 8 + 3] = inst.scale;
        dataPtr[i * 8 + 4] = inst.color.redF();
        dataPtr[i * 8 + 5] = inst.color.greenF();
        dataPtr[i * 8 + 6] = inst.color.blueF();
        dataPtr[i * 8 + 7] = inst.color.alphaF();
    }
    m_instanceBuffer->setData(instanceData);
}

void AtomInstancingSystem::setupMaterial()
{
    // Claude Generated - Phase 3.1: Custom QMaterial with instancing-aware shader.
    // Default QPhongMaterial's shader ignores instance* attributes; must bind our own.

    auto *material = new Qt3DRender::QMaterial(m_instancedEntity);
    auto *effect = new Qt3DRender::QEffect(material);
    auto *technique = new Qt3DRender::QTechnique(effect);

    technique->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGL);
    technique->graphicsApiFilter()->setMajorVersion(3);
    technique->graphicsApiFilter()->setMinorVersion(3);
    technique->graphicsApiFilter()->setProfile(Qt3DRender::QGraphicsApiFilter::CoreProfile);

    auto *filterKey = new Qt3DRender::QFilterKey(technique);
    filterKey->setName(QStringLiteral("renderingStyle"));
    filterKey->setValue(QStringLiteral("forward"));
    technique->addFilterKey(filterKey);

    auto *renderPass = new Qt3DRender::QRenderPass(technique);
    auto *program = new Qt3DRender::QShaderProgram(renderPass);
    program->setVertexShaderCode(Qt3DRender::QShaderProgram::loadSource(
        QUrl(QStringLiteral("qrc:/shaders/src/shaders/atom_instanced.vert"))));
    program->setFragmentShaderCode(Qt3DRender::QShaderProgram::loadSource(
        QUrl(QStringLiteral("qrc:/shaders/src/shaders/atom_instanced.frag"))));
    connect(program, &Qt3DRender::QShaderProgram::logChanged, this, [program]() {
        const QString log = program->log();
        if (!log.isEmpty())
            qWarning() << "[atom_instanced shader]" << program->status() << log;
    });
    renderPass->setShaderProgram(program);

    technique->addRenderPass(renderPass);
    effect->addTechnique(technique);

    m_cameraPosParam = new Qt3DRender::QParameter("cameraPosition", QVector3D(0, 0, 100), material);
    material->addParameter(m_cameraPosParam);

    // Claude Generated 2026 - Phase 1 Fog parameters
    m_fogEnabledParam = new Qt3DRender::QParameter("fogEnabled", 0.0f, material);
    m_fogColorParam = new Qt3DRender::QParameter("fogColor", QVector3D(0.125f, 0.141f, 0.172f), material);
    m_fogDensityParam = new Qt3DRender::QParameter("fogDensity", 0.015f, material);
    material->addParameter(m_fogEnabledParam);
    material->addParameter(m_fogColorParam);
    material->addParameter(m_fogDensityParam);

    material->setEffect(effect);

    m_instancedEntity->addComponent(material);
}

void AtomInstancingSystem::setCameraPosition(const QVector3D &pos)
{
    if (m_cameraPosParam)
        m_cameraPosParam->setValue(pos);
}

// Claude Generated 2026 - Phase 1 Fog setters
void AtomInstancingSystem::setFogEnabled(bool enabled)
{
    if (m_fogEnabledParam)
        m_fogEnabledParam->setValue(enabled ? 1.0f : 0.0f);
}

void AtomInstancingSystem::setFogColor(const QColor &color)
{
    if (m_fogColorParam)
        m_fogColorParam->setValue(QVector3D(color.redF(), color.greenF(), color.blueF()));
}

void AtomInstancingSystem::setFogDensity(float density)
{
    if (m_fogDensityParam)
        m_fogDensityParam->setValue(density);
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
    // Claude Generated - Phase 5D: Custom ray-casting for instanced atom picking
    // Since ObjectPicker doesn't work with instancing, we manually cast rays
    // Ray-sphere intersection test for each atom

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
