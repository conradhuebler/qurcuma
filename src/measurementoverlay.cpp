// Claude Generated - Phase 2B - Measurement Overlay Implementation
#include "measurementoverlay.h"
#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DCore/QTransform>
#include <cmath>
#include <QDebug>

MeasurementOverlay::MeasurementOverlay(Qt3DCore::QEntity *rootEntity)
    : m_rootEntity(rootEntity)
{
    // Create main lines entity
    m_linesEntity = new Qt3DCore::QEntity(m_rootEntity);
}

MeasurementOverlay::~MeasurementOverlay()
{
    clearMeasurement();
}

void MeasurementOverlay::setMeasurement(MeasurementType type, const QVector<int>& atomIndices)
{
    m_currentMeasurement.type = type;
    m_currentMeasurement.atomIndices = atomIndices;
    m_currentMeasurement.value = 0.0f;
    m_currentMeasurement.valueText = "";
}

void MeasurementOverlay::clearMeasurement()
{
    m_currentMeasurement.type = NoMeasurement;
    m_currentMeasurement.atomIndices.clear();
    m_currentMeasurement.valueText = "";
    m_currentMeasurement.value = 0.0f;
    clearVisualization();
}

void MeasurementOverlay::updateMeasurement(const QVector<QVector3D>& atomPositions)
{
    if (m_currentMeasurement.type == NoMeasurement || m_currentMeasurement.atomIndices.isEmpty()) {
        return;
    }

    // Validate atom indices
    for (int idx : m_currentMeasurement.atomIndices) {
        if (idx < 0 || idx >= atomPositions.size()) {
            clearMeasurement();
            return;
        }
    }

    QVector<QVector3D> positions;
    for (int idx : m_currentMeasurement.atomIndices) {
        positions.append(atomPositions[idx]);
    }

    // Calculate measurement based on type
    switch (m_currentMeasurement.type) {
        case Distance: {
            if (positions.size() >= 2) {
                float dist = calculateDistance(positions[0], positions[1]);
                m_currentMeasurement.value = dist;
                m_currentMeasurement.valueText = QString::number(dist, 'f', 2) + " Å";
                createDistanceVisualization(positions);
            }
            break;
        }
        case Angle: {
            if (positions.size() >= 3) {
                float angle = calculateAngle(positions[0], positions[1], positions[2]);
                m_currentMeasurement.value = angle;
                m_currentMeasurement.valueText = QString::number(angle, 'f', 1) + "°";
                createAngleVisualization(positions);
            }
            break;
        }
        case Dihedral: {
            if (positions.size() >= 4) {
                float dihedral = calculateDihedral(positions[0], positions[1],
                                                   positions[2], positions[3]);
                m_currentMeasurement.value = dihedral;
                m_currentMeasurement.valueText = QString::number(dihedral, 'f', 1) + "°";
                createDihedralVisualization(positions);
            }
            break;
        }
        default:
            break;
    }
}

float MeasurementOverlay::calculateDistance(const QVector3D& p1, const QVector3D& p2)
{
    return (p2 - p1).length();
}

float MeasurementOverlay::calculateAngle(const QVector3D& p1, const QVector3D& p2,
                                         const QVector3D& p3)
{
    // Angle at p2, formed by p1-p2-p3
    QVector3D v1 = (p1 - p2).normalized();
    QVector3D v2 = (p3 - p2).normalized();

    float cosAngle = QVector3D::dotProduct(v1, v2);
    cosAngle = qBound(-1.0f, cosAngle, 1.0f);
    float angleRad = qAcos(cosAngle);
    float angleDeg = angleRad * 180.0f / M_PI;

    return angleDeg;
}

float MeasurementOverlay::calculateDihedral(const QVector3D& p1, const QVector3D& p2,
                                            const QVector3D& p3, const QVector3D& p4)
{
    // Dihedral angle between planes p1-p2-p3 and p2-p3-p4
    QVector3D v1 = (p2 - p1);
    QVector3D v2 = (p3 - p2);
    QVector3D v3 = (p4 - p3);

    QVector3D n1 = QVector3D::crossProduct(v1, v2).normalized();
    QVector3D n2 = QVector3D::crossProduct(v2, v3).normalized();

    float cosAngle = QVector3D::dotProduct(n1, n2);
    cosAngle = qBound(-1.0f, cosAngle, 1.0f);
    float angleRad = qAcos(cosAngle);
    float angleDeg = angleRad * 180.0f / M_PI;

    return angleDeg;
}

void MeasurementOverlay::createDistanceVisualization(const QVector<QVector3D>& positions)
{
    clearVisualization();

    if (positions.size() < 2) return;

    // Create line between two atoms
    Qt3DCore::QEntity *line = createLine(positions[0], positions[1]);
    if (line) {
        m_lineEntities.append(line);
    }
}

void MeasurementOverlay::createAngleVisualization(const QVector<QVector3D>& positions)
{
    clearVisualization();

    if (positions.size() < 3) return;

    // Create two lines: p1-p2 and p3-p2 (centered at p2)
    Qt3DCore::QEntity *line1 = createLine(positions[0], positions[1]);
    Qt3DCore::QEntity *line2 = createLine(positions[1], positions[2]);

    if (line1) m_lineEntities.append(line1);
    if (line2) m_lineEntities.append(line2);
}

void MeasurementOverlay::createDihedralVisualization(const QVector<QVector3D>& positions)
{
    clearVisualization();

    if (positions.size() < 4) return;

    // Create central bond line and two reference lines
    Qt3DCore::QEntity *centralBond = createLine(positions[1], positions[2]);
    Qt3DCore::QEntity *line1 = createLine(positions[0], positions[1]);
    Qt3DCore::QEntity *line2 = createLine(positions[2], positions[3]);

    if (centralBond) m_lineEntities.append(centralBond);
    if (line1) m_lineEntities.append(line1);
    if (line2) m_lineEntities.append(line2);
}

Qt3DCore::QEntity* MeasurementOverlay::createLine(const QVector3D& start, const QVector3D& end)
{
    if ((start - end).length() < 0.001f) {
        return nullptr;  // Degenerate line
    }

    Qt3DCore::QEntity *lineEntity = new Qt3DCore::QEntity(m_linesEntity);

    // Mesh: thin cylinder
    Qt3DExtras::QCylinderMesh *mesh = new Qt3DExtras::QCylinderMesh();
    mesh->setRadius(m_lineThickness);
    mesh->setLength((end - start).length());
    mesh->setRings(8);
    mesh->setSlices(8);

    // Material
    Qt3DExtras::QPhongMaterial *material = new Qt3DExtras::QPhongMaterial();
    material->setAmbient(m_lineColor.darker());
    material->setDiffuse(m_lineColor);
    material->setShininess(100);

    // Transform
    Qt3DCore::QTransform *transform = new Qt3DCore::QTransform();

    // Position at midpoint
    QVector3D midpoint = (start + end) * 0.5f;
    transform->setTranslation(midpoint);

    // Rotation to align with vector
    QVector3D direction = (end - start).normalized();
    QVector3D localUp(0, 1, 0);
    QVector3D rotationAxis = QVector3D::crossProduct(localUp, direction);

    if (rotationAxis.length() > 0.001f) {
        rotationAxis.normalize();
        float angle = qAcos(QVector3D::dotProduct(localUp, direction));
        angle = angle * 180.0f / M_PI;
        transform->setRotation(QQuaternion::fromAxisAndAngle(rotationAxis, angle));
    } else if (QVector3D::dotProduct(localUp, direction) < 0) {
        // Opposite direction
        transform->setRotation(QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 180.0f));
    }

    lineEntity->addComponent(mesh);
    lineEntity->addComponent(material);
    lineEntity->addComponent(transform);

    return lineEntity;
}

void MeasurementOverlay::clearVisualization()
{
    for (Qt3DCore::QEntity *entity : m_lineEntities) {
        entity->deleteLater();
    }
    m_lineEntities.clear();
}

void MeasurementOverlay::updateLineColor(const QColor& color)
{
    m_lineColor = color;
}

void MeasurementOverlay::setLineThickness(float thickness)
{
    m_lineThickness = qBound(0.05f, thickness, 0.5f);
}

QVector3D MeasurementOverlay::calculateLabelPosition(const QVector<QVector3D>& positions)
{
    // Calculate centroid of all positions
    QVector3D centroid(0, 0, 0);
    for (const QVector3D& pos : positions) {
        centroid += pos;
    }
    centroid /= static_cast<float>(positions.size());

    // Offset label slightly above measurements
    centroid += QVector3D(0, 1.0f, 0);  // 1 Å offset upward

    return centroid;
}
