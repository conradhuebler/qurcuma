// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// WP0 spike - bond cylinder instancing. Claude Generated.
#include "bondinstancing.h"

#include <QtMath>

namespace {
// Quick3D built-in "#Cylinder" is a 100-unit-tall mesh (local y in [-50,50]) with
// radius 50. To render a cylinder of scene length L and radius r:
//   scale.y = L / 100   (and half-segment of half-length h -> scale.y = h / 50)
//   scale.x = scale.z = r / 50
constexpr float kCylHalfHeight = 50.0f;
constexpr float kCylRadius = 50.0f;

/// Quaternion rotating the cylinder's local +Y axis onto a unit bond direction.
/// Identical logic to qurcuma src/view.cpp:1345. Claude Generated.
QQuaternion bondRotation(const QVector3D& normDir)
{
    const QVector3D localUp(0, 1, 0);
    const QVector3D rotAxis = QVector3D::crossProduct(localUp, normDir);
    if (rotAxis.length() < 0.001f) {
        return normDir.y() > 0 ? QQuaternion()
                               : QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 180.0f);
    }
    const float angle = qAcos(QVector3D::dotProduct(localUp, normDir)) * 180.0f / float(M_PI);
    return QQuaternion::fromAxisAndAngle(rotAxis.normalized(), angle);
}
}

BondInstancing::BondInstancing(QQuick3DObject* parent)
    : QQuick3DInstancing(parent)
{
}

void BondInstancing::setBonds(const QVector<mol::Atom>& atoms,
    const QVector<mol::Bond>& bonds, float radius)
{
    // Two half-cylinders per bond (one per atom colour).
    m_count = bonds.size() * 2;
    m_buffer.resize(m_count * int(sizeof(InstanceTableEntry)));
    auto* entry = reinterpret_cast<InstanceTableEntry*>(m_buffer.data());

    const QVector3D cylScaleXZ(radius / kCylRadius, 0, radius / kCylRadius);
    int n = 0;
    for (const mol::Bond& bond : bonds) {
        if (bond.atom1 < 0 || bond.atom2 < 0
            || bond.atom1 >= atoms.size() || bond.atom2 >= atoms.size())
            continue;
        const QVector3D posA = atoms[bond.atom1].position;
        const QVector3D posB = atoms[bond.atom2].position;
        const QVector3D direction = posB - posA;
        const float length = direction.length();
        if (length < 1e-4f)
            continue;
        const QVector3D normDir = direction / length;
        const QQuaternion rotation = bondRotation(normDir);

        const QVector3D mid = 0.5f * (posA + posB);
        const float halfLength = length * 0.25f; // half of each colour segment
        const QVector3D scale(cylScaleXZ.x(), halfLength / kCylHalfHeight, cylScaleXZ.z());

        // Half A: from posA to mid, coloured like atom A.
        entry[n++] = calculateTableEntryFromQuaternion(0.5f * (posA + mid), scale,
            rotation, mol::cpkColor(atoms[bond.atom1].element));
        // Half B: from mid to posB, coloured like atom B.
        entry[n++] = calculateTableEntryFromQuaternion(0.5f * (mid + posB), scale,
            rotation, mol::cpkColor(atoms[bond.atom2].element));
    }
    m_count = n; // skipped degenerate bonds
    markDirty();
}

QByteArray BondInstancing::getInstanceBuffer(int* instanceCount)
{
    if (instanceCount)
        *instanceCount = m_count;
    return m_buffer;
}
