// Claude Generated - Phase 3.2: GPU instancing for bonds.
#include "bondinstancingsystem.h"

#include <Qt3DCore/QAttribute>
#include <Qt3DCore/QBuffer>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QFilterKey>
#include <Qt3DRender/QGraphicsApiFilter>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QRenderPass>
#include <Qt3DRender/QShaderProgram>
#include <Qt3DRender/QTechnique>

#include <QByteArray>
#include <QUrl>
#include <QtMath>

namespace {

// Generate base cylinder: axis = Y, y in [-1, +1], radius = 1.
// Sides + 2 caps. Slices controls rotational tessellation.
void generateCylinderMesh(int slices,
                          QVector<QVector3D>& vertices,
                          QVector<QVector3D>& normals,
                          QVector<uint>& indices)
{
    vertices.clear();
    normals.clear();
    indices.clear();

    // Side wall: two rings of vertices at y=-1 and y=+1.
    for (int i = 0; i <= slices; ++i) {
        float phi = (float)i / slices * 2.0f * float(M_PI);
        float cx = std::cos(phi);
        float cz = std::sin(phi);
        vertices.append(QVector3D(cx, -1.0f, cz));
        normals.append(QVector3D(cx, 0.0f, cz));
        vertices.append(QVector3D(cx, +1.0f, cz));
        normals.append(QVector3D(cx, 0.0f, cz));
    }
    for (int i = 0; i < slices; ++i) {
        uint a = i * 2;
        uint b = a + 1;
        uint c = a + 2;
        uint d = a + 3;
        indices.append(a); indices.append(c); indices.append(b);
        indices.append(b); indices.append(c); indices.append(d);
    }

    // Top cap (y=+1).
    uint topCenterIdx = vertices.size();
    vertices.append(QVector3D(0.0f, +1.0f, 0.0f));
    normals.append(QVector3D(0.0f, 1.0f, 0.0f));
    uint topRingStart = vertices.size();
    for (int i = 0; i <= slices; ++i) {
        float phi = (float)i / slices * 2.0f * float(M_PI);
        vertices.append(QVector3D(std::cos(phi), +1.0f, std::sin(phi)));
        normals.append(QVector3D(0.0f, 1.0f, 0.0f));
    }
    for (int i = 0; i < slices; ++i) {
        indices.append(topCenterIdx);
        indices.append(topRingStart + i);
        indices.append(topRingStart + i + 1);
    }

    // Bottom cap (y=-1).
    uint botCenterIdx = vertices.size();
    vertices.append(QVector3D(0.0f, -1.0f, 0.0f));
    normals.append(QVector3D(0.0f, -1.0f, 0.0f));
    uint botRingStart = vertices.size();
    for (int i = 0; i <= slices; ++i) {
        float phi = (float)i / slices * 2.0f * float(M_PI);
        vertices.append(QVector3D(std::cos(phi), -1.0f, std::sin(phi)));
        normals.append(QVector3D(0.0f, -1.0f, 0.0f));
    }
    for (int i = 0; i < slices; ++i) {
        indices.append(botCenterIdx);
        indices.append(botRingStart + i + 1);
        indices.append(botRingStart + i);
    }
}

Qt3DRender::QMaterial* buildBondMaterial(Qt3DCore::QEntity* parent)
{
    auto* material = new Qt3DRender::QMaterial(parent);
    auto* effect = new Qt3DRender::QEffect(material);
    auto* technique = new Qt3DRender::QTechnique(effect);

    technique->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGL);
    technique->graphicsApiFilter()->setMajorVersion(3);
    technique->graphicsApiFilter()->setMinorVersion(3);
    technique->graphicsApiFilter()->setProfile(Qt3DRender::QGraphicsApiFilter::CoreProfile);

    auto* filterKey = new Qt3DRender::QFilterKey(technique);
    filterKey->setName(QStringLiteral("renderingStyle"));
    filterKey->setValue(QStringLiteral("forward"));
    technique->addFilterKey(filterKey);

    auto* renderPass = new Qt3DRender::QRenderPass(technique);
    auto* program = new Qt3DRender::QShaderProgram(renderPass);
    program->setVertexShaderCode(Qt3DRender::QShaderProgram::loadSource(
        QUrl(QStringLiteral("qrc:/shaders/src/shaders/bond_instanced.vert"))));
    program->setFragmentShaderCode(Qt3DRender::QShaderProgram::loadSource(
        QUrl(QStringLiteral("qrc:/shaders/src/shaders/bond_instanced.frag"))));
    QObject::connect(program, &Qt3DRender::QShaderProgram::logChanged, program, [program]() {
        const QString log = program->log();
        if (!log.isEmpty())
            qWarning() << "[bond_instanced shader]" << program->status() << log;
    });
    renderPass->setShaderProgram(program);
    technique->addRenderPass(renderPass);
    effect->addTechnique(technique);
    material->setEffect(effect);
    return material;
}

} // namespace

BondInstancingSystem::BondInstancingSystem(Qt3DCore::QEntity* rootEntity, QObject* parent)
    : QObject(parent)
    , m_rootEntity(rootEntity)
{
    m_instancedEntity = new Qt3DCore::QEntity(rootEntity);
    m_instancedEntity->setObjectName("BondInstancingEntity");
}

BondInstancingSystem::~BondInstancingSystem() = default;

void BondInstancingSystem::setBonds(const QVector<BondInstance>& instances)
{
    m_instances = instances;
    if (m_instances.isEmpty())
        return;

    createGeometry();
    createInstanceBuffer();
    m_instancedEntity->addComponent(buildBondMaterial(m_instancedEntity));
}

void BondInstancingSystem::updateTransforms(const QVector<QVector3D>& centers,
                                             const QVector<float>& halfLengths)
{
    if (centers.size() != m_instances.size() || halfLengths.size() != m_instances.size())
        return;
    for (int i = 0; i < m_instances.size(); ++i) {
        m_instances[i].center = centers[i];
        m_instances[i].halfLength = halfLengths[i];
    }
    uploadInstanceData();
}

void BondInstancingSystem::createGeometry()
{
    if (m_geometry) {
        delete m_geometry;
    }
    m_geometry = new Qt3DCore::QGeometry();

    QVector<QVector3D> verts, norms;
    QVector<uint> idx;
    // Slices=12: balance between visual smoothness and fragment count for large scenes.
    generateCylinderMesh(12, verts, norms, idx);

    auto* vbuf = new Qt3DCore::QBuffer(m_geometry);
    QByteArray vdata;
    vdata.resize(verts.size() * sizeof(QVector3D));
    memcpy(vdata.data(), verts.data(), vdata.size());
    vbuf->setData(vdata);

    auto* posAttr = new Qt3DCore::QAttribute();
    posAttr->setName(Qt3DCore::QAttribute::defaultPositionAttributeName());
    posAttr->setVertexBaseType(Qt3DCore::QAttribute::Float);
    posAttr->setVertexSize(3);
    posAttr->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
    posAttr->setBuffer(vbuf);
    posAttr->setByteStride(sizeof(QVector3D));
    posAttr->setCount(verts.size());
    m_geometry->addAttribute(posAttr);

    auto* nbuf = new Qt3DCore::QBuffer(m_geometry);
    QByteArray ndata;
    ndata.resize(norms.size() * sizeof(QVector3D));
    memcpy(ndata.data(), norms.data(), ndata.size());
    nbuf->setData(ndata);

    auto* normAttr = new Qt3DCore::QAttribute();
    normAttr->setName(Qt3DCore::QAttribute::defaultNormalAttributeName());
    normAttr->setVertexBaseType(Qt3DCore::QAttribute::Float);
    normAttr->setVertexSize(3);
    normAttr->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
    normAttr->setBuffer(nbuf);
    normAttr->setByteStride(sizeof(QVector3D));
    normAttr->setCount(norms.size());
    m_geometry->addAttribute(normAttr);

    auto* ibuf = new Qt3DCore::QBuffer(m_geometry);
    QByteArray idata;
    idata.resize(idx.size() * sizeof(uint));
    memcpy(idata.data(), idx.data(), idata.size());
    ibuf->setData(idata);

    auto* idxAttr = new Qt3DCore::QAttribute();
    idxAttr->setVertexBaseType(Qt3DCore::QAttribute::UnsignedInt);
    idxAttr->setAttributeType(Qt3DCore::QAttribute::IndexAttribute);
    idxAttr->setBuffer(ibuf);
    idxAttr->setCount(idx.size());
    m_geometry->addAttribute(idxAttr);
}

void BondInstancingSystem::createInstanceBuffer()
{
    // Layout per instance (16 floats = 64 bytes):
    //  0..2  center.xyz,     3   halfLength
    //  4..6  quat.xyz,       7   quat.w
    //  8..10 color.rgb,      11  color.a
    //  12    radius,         13..15 pad

    m_instanceBuffer = new Qt3DCore::QBuffer(m_geometry);
    uploadInstanceData();

    constexpr int STRIDE = 16 * sizeof(float);

    auto makeAttr = [&](const char* name, int size, int byteOffset) {
        auto* a = new Qt3DCore::QAttribute();
        a->setName(QString::fromLatin1(name));
        a->setVertexBaseType(Qt3DCore::QAttribute::Float);
        a->setVertexSize(size);
        a->setAttributeType(Qt3DCore::QAttribute::VertexAttribute);
        a->setBuffer(m_instanceBuffer);
        a->setByteStride(STRIDE);
        a->setByteOffset(byteOffset);
        a->setCount(m_instances.size());
        a->setDivisor(1);
        m_geometry->addAttribute(a);
    };

    makeAttr("instancePosLen", 4, 0);
    makeAttr("instanceQuat", 4, 4 * sizeof(float));
    makeAttr("instanceColor", 4, 8 * sizeof(float));
    makeAttr("instanceRadius", 4, 12 * sizeof(float));

    if (!m_geometryRenderer) {
        m_geometryRenderer = new Qt3DRender::QGeometryRenderer(m_instancedEntity);
        m_instancedEntity->addComponent(m_geometryRenderer);
    }
    m_geometryRenderer->setGeometry(m_geometry);
    m_geometryRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);
    m_geometryRenderer->setInstanceCount(m_instances.size());
}

void BondInstancingSystem::uploadInstanceData()
{
    if (m_instances.isEmpty() || !m_instanceBuffer)
        return;

    QByteArray data;
    data.resize(m_instances.size() * 16 * sizeof(float));
    float* p = reinterpret_cast<float*>(data.data());
    for (int i = 0; i < m_instances.size(); ++i) {
        const BondInstance& b = m_instances[i];
        p[i * 16 + 0] = b.center.x();
        p[i * 16 + 1] = b.center.y();
        p[i * 16 + 2] = b.center.z();
        p[i * 16 + 3] = b.halfLength;
        p[i * 16 + 4] = b.rotation.x();
        p[i * 16 + 5] = b.rotation.y();
        p[i * 16 + 6] = b.rotation.z();
        p[i * 16 + 7] = b.rotation.scalar();
        p[i * 16 + 8] = b.color.redF();
        p[i * 16 + 9] = b.color.greenF();
        p[i * 16 + 10] = b.color.blueF();
        p[i * 16 + 11] = b.color.alphaF();
        p[i * 16 + 12] = b.radius;
        p[i * 16 + 13] = 0.0f;
        p[i * 16 + 14] = 0.0f;
        p[i * 16 + 15] = 0.0f;
    }
    m_instanceBuffer->setData(data);
}

void BondInstancingSystem::setVisible(bool visible)
{
    if (m_instancedEntity)
        m_instancedEntity->setEnabled(visible);
}
