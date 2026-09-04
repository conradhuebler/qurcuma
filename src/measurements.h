// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// measurements — geometric quantities of a structure (distance, angle, dihedral)
// and a description of one quantity tracked over a trajectory, plus histogram
// binning. Free functions over plain data, no widgets, so the geometry and the
// binning are testable without a GUI (test_measurements).
//
// The viewer's Measure HUD and the live charts share these functions, so a number
// read off the HUD and the same number in a plot cannot drift apart.
// Claude Generated 2026.
#pragma once

#include <QColor>
#include <QString>
#include <QVector>
#include <QVector3D>
#include <vector>

namespace measure {

/** @brief Distance a-b in Angstrom. */
double distance(const QVector3D& a, const QVector3D& b);

/** @brief Angle a-b-c at the middle atom @p b, in degrees (0 … 180). */
double angleDeg(const QVector3D& a, const QVector3D& b, const QVector3D& c);

/** @brief Dihedral a-b-c-d in degrees, signed (-180 … 180, IUPAC convention). */
double dihedralDeg(const QVector3D& a, const QVector3D& b, const QVector3D& c, const QVector3D& d);

/**
 * @brief One quantity followed over a run: which atoms, and what to compute.
 *
 * The kind follows from how many atoms were picked, which is also how the viewer's
 * Measure mode reads a selection: 2 = distance, 3 = angle, 4 = dihedral.
 */
struct Tracked {
    enum class Kind {
        Distance,
        Angle,
        Dihedral
    };

    Kind kind = Kind::Distance;
    QVector<int> atoms;   ///< 2, 3 or 4 zero-based atom indices, in order
    QString label;        ///< shown in the legend, e.g. "N1-H4"
    QColor colour = Qt::black;

    /** @brief Kind implied by a picked-atom count; false for anything but 2..4. */
    static bool kindForCount(int count, Kind& kind);

    /** @brief Atom count matches the kind and every index is inside @p atomCount. */
    bool isValid(int atomCount) const;

    /** @brief The quantity for these positions, or NaN when the indices do not fit. */
    double evaluate(const std::vector<QVector3D>& positions) const;

    /** @brief "A" for a distance, "deg" for an angle or dihedral. */
    QString unit() const;

    /** @brief Default label from element symbols, e.g. "N1-H4-H5". */
    static QString makeLabel(const QVector<int>& atoms, const QVector<QString>& elements);
};

/**
 * @brief Counts per bin over [min, max] of the sampled values.
 *
 * @c counts is empty when there is nothing to bin. A constant sample set yields a
 * single bin holding every value, rather than a zero-width range.
 */
struct Histogram {
    double min = 0.0;
    double max = 0.0;
    double binWidth = 0.0;
    QVector<int> counts;

    /** @brief Centre of bin @p i, for plotting. */
    double binCentre(int i) const { return min + (i + 0.5) * binWidth; }
};

/** @brief Bin @p values into @p bins equal-width bins spanning their range. */
Histogram histogram(const QVector<double>& values, int bins);

} // namespace measure
