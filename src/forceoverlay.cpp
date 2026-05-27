// forceoverlay.cpp - Visual force arrows for interactive simulation
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - Force visualization overlay

#include "forceoverlay.h"

#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QConeMesh>
#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DExtras/QPhongMaterial>

#include <QtMath>

ForceOverlay::ForceOverlay(Qt3DCore::QEntity *parentEntity)
    : QObject(parentEntity)
    , m_parentEntity(parentEntity)
{
}

ForceOverlay::~ForceOverlay()
{
    clear();
}

void ForceOverlay::clear()
{
    destroyArrow(m_mainArrow);
    for (auto it = m_distArrows.begin(); it != m_distArrows.end(); ++it)
        destroyArrow(it.value());
    m_distArrows.clear();
}

QColor ForceOverlay::forceToColor(float magnitude) const
{
    float t = qBound(0.0f, magnitude / m_maxForceMagnitude, 1.0f);
    int r = static_cast<int>(255.0f * t);
    int g = static_cast<int>(255.0f * (1.0f - t));
    return QColor(r, g, 0);
}

void ForceOverlay::destroyArrow(Arrow &arrow)
{
    if (arrow.parent) {
        arrow.parent->deleteLater();
        arrow.parent = nullptr;
    }
    arrow.shaft = nullptr;
    arrow.tip = nullptr;
    arrow.shaftTransform = nullptr;
    arrow.tipTransform = nullptr;
    arrow.material = nullptr;
    arrow.shaftMesh = nullptr;
    arrow.tipMesh = nullptr;
    arrow.atomIndex = -1;
}

ForceOverlay::Arrow* ForceOverlay::ensureArrow(int atomIndex, bool isMain)
{
    if (isMain) {
        if (!m_mainArrow.parent)
            m_mainArrow.parent = new Qt3DCore::QEntity(m_parentEntity);
        m_mainArrow.isMain = true;
        m_mainArrow.atomIndex = atomIndex;
        return &m_mainArrow;
    }

    auto it = m_distArrows.find(atomIndex);
    if (it != m_distArrows.end()) {
        it.value().atomIndex = atomIndex;
        return &it.value();
    }

    Arrow arrow;
    arrow.parent = new Qt3DCore::QEntity(m_parentEntity);
    arrow.isMain = false;
    arrow.atomIndex = atomIndex;
    m_distArrows.insert(atomIndex, arrow);
    return &m_distArrows[atomIndex];
}

void ForceOverlay::applyArrowGeometry(Arrow *arrow, const QVector3D &position,
    const QVector3D &force, bool isMain)
{
    if (!arrow || !arrow->parent)
        return;

    float forceLen = force.length();
    if (forceLen < 1e-9f)
        forceLen = 1e-9f;

    QVector3D dir = force / forceLen;

    // Scale arrow length by force magnitude, clamped for readability
    float shaftLength = qBound(0.3f, forceLen * m_arrowScale, 6.0f);
    if (!isMain)
        shaftLength *= 0.6f;

    float tipLength = shaftLength * 0.25f;
    float shaftRadius = isMain ? 0.06f : 0.03f;
    float tipBottomRadius = shaftRadius * 2.5f;

    // Rotation: align local Y with direction
    QVector3D localUp(0.0f, 1.0f, 0.0f);
    QVector3D rotAxis = QVector3D::crossProduct(localUp, dir);
    QQuaternion rotation;
    if (rotAxis.lengthSquared() > 1e-6f) {
        rotAxis.normalize();
        float angle = qAcos(QVector3D::dotProduct(localUp, dir));
        rotation = QQuaternion::fromAxisAndAngle(rotAxis, angle * 180.0f / M_PI);
    } else if (QVector3D::dotProduct(localUp, dir) < 0.0f) {
        rotation = QQuaternion::fromAxisAndAngle(QVector3D(1.0f, 0.0f, 0.0f), 180.0f);
    }

    // Lazy-create mesh / transform / material on first call
    if (!arrow->shaft) {
        arrow->shaft = new Qt3DCore::QEntity(arrow->parent);
        arrow->shaftTransform = new Qt3DCore::QTransform(arrow->shaft);
        arrow->shaft->addComponent(arrow->shaftTransform);

        arrow->shaftMesh = new Qt3DExtras::QCylinderMesh(arrow->shaft);
        arrow->shaftMesh->setRings(4);
        arrow->shaftMesh->setSlices(12);
        arrow->shaft->addComponent(arrow->shaftMesh);
    }
    if (!arrow->tip) {
        arrow->tip = new Qt3DCore::QEntity(arrow->parent);
        arrow->tipTransform = new Qt3DCore::QTransform(arrow->tip);
        arrow->tip->addComponent(arrow->tipTransform);

        arrow->tipMesh = new Qt3DExtras::QConeMesh(arrow->tip);
        arrow->tipMesh->setRings(4);
        arrow->tipMesh->setSlices(12);
        arrow->tipMesh->setHasTopEndcap(false);
        arrow->tipMesh->setHasBottomEndcap(true);
        arrow->tip->addComponent(arrow->tipMesh);
    }
    if (!arrow->material) {
        arrow->material = new Qt3DExtras::QPhongMaterial(arrow->parent);
        arrow->shaft->addComponent(arrow->material);
        arrow->tip->addComponent(arrow->material);
    }

    // Update shaft
    arrow->shaftMesh->setRadius(shaftRadius);
    arrow->shaftMesh->setLength(shaftLength);
    arrow->shaftTransform->setTranslation(position + dir * shaftLength * 0.5f);
    arrow->shaftTransform->setRotation(rotation);

    // Update tip
    arrow->tipMesh->setBottomRadius(tipBottomRadius);
    arrow->tipMesh->setTopRadius(0.0f);
    arrow->tipMesh->setLength(tipLength);
    arrow->tipTransform->setTranslation(position + dir * (shaftLength + tipLength * 0.5f));
    arrow->tipTransform->setRotation(rotation);

    // Update color
    QColor col = forceToColor(forceLen);
    arrow->material->setAmbient(col.darker(150));
    arrow->material->setDiffuse(col);

    arrow->cachedForce = force;
}

void ForceOverlay::updateMainArrow(int atomIndex, const QVector3D &force,
    const QVector<QVector3D> &atomPositions)
{
    if (atomIndex < 0 || atomIndex >= atomPositions.size())
        return;

    Arrow *arrow = ensureArrow(atomIndex, /*isMain=*/true);
    applyArrowGeometry(arrow, atomPositions[atomIndex], force, /*isMain=*/true);
}

void ForceOverlay::updateDistributedArrows(const QVector<int> &atomIndices,
    const QVector<QVector3D> &forces,
    const QVector<QVector3D> &atomPositions)
{
    // Mark which arrows we still need
    QHash<int, bool> needed;
    int count = qMin(atomIndices.size(), forces.size());
    for (int i = 0; i < count; ++i) {
        int idx = atomIndices[i];
        if (idx < 0 || idx >= atomPositions.size())
            continue;
        if (forces[i].lengthSquared() < 1e-14f)
            continue;
        needed.insert(idx, true);

        Arrow *arrow = ensureArrow(idx, /*isMain=*/false);
        applyArrowGeometry(arrow, atomPositions[idx], forces[i], /*isMain=*/false);
    }

    // Remove arrows for atoms no longer affected
    auto it = m_distArrows.begin();
    while (it != m_distArrows.end()) {
        if (!needed.contains(it.key())) {
            destroyArrow(it.value());
            it = m_distArrows.erase(it);
        } else {
            ++it;
        }
    }
}

void ForceOverlay::updatePositions(const QVector<QVector3D> &atomPositions)
{
    // Update main arrow
    if (m_mainArrow.parent && m_mainArrow.atomIndex >= 0
        && m_mainArrow.atomIndex < atomPositions.size()) {
        applyArrowGeometry(&m_mainArrow, atomPositions[m_mainArrow.atomIndex],
            m_mainArrow.cachedForce, /*isMain=*/true);
    }

    // Update distributed arrows
    for (auto it = m_distArrows.begin(); it != m_distArrows.end(); ++it) {
        Arrow &arrow = it.value();
        if (arrow.atomIndex >= 0 && arrow.atomIndex < atomPositions.size()) {
            applyArrowGeometry(&arrow, atomPositions[arrow.atomIndex],
                arrow.cachedForce, /*isMain=*/false);
        }
    }
}
