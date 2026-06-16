// forceoverlay.h - Visual force arrows for interactive simulation
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - Force visualization overlay

#ifndef FORCEOVERLAY_H
#define FORCEOVERLAY_H

#include <QColor>
#include <QHash>
#include <QObject>
#include <QVector>
#include <QVector3D>

namespace Qt3DCore {
class QEntity;
class QTransform;
}
namespace Qt3DExtras {
class QCylinderMesh;
class QConeMesh;
class QPhongMaterial;
}

class ForceOverlay : public QObject
{
    Q_OBJECT
public:
    explicit ForceOverlay(Qt3DCore::QEntity *parentEntity);
    ~ForceOverlay();

    /** Update main arrow at atomIndex. Creates entity on first call, reuses afterwards. */
    void updateMainArrow(int atomIndex, const QVector3D &force,
        const QVector<QVector3D> &atomPositions);

    /** Update distributed arrows. Reuses existing entities, creates only new ones.
     *  Removes arrows for atoms no longer in the list. */
    void updateDistributedArrows(const QVector<int> &atomIndices,
        const QVector<QVector3D> &forces,
        const QVector<QVector3D> &atomPositions);

    /** Remove all arrows. */
    void clear();

    void setMaxForceMagnitude(float max) { m_maxForceMagnitude = max; }
    void setArrowScale(float scale) { m_arrowScale = scale; }

private:
    struct Arrow {
        Qt3DCore::QEntity *parent = nullptr;
        Qt3DCore::QEntity *shaft = nullptr;
        Qt3DCore::QEntity *tip = nullptr;
        Qt3DCore::QTransform *shaftTransform = nullptr;
        Qt3DCore::QTransform *tipTransform = nullptr;
        Qt3DExtras::QPhongMaterial *material = nullptr;
        Qt3DExtras::QCylinderMesh *shaftMesh = nullptr;
        Qt3DExtras::QConeMesh *tipMesh = nullptr;
        int atomIndex = -1;
        QVector3D cachedForce;
        bool isMain = false;
    };

    Arrow* ensureArrow(int atomIndex, bool isMain);
    void destroyArrow(Arrow &arrow);
    void applyArrowGeometry(Arrow *arrow, const QVector3D &position,
        const QVector3D &force, bool isMain);
    QColor forceToColor(float magnitude) const;

    Qt3DCore::QEntity *m_parentEntity = nullptr;
    Arrow m_mainArrow;
    QHash<int, Arrow> m_distArrows;
    float m_maxForceMagnitude = 0.05f; // Eh/Bohr
    float m_arrowScale = 150.0f;
};

#endif // FORCEOVERLAY_H
