// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 — see measurements.h.

#include "measurements.h"

#include <QtMath>
#include <algorithm>
#include <cmath>
#include <limits>

namespace measure {

double distance(const QVector3D& a, const QVector3D& b)
{
    return double((b - a).length());
}

double angleDeg(const QVector3D& a, const QVector3D& b, const QVector3D& c)
{
    const QVector3D u = (a - b).normalized();
    const QVector3D v = (c - b).normalized();
    // Clamp before acos: rounding can push the dot product a hair outside [-1, 1]
    // for a straight or fully folded arrangement, which would give NaN.
    const float dot = qBound(-1.0f, QVector3D::dotProduct(u, v), 1.0f);
    return double(qRadiansToDegrees(qAcos(dot)));
}

double dihedralDeg(const QVector3D& a, const QVector3D& b, const QVector3D& c, const QVector3D& d)
{
    const QVector3D b1 = b - a, b2 = c - b, b3 = d - c;
    const QVector3D n1 = QVector3D::crossProduct(b1, b2);
    const QVector3D n2 = QVector3D::crossProduct(b2, b3);
    const QVector3D m = QVector3D::crossProduct(n1, b2.normalized());
    return double(qRadiansToDegrees(qAtan2(QVector3D::dotProduct(m, n2),
        QVector3D::dotProduct(n1, n2))));
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

bool Tracked::isValid(int atomCount) const
{
    const int need = (kind == Kind::Distance) ? 2 : (kind == Kind::Angle) ? 3 : 4;
    if (atoms.size() != need)
        return false;
    for (int a : atoms)
        if (a < 0 || a >= atomCount)
            return false;
    return true;
}

double Tracked::evaluate(const std::vector<QVector3D>& positions) const
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
    }
    return std::numeric_limits<double>::quiet_NaN();
}

QString Tracked::unit() const
{
    return kind == Kind::Distance ? QStringLiteral("A") : QStringLiteral("deg");
}

QString Tracked::makeLabel(const QVector<int>& atoms, const QVector<QString>& elements)
{
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
        b = qBound(0, b, bins - 1);   // the maximum lands in the last bin, not past it
        ++h.counts[b];
    }
    return h;
}

} // namespace measure
