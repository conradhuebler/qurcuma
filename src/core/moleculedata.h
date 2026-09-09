// moleculedata.h - The viewer's atom and bond records.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Lifted out of view.h so code that only needs the data
// records no longer has to pull in a ~970-line QWidget header. MoleculeFileLoader
// was the first case: it parses structure files and never touches the viewer, yet
// included view.h solely for these two structs.
//
// MoleculeViewer keeps `Atom` and `Bond` as aliases onto these, so every existing
// `MoleculeViewer::Atom` spelling in the tree stays valid.
//
// These are the *render* records and deliberately richer than curcuma's: the bond
// order carries the builder (aromatic rings, Kekule matching) and the renderer,
// and radius/type carry coarse-grained VTF beads. curcuma's Molecule stores bonds
// as bare index pairs and has no per-atom radius or type, so it cannot replace
// them; conversion for analysis goes through moleculebridge.h instead.
#pragma once

#include <QString>
#include <QVector3D>

namespace moldata {

/// One atom of one frame, in the viewer's own representation.
struct Atom {
    QVector3D position;
    QString element;
    float charge = 0.0f;  // Claude Generated - for charge-based coloring
    // Claude Generated 2026 - coarse-grained (VTF bead) support:
    float radius = 0.0f;  // per-atom draw radius; 0 = fall back to element vdW
    QString type;         // bead/residue type label; drives "By Type" colouring
};

/// A bond between two atoms of the same frame, by index into that frame.
struct Bond {
    int atom1;
    int atom2;
    int bondOrder;
};

}  // namespace moldata
