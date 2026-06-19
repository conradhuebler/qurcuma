// moleculebridge.h
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Shared conversion helpers between qurcuma's viewer
// atom list (MoleculeViewer::Atom) and curcuma's Molecule type. Mirrors the
// file-static atomsToMolecule() in simulationworker.cpp so the RMSD/align
// dialog can build a curcuma Molecule and read results back without touching
// the simulation code.
#pragma once

#include "view.h"  // MoleculeViewer::Atom

#include <src/core/elements.h>
#include <src/core/molecule.h>

#include <QString>
#include <QVector>
#include <QVector3D>

#include <utility>

/**
 * @brief Build a curcuma Molecule from a qurcuma viewer atom list.
 * Element symbol -> atomic number via Elements::String2Element; positions are
 * passed through unchanged (Angstrom).
 * Claude Generated.
 */
inline curcuma::Molecule atomsToMolecule(const QVector<MoleculeViewer::Atom>& atoms)
{
    curcuma::Molecule mol;
    for (const auto& atom : atoms) {
        int Z = Elements::String2Element(atom.element.toStdString());
        Position pos(atom.position.x(), atom.position.y(), atom.position.z());
        mol.addPair({ Z, pos });
    }
    return mol;
}

/**
 * @brief Build a qurcuma viewer atom list from a curcuma Molecule.
 * Atomic number -> element symbol via Elements::ElementAbbr (index = Z);
 * positions are read in Angstrom. Used to read back the aligned/reordered
 * target after RMSDDriver::start().
 * Claude Generated.
 */
inline QVector<MoleculeViewer::Atom> moleculeToAtoms(const curcuma::Molecule& mol)
{
    QVector<MoleculeViewer::Atom> atoms;
    const int n = static_cast<int>(mol.AtomCount());
    atoms.reserve(n);
    for (int i = 0; i < n; ++i) {
        const std::pair<int, Position> a = mol.Atom(i);
        const int Z = a.first;
        MoleculeViewer::Atom atom;
        atom.element = (Z >= 0 && Z < static_cast<int>(Elements::ElementAbbr.size()))
            ? QString::fromStdString(Elements::ElementAbbr[Z])
            : QStringLiteral("X");
        atom.position = QVector3D(static_cast<float>(a.second.x()),
            static_cast<float>(a.second.y()),
            static_cast<float>(a.second.z()));
        atoms.append(atom);
    }
    return atoms;
}
