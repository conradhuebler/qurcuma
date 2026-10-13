// bonddetect.h - Distance-based bond detection with a cell list
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026: replaces the all-pairs loop of MoleculeViewer::detectBonds*, which cost
// O(N^2) pair tests plus two element-string lookups per pair on the GUI thread for every live
// frame (measured: 24 ms per frame at 1410 atoms, 610 to 714 ms at 7320; remote.log, Oct 2026).

#pragma once

#include <QHash>
#include <QSet>
#include <QVector>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <utility>

namespace bonddetect {

inline quint64 pairKey(int i, int j)
{
    return (static_cast<quint64>(qMin(i, j)) << 32) | static_cast<quint32>(qMax(i, j));
}

/**
 * @brief All pairs i < j whose distance is at most (r_i + r_j) * tol, ordered by (i, j).
 *
 * `tol` is `breakTol` for a pair contained in @p bonded and `formTol` otherwise (the hysteresis
 * of the live viewer: an existing bond survives until it is stretched past the looser threshold).
 * With @p bonded == nullptr every pair uses `formTol`. The result equals that of the all-pairs
 * loop with the same arithmetic; the cell list only decides which pairs get tested: cells are
 * 2 * maxRadius * max(formTol, breakTol) wide, so every pair within its threshold lies in the
 * same or an adjacent cell.
 *
 * Claude Generated 2026.
 *
 * @param positions  Atom positions in Angstrom.
 * @param radius     Covalent radius per atom, same units and order as @p positions.
 */
inline QVector<std::pair<int, int>> pairsWithin(const QVector<QVector3D>& positions, const QVector<float>& radius,
    float formTol, float breakTol, const QSet<quint64>* bonded)
{
    const int n = int(positions.size());
    QVector<std::pair<int, int>> out;
    if (n < 2 || radius.size() != n)
        return out;

    float maxR = 0.0f;
    for (float r : radius)
        maxR = std::max(maxR, r);
    const float cell = std::max(1e-3f, 2.0f * maxR * std::max(formTol, breakTol));

    QVector3D lo = positions[0];
    for (const QVector3D& p : positions)
        lo = QVector3D(std::min(lo.x(), p.x()), std::min(lo.y(), p.y()), std::min(lo.z(), p.z()));

    auto cellOf = [&](const QVector3D& p, int& cx, int& cy, int& cz) {
        cx = int(std::floor((p.x() - lo.x()) / cell));
        cy = int(std::floor((p.y() - lo.y()) / cell));
        cz = int(std::floor((p.z() - lo.z()) / cell));
    };
    auto cellKey = [](int cx, int cy, int cz) {
        return (static_cast<quint64>(cx + 1) << 42) | (static_cast<quint64>(cy + 1) << 21) | static_cast<quint64>(cz + 1);
    };

    QHash<quint64, QVector<int>> cells;
    cells.reserve(n);
    for (int i = 0; i < n; ++i) {
        int cx, cy, cz;
        cellOf(positions[i], cx, cy, cz);
        cells[cellKey(cx, cy, cz)].append(i);
    }

    for (int i = 0; i < n; ++i) {
        int cx, cy, cz;
        cellOf(positions[i], cx, cy, cz);
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
                for (int dz = -1; dz <= 1; ++dz) {
                    const auto it = cells.constFind(cellKey(cx + dx, cy + dy, cz + dz));
                    if (it == cells.constEnd())
                        continue;
                    for (int j : it.value()) {
                        if (j <= i)
                            continue;
                        const float distance = (positions[i] - positions[j]).length();
                        const float r = radius[i] + radius[j];
                        const float tol = (bonded && bonded->contains(pairKey(i, j))) ? breakTol : formTol;
                        if (distance <= r * tol)
                            out.append({ i, j });
                    }
                }
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace bonddetect
