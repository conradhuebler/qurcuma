// moleculetypes.h - GUI-free atom and bond records
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - Split out of view.h so the simulation worker and the remote
// server (qurcuma-server, no widgets) can use them. MoleculeViewer::Atom/Bond are aliases.

#pragma once

#include <QString>
#include <QVector3D>

struct MolAtom {
    QVector3D position;
    QString element;
    float charge = 0.0f;  // for charge-based colouring
    // coarse-grained (VTF bead) support
    float radius = 0.0f;  // per-atom draw radius; 0 = fall back to element vdW
    QString type;         // bead/residue type label; drives "By Type" colouring
};

struct MolBond {
    int atom1;
    int atom2;
    int bondOrder;
};
