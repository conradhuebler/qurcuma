// Claude Generated - Phase 5D - Frustum Culling Implementation
#include "frustumculler.h"
#include <cmath>
#include <algorithm>

FrustumCuller::FrustumCuller()
{
}

FrustumCuller::~FrustumCuller()
{
}

void FrustumCuller::updateFrustum(const QMatrix4x4& viewProjectionMatrix)
{
    // Extract frustum planes from view-projection matrix
    // Reference: Fast Extraction of Viewing Frustum Planes from the World-View-Projection Matrix
    // https://www.gamedevs.org/uploads/raw/b45426858c954500293a4ba10115176f.pdf

    const float *data = viewProjectionMatrix.constData();

    // Right plane: [m30 - m00, m31 - m01, m32 - m02, m33 - m03]
    m_planes[0].a = data[12] - data[0];
    m_planes[0].b = data[13] - data[1];
    m_planes[0].c = data[14] - data[2];
    m_planes[0].d = data[15] - data[3];

    // Left plane: [m30 + m00, m31 + m01, m32 + m02, m33 + m03]
    m_planes[1].a = data[12] + data[0];
    m_planes[1].b = data[13] + data[1];
    m_planes[1].c = data[14] + data[2];
    m_planes[1].d = data[15] + data[3];

    // Bottom plane: [m30 + m10, m31 + m11, m32 + m12, m33 + m13]
    m_planes[2].a = data[12] + data[4];
    m_planes[2].b = data[13] + data[5];
    m_planes[2].c = data[14] + data[6];
    m_planes[2].d = data[15] + data[7];

    // Top plane: [m30 - m10, m31 - m11, m32 - m12, m33 - m13]
    m_planes[3].a = data[12] - data[4];
    m_planes[3].b = data[13] - data[5];
    m_planes[3].c = data[14] - data[6];
    m_planes[3].d = data[15] - data[7];

    // Far plane: [m30 - m20, m31 - m21, m32 - m22, m33 - m23]
    m_planes[4].a = data[12] - data[8];
    m_planes[4].b = data[13] - data[9];
    m_planes[4].c = data[14] - data[10];
    m_planes[4].d = data[15] - data[11];

    // Near plane: [m20, m21, m22, m23]
    m_planes[5].a = data[8];
    m_planes[5].b = data[9];
    m_planes[5].c = data[10];
    m_planes[5].d = data[11];

    // Normalize all planes
    for (int i = 0; i < 6; ++i) {
        m_planes[i].normalize();
    }
}

bool FrustumCuller::isAtomVisible(const QVector3D& position, float radius) const
{
    // Sphere vs. frustum plane test
    // Sphere is visible if distance from center to plane > -radius for all 6 planes

    for (int i = 0; i < 6; ++i) {
        float dist = m_planes[i].distance(position);
        if (dist < -radius) {
            // Sphere is completely on the negative side of this plane (outside)
            return false;
        }
    }
    return true;
}

bool FrustumCuller::isAABBVisible(const QVector3D& min, const QVector3D& max) const
{
    // AABB vs. frustum plane test
    // Test all 8 corners against all 6 planes

    for (int i = 0; i < 6; ++i) {
        // Get the positive extent of the box relative to the plane normal
        QVector3D p;
        p.setX(m_planes[i].a > 0 ? max.x() : min.x());
        p.setY(m_planes[i].b > 0 ? max.y() : min.y());
        p.setZ(m_planes[i].c > 0 ? max.z() : min.z());

        // If the positive extent is behind the plane, the whole box is behind
        if (m_planes[i].distance(p) < -EPSILON) {
            return false;
        }
    }
    return true;
}

QVector<int> FrustumCuller::cullAtoms(const QVector<QVector3D>& atomPositions,
                                      const QVector<float>& atomScales) const
{
    QVector<int> visibleAtoms;

    for (int i = 0; i < atomPositions.size(); ++i) {
        float scale = (i < atomScales.size()) ? atomScales[i] : 1.0f;
        float radius = scale * 0.5f;  // Approximate radius for visibility test

        if (isAtomVisible(atomPositions[i], radius)) {
            visibleAtoms.append(i);
        }
    }

    return visibleAtoms;
}

QVector<int> FrustumCuller::cullBonds(const QVector<Bond>& bonds,
                                     const QVector<bool>& visibleAtoms) const
{
    QVector<int> visibleBonds;

    for (int i = 0; i < bonds.size(); ++i) {
        const Bond& bond = bonds[i];

        // Bond is visible if at least one endpoint is visible
        if ((bond.atom1 < visibleAtoms.size() && visibleAtoms[bond.atom1]) ||
            (bond.atom2 < visibleAtoms.size() && visibleAtoms[bond.atom2])) {
            visibleBonds.append(i);
        }
    }

    return visibleBonds;
}
