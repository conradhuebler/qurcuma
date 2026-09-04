// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// measurements — quantities of a structure that can be followed over a trajectory:
// internal coordinates (distance, angle, dihedral), the RMSD to the run's first
// frame, and the radius of gyration. Plus histogram binning.
//
// The geometry itself lives in curcuma (`GeometryTools`, `RMSDFunctions`), not
// here: curcuma needs the same quantities in its own capabilities, and a second
// implementation in the GUI would drift from it. This file is the Qt-side adapter
// — it converts QVector3D to curcuma's Position/Geometry, carries the labels and
// colours, and decides which atoms go into which quantity.
//
// Claude Generated 2026.
#pragma once

#include <QColor>
#include <QString>
#include <QVector>
#include <QVector3D>
#include <vector>

namespace measure {

/** @brief Distance a-b in Angstrom (curcuma GeometryTools::Distance). */
double distance(const QVector3D& a, const QVector3D& b);

/** @brief Angle a-b-c at the middle atom @p b, in degrees (GeometryTools::Angle). */
double angleDeg(const QVector3D& a, const QVector3D& b, const QVector3D& c);

/** @brief Dihedral a-b-c-d in degrees, signed (GeometryTools::Dihedral). */
double dihedralDeg(const QVector3D& a, const QVector3D& b, const QVector3D& c, const QVector3D& d);

/** @brief Unweighted radius of gyration of a coordinate set, in Angstrom
 *  (GeometryTools::GyrationRadius). */
double gyrationRadius(const std::vector<QVector3D>& positions);

/** @brief Best-fit (Kabsch) RMSD of @p positions against @p reference in Angstrom,
 *  in the given atom order (curcuma RMSDFunctions). Returns NaN when the two sets
 *  differ in size. Centroids are removed first, so this is the RMSD after optimal
 *  superposition and not a raw coordinate difference. */
double rmsdToReference(const std::vector<QVector3D>& positions,
    const std::vector<QVector3D>& reference);

/**
 * @brief One quantity followed over a run.
 *
 * An internal coordinate takes 2, 3 or 4 atoms, which is also how the viewer's
 * Measure mode reads a selection. The two whole-structure quantities (RMSD to the
 * first frame, radius of gyration) take none.
 */
struct Tracked {
    enum class Kind {
        Distance,
        Angle,
        Dihedral,
        RmsdToStart,
        GyrationRadius
    };

    Kind kind = Kind::Distance;
    QVector<int> atoms;   ///< 2, 3 or 4 atom indices for an internal coordinate; empty otherwise
    QString label;        ///< shown in the legend, e.g. "N1-H4"
    QColor colour = Qt::black;

    /** @brief True for the quantities computed from the whole structure. */
    static bool isWholeStructure(Kind kind);

    /** @brief Kind implied by a picked-atom count; false for anything but 2..4. */
    static bool kindForCount(int count, Kind& kind);

    /** @brief How many atoms this kind needs (0 for a whole-structure quantity). */
    int requiredAtoms() const;

    /** @brief Atom count matches the kind and every index is inside @p atomCount. */
    bool isValid(int atomCount) const;

    /**
     * @brief The quantity for these positions, or NaN when it cannot be computed.
     * @param reference first frame of the run; only read for Kind::RmsdToStart.
     */
    double evaluate(const std::vector<QVector3D>& positions,
        const std::vector<QVector3D>& reference = {}) const;

    /** @brief "A" for a length, "deg" for an angle. */
    QString unit() const;

    /** @brief Default label from element symbols, e.g. "N1-H4"; a fixed name for
     *  the whole-structure quantities. */
    static QString makeLabel(Kind kind, const QVector<int>& atoms, const QVector<QString>& elements);
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
