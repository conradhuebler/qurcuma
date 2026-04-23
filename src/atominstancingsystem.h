// Claude Generated - Phase 3B - GPU Instancing System for Atom Rendering
#ifndef ATOMINSTANCINGSYSTEM_H
#define ATOMINSTANCINGSYSTEM_H

#include <Qt3DCore/QBuffer>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QGeometry>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QParameter>
#include <QVector3D>
#include <QColor>
#include <QVector>
#include <QString>

/**
 * @brief GPU Instancing System for efficient atom rendering
 *
 * Renders atoms using GPU instancing (single sphere mesh with per-instance transforms):
 * - Single sphere mesh shared across all atoms
 * - Per-instance: position, scale, color via vertex attributes
 * - 10x performance improvement for >1000 atoms
 * - Custom ray-casting for atom picking (replaces ObjectPicker)
 */
class AtomInstancingSystem : public QObject
{
    Q_OBJECT

public:
    struct AtomInstance {
        QVector3D position;
        float scale;
        QColor color;
        int atomIndex;  // For picking mapping
    };

    explicit AtomInstancingSystem(Qt3DCore::QEntity *rootEntity, QObject *parent = nullptr);
    ~AtomInstancingSystem();

    // Atom rendering
    void setAtoms(const QVector<QVector3D>& positions,
                  const QVector<QString>& elements,
                  const QVector<QColor>& colors,
                  const QVector<float>& scales);

    void updateAtomPositions(const QVector<QVector3D>& positions);
    void updateAtomColors(const QVector<QColor>& colors);
    void updateAtomScales(const QVector<float>& scales);

    // Update world-space camera position for correct specular lighting
    void setCameraPosition(const QVector3D &pos);

    // Claude Generated 2026 - Phase 1 Fog
    void setFogEnabled(bool enabled);
    void setFogColor(const QColor &color);
    void setFogDensity(float density);

    // Picking support
    int raycastAtom(const QVector3D& rayOrigin, const QVector3D& rayDirection,
                   const QVector<QVector3D>& atomPositions,
                   float pickingRadius = 0.3f) const;

    // Status
    int getAtomCount() const { return m_atomInstances.size(); }
    bool isSupported() const { return m_isSupported; }
    QString getLastError() const { return m_lastError; }

    // Show/hide
    void setVisible(bool visible);
    bool isVisible() const { return m_isVisible; }

signals:
    void atomCountChanged(int count);
    void renderingModeChanged(bool isInstancing);

private:
    void createGeometry();
    void createInstanceBuffer();
    void updateInstanceBuffer();
    void setupMaterial();

    Qt3DCore::QEntity *m_rootEntity = nullptr;
    Qt3DCore::QEntity *m_instancedEntity = nullptr;

    // Geometry components
    Qt3DCore::QGeometry *m_geometry = nullptr;
    Qt3DRender::QGeometryRenderer *m_geometryRenderer = nullptr;
    // Claude Generated - Phase 3.1: cached instance buffer for zero-realloc updates.
    Qt3DCore::QBuffer *m_instanceBuffer = nullptr;

    // Instance data
    QVector<AtomInstance> m_atomInstances;

    // Lighting parameter - updated each frame with camera world position
    Qt3DRender::QParameter* m_cameraPosParam = nullptr;

    // Claude Generated 2026 - Phase 1 Fog parameters
    Qt3DRender::QParameter* m_fogEnabledParam = nullptr;
    Qt3DRender::QParameter* m_fogColorParam = nullptr;
    Qt3DRender::QParameter* m_fogDensityParam = nullptr;

    // Status
    bool m_isSupported = true;
    bool m_isVisible = true;
    QString m_lastError = "";

    // Atom vertex attribute structure
    struct VertexData {
        QVector3D position;
        float radius;
    };
};

#endif // ATOMINSTANCINGSYSTEM_H
