// Claude Generated - Phase 2B - Measurement Overlay System
#ifndef MEASUREMENTOVERLAY_H
#define MEASUREMENTOVERLAY_H

#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <QVector3D>
#include <QVector>
#include <QString>
#include <QColor>

/**
 * @brief Manages measurement visualization in 3D space
 *
 * Displays distance, angle, and dihedral measurements with:
 * - Line visualization between atoms (cylinders)
 * - Text labels showing measurement values
 * - Dynamic positioning following atom movements
 */
class MeasurementOverlay
{
public:
    enum MeasurementType {
        NoMeasurement = 0,
        Distance = 1,      // 2 atoms
        Angle = 2,         // 3 atoms
        Dihedral = 3       // 4 atoms
    };

    struct MeasurementData {
        MeasurementType type;
        QVector<int> atomIndices;  // 2, 3, or 4 atom indices
        QString valueText;         // Formatted value (e.g., "2.45 Å", "120.5°")
        float value;               // Raw numeric value
    };

    MeasurementOverlay(Qt3DCore::QEntity *rootEntity);
    ~MeasurementOverlay();

    // Measurement management
    void setMeasurement(MeasurementType type, const QVector<int>& atomIndices);
    void clearMeasurement();
    const MeasurementData& getCurrentMeasurement() const { return m_currentMeasurement; }

    // Update measurements based on atom positions
    void updateMeasurement(const QVector<QVector3D>& atomPositions);

private:
    Qt3DCore::QEntity *m_rootEntity = nullptr;
    Qt3DCore::QEntity *m_linesEntity = nullptr;       // Contains line meshes (cylinders)
    MeasurementData m_currentMeasurement;

    // Stored references for updates
    QVector<Qt3DCore::QEntity*> m_lineEntities;       // One or more lines for measurement
    QColor m_lineColor = QColor(255, 200, 50);       // Default: orange-yellow
    float m_lineThickness = 0.1f;

    // Calculation helpers
    float calculateDistance(const QVector3D& p1, const QVector3D& p2);
    float calculateAngle(const QVector3D& p1, const QVector3D& p2, const QVector3D& p3);
    float calculateDihedral(const QVector3D& p1, const QVector3D& p2,
                           const QVector3D& p3, const QVector3D& p4);

    // Visualization
    void createDistanceVisualization(const QVector<QVector3D>& positions);
    void createAngleVisualization(const QVector<QVector3D>& positions);
    void createDihedralVisualization(const QVector<QVector3D>& positions);
    void clearVisualization();

    Qt3DCore::QEntity* createLine(const QVector3D& start, const QVector3D& end);
};

#endif // MEASUREMENTOVERLAY_H
