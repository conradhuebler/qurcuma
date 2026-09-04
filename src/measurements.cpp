// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 — see measurements.h. Qt-side adapter over curcuma's
// GeometryTools and RMSDFunctions; no geometry is implemented here.

#include "measurements.h"

#include <src/capabilities/rmsd/rmsd_functions.h>
#include <src/tools/geometry.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

inline Position toPosition(const QVector3D& v)
{
    return Position(double(v.x()), double(v.y()), double(v.z()));
}

Geometry toGeometry(const std::vector<QVector3D>& positions)
{
    Geometry g(static_cast<int>(positions.size()), 3);
    for (size_t i = 0; i < positions.size(); ++i) {
        g(int(i), 0) = double(positions[i].x());
        g(int(i), 1) = double(positions[i].y());
        g(int(i), 2) = double(positions[i].z());
    }
    return g;
}

} // namespace

namespace measure {

double distance(const QVector3D& a, const QVector3D& b)
{
    return GeometryTools::Distance(toPosition(a), toPosition(b));
}

double angleDeg(const QVector3D& a, const QVector3D& b, const QVector3D& c)
{
    return GeometryTools::Angle(toPosition(a), toPosition(b), toPosition(c));
}

double dihedralDeg(const QVector3D& a, const QVector3D& b, const QVector3D& c, const QVector3D& d)
{
    return GeometryTools::Dihedral(toPosition(a), toPosition(b), toPosition(c), toPosition(d));
}

double gyrationRadius(const std::vector<QVector3D>& positions)
{
    if (positions.empty())
        return std::numeric_limits<double>::quiet_NaN();
    return GeometryTools::GyrationRadius(toGeometry(positions));
}

double rmsdToReference(const std::vector<QVector3D>& positions,
    const std::vector<QVector3D>& reference)
{
    if (positions.empty() || positions.size() != reference.size())
        return std::numeric_limits<double>::quiet_NaN();

    // Remove both centroids, then the optimal rotation: what is plotted is the RMSD
    // after superposition, so a molecule drifting or tumbling through the box does
    // not register as a structural change.
    Geometry ref = toGeometry(reference);
    Geometry tar = toGeometry(positions);
    ref = GeometryTools::TranslateGeometry(ref, GeometryTools::Centroid(ref), Position { 0, 0, 0 });
    tar = GeometryTools::TranslateGeometry(tar, GeometryTools::Centroid(tar), Position { 0, 0, 0 });
    const Eigen::Matrix3d rotation = RMSDFunctions::BestFitRotation(ref, tar, 1);
    return RMSDFunctions::getRMSD(ref, RMSDFunctions::applyRotation(tar, rotation));
}

bool Tracked::isWholeStructure(Kind kind)
{
    return kind == Kind::RmsdToStart || kind == Kind::GyrationRadius;
}

bool Tracked::kindForCount(int count, Kind& kind)
{
    switch (count) {
    case 2: kind = Kind::Distance; return true;
    case 3: kind = Kind::Angle; return true;
    case 4: kind = Kind::Dihedral; return true;
    default: return false;
    }
}

int Tracked::requiredAtoms() const
{
    switch (kind) {
    case Kind::Distance: return 2;
    case Kind::Angle: return 3;
    case Kind::Dihedral: return 4;
    case Kind::RmsdToStart:
    case Kind::GyrationRadius: return 0;
    }
    return 0;
}

bool Tracked::isValid(int atomCount) const
{
    if (isWholeStructure(kind))
        return atomCount > 0;
    if (atoms.size() != requiredAtoms())
        return false;
    for (int a : atoms)
        if (a < 0 || a >= atomCount)
            return false;
    return true;
}

double Tracked::evaluate(const std::vector<QVector3D>& positions,
    const std::vector<QVector3D>& reference) const
{
    if (!isValid(static_cast<int>(positions.size())))
        return std::numeric_limits<double>::quiet_NaN();
    switch (kind) {
    case Kind::Distance:
        return distance(positions[atoms[0]], positions[atoms[1]]);
    case Kind::Angle:
        return angleDeg(positions[atoms[0]], positions[atoms[1]], positions[atoms[2]]);
    case Kind::Dihedral:
        return dihedralDeg(positions[atoms[0]], positions[atoms[1]],
            positions[atoms[2]], positions[atoms[3]]);
    case Kind::GyrationRadius:
        return gyrationRadius(positions);
    case Kind::RmsdToStart:
        return rmsdToReference(positions, reference);
    }
    return std::numeric_limits<double>::quiet_NaN();
}

QString Tracked::unit() const
{
    return (kind == Kind::Angle || kind == Kind::Dihedral)
        ? QStringLiteral("deg")
        : QStringLiteral("A");
}

QString Tracked::makeLabel(Kind kind, const QVector<int>& atoms, const QVector<QString>& elements)
{
    if (kind == Kind::RmsdToStart)
        return QStringLiteral("RMSD to start");
    if (kind == Kind::GyrationRadius)
        return QStringLiteral("Radius of gyration");
    QStringList parts;
    for (int a : atoms) {
        const QString el = (a >= 0 && a < elements.size()) ? elements[a] : QStringLiteral("?");
        parts << QStringLiteral("%1%2").arg(el).arg(a + 1);
    }
    return parts.join(QStringLiteral("-"));
}

Histogram histogram(const QVector<double>& values, int bins)
{
    Histogram h;
    if (values.isEmpty() || bins < 1)
        return h;

    double lo = std::numeric_limits<double>::max();
    double hi = -std::numeric_limits<double>::max();
    int finite = 0;
    for (double v : values) {
        if (!std::isfinite(v))
            continue;
        lo = std::min(lo, v);
        hi = std::max(hi, v);
        ++finite;
    }
    if (finite == 0)
        return h;

    // A constant sample set has no range to divide; report one bin holding
    // everything rather than dividing by zero.
    if (hi - lo < 1e-12) {
        h.min = lo - 0.5;
        h.max = lo + 0.5;
        h.binWidth = 1.0;
        h.counts = QVector<int>(1, finite);
        return h;
    }

    h.min = lo;
    h.max = hi;
    h.binWidth = (hi - lo) / bins;
    h.counts = QVector<int>(bins, 0);
    for (double v : values) {
        if (!std::isfinite(v))
            continue;
        int b = static_cast<int>((v - lo) / h.binWidth);
        b = std::max(0, std::min(b, bins - 1));   // the maximum lands in the last bin
        ++h.counts[b];
    }
    return h;
}

} // namespace measure
