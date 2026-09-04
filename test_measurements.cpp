// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 — geometry and histogram binning behind the tracked
// measurements, checked against hand-computed reference values.

#include "measurements.h"

#include <QCoreApplication>
#include <cmath>
#include <cstdio>

static int g_failed = 0;

static void check(bool ok, const QString& what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
    if (!ok)
        ++g_failed;
}

static void near(double got, double want, double tol, const QString& what)
{
    const bool ok = std::isfinite(got) && std::abs(got - want) <= tol;
    std::printf("  %s  %s (got %.4f, want %.4f)\n", ok ? "PASS" : "FAIL",
        qPrintable(what), got, want);
    if (!ok)
        ++g_failed;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    using measure::Tracked;

    // --- distance -----------------------------------------------------------
    near(measure::distance({ 0, 0, 0 }, { 3, 4, 0 }), 5.0, 1e-5, "distance 3-4-5");

    // --- angle: right angle, straight, and fully folded ---------------------
    near(measure::angleDeg({ 1, 0, 0 }, { 0, 0, 0 }, { 0, 1, 0 }), 90.0, 1e-4, "angle 90 deg");
    near(measure::angleDeg({ 1, 0, 0 }, { 0, 0, 0 }, { -1, 0, 0 }), 180.0, 1e-3, "angle 180 deg (straight)");
    near(measure::angleDeg({ 1, 0, 0 }, { 0, 0, 0 }, { 1, 0, 0 }), 0.0, 1e-3, "angle 0 deg (folded)");
    // Water: O at the origin, the two O-H at the tetrahedral-ish experimental angle.
    near(measure::angleDeg({ 0.757f, 0.586f, 0.0f }, { 0, 0, 0 }, { -0.757f, 0.586f, 0.0f }),
        104.48, 0.05, "water H-O-H");

    // --- dihedral: the four canonical conformations -------------------------
    // Butane-like backbone a-b-c-d with b-c along x.
    const QVector3D b(0, 0, 0), c(1.5f, 0, 0);
    near(measure::dihedralDeg({ -0.5f, 1.0f, 0.0f }, b, c, { 2.0f, 1.0f, 0.0f }), 0.0, 1e-3,
        "dihedral 0 deg (syn)");
    near(measure::dihedralDeg({ -0.5f, 1.0f, 0.0f }, b, c, { 2.0f, -1.0f, 0.0f }), 180.0, 1e-3,
        "dihedral 180 deg (anti)");
    // Sign convention, worked through by hand for d above the bc axis:
    //   b1 = (0.5,-1,0), b2 = (1.5,0,0), b3 = (0.5,0,1)
    //   n1 = b1 x b2 = (0,0,1.5), n2 = b2 x b3 = (0,-1.5,0), m = n1 x b2_hat = (0,1.5,0)
    //   atan2(m.n2, n1.n2) = atan2(-2.25, 0) = -90 deg
    // Mirroring d through the bc axis must flip that sign and nothing else.
    near(measure::dihedralDeg({ -0.5f, 1.0f, 0.0f }, b, c, { 2.0f, 0.0f, 1.0f }), -90.0, 1e-3,
        "dihedral -90 deg (d above the bc axis)");
    near(measure::dihedralDeg({ -0.5f, 1.0f, 0.0f }, b, c, { 2.0f, 0.0f, -1.0f }), 90.0, 1e-3,
        "dihedral +90 deg (mirrored d flips the sign)");

    // --- Tracked ------------------------------------------------------------
    Tracked::Kind kind;
    check(Tracked::kindForCount(2, kind) && kind == Tracked::Kind::Distance, "2 atoms mean a distance");
    check(Tracked::kindForCount(3, kind) && kind == Tracked::Kind::Angle, "3 atoms mean an angle");
    check(Tracked::kindForCount(4, kind) && kind == Tracked::Kind::Dihedral, "4 atoms mean a dihedral");
    check(!Tracked::kindForCount(1, kind) && !Tracked::kindForCount(5, kind),
        "1 or 5 atoms are not a measurement");

    const std::vector<QVector3D> pos { { 0, 0, 0 }, { 3, 4, 0 }, { 3, 4, 5 } };
    Tracked d;
    d.kind = Tracked::Kind::Distance;
    d.atoms = { 0, 1 };
    near(d.evaluate(pos), 5.0, 1e-5, "tracked distance evaluates");
    check(d.unit() == QStringLiteral("A"), "a distance reports Angstrom");

    Tracked ang;
    ang.kind = Tracked::Kind::Angle;
    ang.atoms = { 0, 1, 2 };
    check(std::isfinite(ang.evaluate(pos)) && ang.unit() == QStringLiteral("deg"),
        "tracked angle evaluates and reports degrees");

    Tracked bad;
    bad.kind = Tracked::Kind::Distance;
    bad.atoms = { 0, 99 };
    check(!bad.isValid(int(pos.size())) && std::isnan(bad.evaluate(pos)),
        "an out-of-range index is invalid and evaluates to NaN");

    Tracked wrongCount;
    wrongCount.kind = Tracked::Kind::Dihedral;
    wrongCount.atoms = { 0, 1 };
    check(!wrongCount.isValid(int(pos.size())), "a dihedral needs four atoms");

    const QVector<QString> elements { "N", "H", "O" };
    check(Tracked::makeLabel({ 0, 1 }, elements) == QStringLiteral("N1-H2"),
        "label uses element symbols and one-based numbers");

    // --- histogram ----------------------------------------------------------
    const QVector<double> values { 0.0, 1.0, 2.0, 3.0, 4.0 };
    const measure::Histogram h = measure::histogram(values, 4);
    int total = 0;
    for (int c : h.counts)
        total += c;
    check(h.counts.size() == 4, QString("4 bins requested (got %1)").arg(h.counts.size()));
    check(total == values.size(), QString("every sample is binned (%1 of %2)").arg(total).arg(values.size()));
    near(h.min, 0.0, 1e-9, "histogram lower edge is the minimum");
    near(h.max, 4.0, 1e-9, "histogram upper edge is the maximum");
    near(h.binWidth, 1.0, 1e-9, "bin width is range / bins");
    check(h.counts.last() >= 1, "the maximum lands in the last bin, not past it");
    near(h.binCentre(0), 0.5, 1e-9, "first bin centre");

    const measure::Histogram constant = measure::histogram({ 2.5, 2.5, 2.5 }, 10);
    check(constant.counts.size() == 1 && constant.counts[0] == 3 && constant.binWidth > 0.0,
        "a constant sample set gives one bin, not a zero-width range");

    QVector<double> withNan { 1.0, std::nan(""), 3.0 };
    const measure::Histogram skipped = measure::histogram(withNan, 2);
    int t2 = 0;
    for (int c : skipped.counts)
        t2 += c;
    check(t2 == 2, QString("NaN samples are skipped (binned %1 of 3)").arg(t2));

    check(measure::histogram({}, 5).counts.isEmpty(), "no samples give an empty histogram");

    std::printf("%s (%d failed)\n", g_failed == 0 ? "PASS" : "FAIL", g_failed);
    return g_failed == 0 ? 0 : 1;
}
