// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 — see scenefiller.h.

#include "scenefiller.h"

#include <QtMath>
#include <cmath>

namespace build {

bool insideContainer(const Container& c, const QVector3D& p)
{
    if (c.kind == Container::Sphere)
        return p.length() <= c.radius - c.margin;
    return p.x() >= c.min.x() + c.margin && p.x() <= c.max.x() - c.margin
        && p.y() >= c.min.y() + c.margin && p.y() <= c.max.y() - c.margin
        && p.z() >= c.min.z() + c.margin && p.z() <= c.max.z() - c.margin;
}

// Shoemake, "Uniform Random Rotations", Graphics Gems III (1992): three uniform
// deviates map to a quaternion drawn evenly over SO(3). Sampling Euler angles
// uniformly instead would clump orientations near the poles.
QQuaternion randomRotation(QRandomGenerator& rng)
{
    const double u1 = rng.generateDouble();
    const double u2 = rng.generateDouble();
    const double u3 = rng.generateDouble();
    const double s1 = std::sqrt(1.0 - u1), s2 = std::sqrt(u1);
    const double t1 = 2.0 * M_PI * u2, t2 = 2.0 * M_PI * u3;
    return QQuaternion(float(s2 * std::cos(t2)),   // w
        float(s1 * std::sin(t1)),                  // x
        float(s1 * std::cos(t1)),                  // y
        float(s2 * std::sin(t2)));                 // z
}

QVector3D randomPointIn(const Container& c, QRandomGenerator& rng)
{
    QVector3D lo, hi;
    if (c.kind == Container::Sphere) {
        lo = QVector3D(-c.radius, -c.radius, -c.radius);
        hi = QVector3D(c.radius, c.radius, c.radius);
    } else {
        lo = c.min;
        hi = c.max;
    }
    // Rejection sampling keeps the distribution uniform over the volume; for a
    // sphere it accepts ~52 % of draws, which is cheap enough here.
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const QVector3D p(
            float(lo.x() + rng.generateDouble() * (hi.x() - lo.x())),
            float(lo.y() + rng.generateDouble() * (hi.y() - lo.y())),
            float(lo.z() + rng.generateDouble() * (hi.z() - lo.z())));
        if (insideContainer(c, p))
            return p;
    }
    return QVector3D(0, 0, 0);
}

FillResult fillContainer(const QVector<FillRequest>& requests,
    const Container& container,
    const QVector<MoleculeViewer::Atom>& existing,
    float minDistance,
    int maxAttemptsPerCopy,
    quint32 seed)
{
    FillResult result;
    QRandomGenerator local(seed);
    QRandomGenerator& rng = (seed == 0) ? *QRandomGenerator::global() : local;

    // Occupied positions: everything already in the scene plus what we place.
    QVector<QVector3D> occupied;
    occupied.reserve(existing.size() + 64);
    for (const MoleculeViewer::Atom& a : existing)
        occupied.append(a.position);

    const float minDistSq = minDistance * minDistance;

    for (const FillRequest& req : requests) {
        if (!req.fragment || req.count <= 0)
            continue;
        result.requested += req.count;

        // Centroid of the template, so the random rotation turns the fragment
        // about its own centre rather than swinging it around the origin.
        QVector3D centroid;
        for (const MoleculeViewer::Atom& a : req.fragment->atoms)
            centroid += a.position;
        if (!req.fragment->atoms.isEmpty())
            centroid /= float(req.fragment->atoms.size());

        for (int copy = 0; copy < req.count; ++copy) {
            bool ok = false;
            QVector<QVector3D> candidate;
            for (int attempt = 0; attempt < maxAttemptsPerCopy && !ok; ++attempt) {
                const QQuaternion rot = randomRotation(rng);
                const QVector3D centre = randomPointIn(container, rng);
                candidate.clear();
                candidate.reserve(req.fragment->atoms.size());
                ok = true;
                for (const MoleculeViewer::Atom& a : req.fragment->atoms) {
                    const QVector3D p = centre + rot.rotatedVector(a.position - centroid);
                    if (!insideContainer(container, p)) {
                        ok = false;
                        break;
                    }
                    for (const QVector3D& q : occupied) {
                        if ((p - q).lengthSquared() < minDistSq) {
                            ok = false;
                            break;
                        }
                    }
                    if (!ok)
                        break;
                    candidate.append(p);
                }
            }
            if (!ok)
                continue;  // container full for this fragment; try the next copy

            const int base = result.atoms.size();
            for (int i = 0; i < candidate.size(); ++i) {
                MoleculeViewer::Atom a = req.fragment->atoms[i];
                a.position = candidate[i];
                result.atoms.append(a);
                occupied.append(candidate[i]);
            }
            for (const MoleculeViewer::Bond& b : req.fragment->bonds)
                result.bonds.append({ base + b.atom1, base + b.atom2, b.bondOrder });
            ++result.placed;
        }
    }
    return result;
}

} // namespace build
