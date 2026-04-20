// Claude Generated - Phase 3.2: GPU instancing for bonds.
// Single base cylinder mesh rendered N times with per-instance transform (pos, length,
// quaternion rotation, radius, color). Companion to AtomInstancingSystem.

#ifndef BONDINSTANCINGSYSTEM_H
#define BONDINSTANCINGSYSTEM_H

#include <Qt3DCore/QBuffer>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QGeometry>
#include <Qt3DRender/QGeometryRenderer>
#include <QColor>
#include <QObject>
#include <QQuaternion>
#include <QVector3D>
#include <QVector>

class BondInstancingSystem : public QObject
{
    Q_OBJECT

public:
    struct BondInstance {
        QVector3D center;        // bond midpoint
        float halfLength;        // length / 2 (cylinder base is y in [-1, +1])
        QQuaternion rotation;    // rotates local Y to bond direction
        QColor color;
        float radius;
    };

    explicit BondInstancingSystem(Qt3DCore::QEntity* rootEntity, QObject* parent = nullptr);
    ~BondInstancingSystem() override;

    /** Build buffers from full instance list. Called once per scene rebuild. */
    void setBonds(const QVector<BondInstance>& instances);

    /** Per-frame update: centers + half-lengths change, rotations stay cached. */
    void updateTransforms(const QVector<QVector3D>& centers, const QVector<float>& halfLengths);

    int bondCount() const { return m_instances.size(); }
    void setVisible(bool visible);

private:
    void createGeometry();
    void createInstanceBuffer();
    void uploadInstanceData();

    Qt3DCore::QEntity* m_rootEntity = nullptr;
    Qt3DCore::QEntity* m_instancedEntity = nullptr;
    Qt3DCore::QGeometry* m_geometry = nullptr;
    Qt3DRender::QGeometryRenderer* m_geometryRenderer = nullptr;
    Qt3DCore::QBuffer* m_instanceBuffer = nullptr;

    QVector<BondInstance> m_instances;
};

#endif // BONDINSTANCINGSYSTEM_H
