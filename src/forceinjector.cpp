// forceinjector.cpp - Topological force distribution across bond shells
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - Interactive Simulation

#include "forceinjector.h"

#include <queue>
#include <vector>

namespace forceinjector {

Adjacency buildAdjacency(int atomCount, const QVector<MoleculeViewer::Bond>& bonds)
{
    Adjacency adj(atomCount);
    for (const MoleculeViewer::Bond& b : bonds) {
        if (b.atom1 < 0 || b.atom2 < 0 || b.atom1 >= atomCount || b.atom2 >= atomCount)
            continue;
        if (b.atom1 == b.atom2)
            continue;
        adj[b.atom1].append(b.atom2);
        adj[b.atom2].append(b.atom1);
    }
    return adj;
}

Eigen::MatrixXd distributeForce(
    int seedAtom,
    const Eigen::Vector3d& seedForce,
    const Adjacency& adjacency,
    double alpha,
    int maxShells,
    int totalAtoms)
{
    Eigen::MatrixXd forces = Eigen::MatrixXd::Zero(totalAtoms, 3);
    if (totalAtoms <= 0 || seedAtom < 0 || seedAtom >= totalAtoms)
        return forces;

    std::vector<bool> visited(totalAtoms, false);
    std::queue<std::pair<int, int>> bfs;
    bfs.push({ seedAtom, 0 });
    visited[seedAtom] = true;

    while (!bfs.empty()) {
        auto [idx, depth] = bfs.front();
        bfs.pop();

        const double scale = (depth == 0) ? 1.0 : std::pow(alpha, depth);
        forces.row(idx) = scale * seedForce.transpose();

        if (maxShells >= 0 && depth >= maxShells)
            continue;
        if (idx >= adjacency.size())
            continue;

        for (int neighbour : adjacency[idx]) {
            if (neighbour < 0 || neighbour >= totalAtoms)
                continue;
            if (visited[neighbour])
                continue;
            visited[neighbour] = true;
            bfs.push({ neighbour, depth + 1 });
        }
    }

    return forces;
}

} // namespace forceinjector
