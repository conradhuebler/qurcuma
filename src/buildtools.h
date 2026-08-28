// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// buildtools — free functions for the molecule builder: valence accounting and
// automatic hydrogen placement from local VSEPR-style geometry. Kept as flat,
// documented functions (Educational-First) with no widget/state dependencies,
// so they are unit-testable without a GUI. Claude Generated 2026.
#pragma once

#include "view.h"

namespace build {

/** @brief Typical maximum valence (sum of bond orders) of an element.
 *  Returns 0 for elements outside the table (metals, noble gases, beads):
 *  those never receive automatic hydrogens. */
int maxValence(const QString& element);

/** @brief Sum of bond orders at @p atomIndex. */
int usedValence(int atomIndex, const QVector<MoleculeViewer::Bond>& bonds);

/** @brief Open valences of one atom: max(0, maxValence - usedValence);
 *  0 for unknown elements. */
int openValence(int atomIndex, const QVector<MoleculeViewer::Atom>& atoms,
    const QVector<MoleculeViewer::Bond>& bonds);

/** @brief Generate hydrogens for the open valences of the target atoms
 *  (@p targets empty = all atoms).
 *
 *  Placement follows VSEPR: the steric number SN = neighbours + missing H +
 *  lone pairs (O/S: 2, N/P: 1, halogens: 3) selects the geometry —
 *  2 = linear (180°), 3 = trigonal planar (120°), >= 4 = tetrahedral (109.47°).
 *  A double/triple bond lowers the H count via usedValence and thereby the SN,
 *  so sp2/sp centres come out planar/linear without explicit hybridisation
 *  detection. H distance = covalentRadius(X) + covalentRadius(H).
 *
 *  @param outH      new hydrogen atoms, to be appended AFTER the existing atoms
 *  @param outHBonds bonds (heavy atom index, atoms.size() + k) — absolute
 *                   indices into the combined array. */
void generateHydrogens(const QVector<MoleculeViewer::Atom>& atoms,
    const QVector<MoleculeViewer::Bond>& bonds, const QVector<int>& targets,
    QVector<MoleculeViewer::Atom>& outH, QVector<MoleculeViewer::Bond>& outHBonds);

} // namespace build
