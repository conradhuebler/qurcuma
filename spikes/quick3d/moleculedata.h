// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// WP0 Qt Quick 3D spike - self-contained molecule data layer.
// Deliberately free of any qurcuma/Qt3D headers so the spike stays standalone.
// Claude Generated.
#pragma once

#include <QColor>
#include <QString>
#include <QVector>
#include <QVector3D>

namespace mol {

/// One atom: position in Angstrom + element symbol. Mirrors MoleculeViewer::Atom
/// but kept local so the spike does not drag in src/view.h (-> Qt3D).
struct Atom {
    QVector3D position;
    QString element;
};

/// Index pair into the atom list. bondOrder unused for the grid (always 1).
struct Bond {
    int atom1 = 0;
    int atom2 = 0;
    int bondOrder = 1;
};

/// A loaded/generated structure plus its bonds.
struct Structure {
    QVector<Atom> atoms;
    QVector<Bond> bonds;
};

/// CPK element colour (same table as MoleculeViewer::getAtomColor). Claude Generated.
QColor cpkColor(const QString& element);

/// Van-der-Waals-ish display radius in Angstrom (MoleculeViewer::getAtomRadius).
float vdwRadius(const QString& element);

/// Covalent radius for distance-based bond detection (MoleculeViewer::getCovalentRadius).
float covalentRadius(const QString& element);

/// Distance-based bond detection: bond if dist <= 1.25*(rcov_i+rcov_j). O(N^2),
/// fine for the spike's stress sizes because the grid supplies lattice bonds directly.
QVector<Bond> detectBonds(const QVector<Atom>& atoms);

/// Synthetic carbon grid with ~targetCount atoms, plus nearest-neighbour lattice
/// bonds along +x/+y/+z so the bond-cylinder/quaternion path is exercised in all
/// three axes. spacing in Angstrom. Claude Generated.
Structure makeGrid(int targetCount, float spacing = 2.6f);

/// Minimal multi-frame-agnostic XYZ loader (first frame only): line1=count,
/// line2=comment, then "Element x y z". Bonds auto-detected. Claude Generated.
bool loadXyz(const QString& path, Structure& out, QString* error = nullptr);

} // namespace mol
