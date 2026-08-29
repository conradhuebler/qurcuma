// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// fragmentlibrary — small built-in set of molecular fragments for the builder:
// standalone molecules (water, benzene, ...) and substituents with one open
// valence (methyl, phenyl, ...) that dock onto a selected atom. Plain data, no
// files, no SMILES. Claude Generated 2026.
#pragma once

#include "view.h"

namespace build {

/** @brief One insertable fragment. attachAtom >= 0 marks a substituent: that
 *  atom carries an open valence and is bonded to the target on attachment;
 *  -1 = standalone molecule. */
struct Fragment {
    QString name;
    QString category;  // menu section: Substituents / Molecules / Gases / Materials
    QVector<MoleculeViewer::Atom> atoms;
    QVector<MoleculeViewer::Bond> bonds;
    int attachAtom = -1;
};

/** @brief The built-in fragments (built once). Ring skeletons are saturated
 *  with hydrogens via build::generateHydrogens, so their geometry rules match
 *  the Add-H button exactly. */
const QVector<Fragment>& fragmentLibrary();

} // namespace build
