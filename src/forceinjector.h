// forceinjector.h - Topological force distribution across bond shells
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - Interactive Simulation

#pragma once

#include <Eigen/Dense>
#include <QVector>

#include "view.h"

namespace forceinjector {

using Adjacency = QVector<QVector<int>>;

/** Build a per-atom adjacency list from the viewer bond vector. */
Adjacency buildAdjacency(int atomCount, const QVector<MoleculeViewer::Bond>& bonds);

/** Distribute a force across an atom and its bonded neighbours.
 *
 *  BFS from @p seedAtom through the adjacency graph. Each atom in shell @c d
 *  (seed = 0) receives force @c pow(alpha,d) * seedForce. Traversal stops at
 *  @p maxShells. Atoms that are not reached keep a zero force.
 *
 *  @param seedAtom    Atom the user grabbed (0-based index).
 *  @param seedForce   Force vector in the integrator's units (Eh/Bohr).
 *  @param adjacency   Adjacency list — entry i holds the indices bonded to i.
 *  @param alpha       Per-shell damping factor, typically 0.3-0.6.
 *  @param maxShells   Maximum BFS depth (0 = only the seed atom).
 *  @param totalAtoms  Size of the returned matrix.
 *  @return (totalAtoms x 3) matrix of per-atom forces. */
Eigen::MatrixXd distributeForce(
    int seedAtom,
    const Eigen::Vector3d& seedForce,
    const Adjacency& adjacency,
    double alpha,
    int maxShells,
    int totalAtoms);

} // namespace forceinjector
