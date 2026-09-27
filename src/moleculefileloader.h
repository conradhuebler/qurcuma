// moleculefileloader.h - Unified structure-file reader.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Single entry point for reading molecular structure
// files (xyz / vtf / pdb / mol2 / cif) into viewer atoms/bonds. Replaces the format
// dispatch ladder that was copy-pasted across MainWindow (full trajectory load,
// first-frame merge, remote download). Parsing only — no UI side effects, and
// since 2026-09 no dependency on view.h either: the whole parse path speaks
// moldata::Atom / ::Bond, so it is usable without QtWidgets.

#pragma once

#include "core/moleculedata.h"  // moldata::Atom / ::Bond

#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QVector>

class MoleculeFileLoader
{
public:
    /// How a cif is built. Claude Generated 2026.
    struct CifOptions {
        /// The sites as written (the asymmetric unit, what Avogadro and Mercury
        /// show) when false; every symmetry image, i.e. the unit cell, when true.
        bool unitCell = false;
        int na = 1, nb = 1, nc = 1;  ///< repeats of the unit cell; unitCell only
        /// unitCell only: reassemble molecules cut by the cell faces (Mercury's
        /// "complete molecules"), each moved so its centroid lies in the cell.
        bool completeMolecules = false;
        /// Disorder: kMajorGroup = the group with the highest mean occupancy,
        /// kAllGroups = every alternative at once, n > 0 = that group.
        int disorderGroup = kMajorGroup;
        static constexpr int kMajorGroup = -1;
        static constexpr int kAllGroups = 0;
    };

    /// One disorder group (SHELX PART) of a cif.
    struct CifDisorderGroup {
        int group = 0;
        int sites = 0;
        double meanOccupancy = 0.0;
    };

    /// What a cif said about itself, so the viewer can show it next to the
    /// repeat controls. Empty (present=false) for every other format.
    /// Claude Generated 2026.
    struct CifInfo {
        bool present = false;
        double a = 0.0, b = 0.0, c = 0.0;              ///< Angstrom
        double alpha = 90.0, beta = 90.0, gamma = 90.0; ///< degrees
        bool hasCell = false;       ///< false: Cartesian file without a cell, cannot repeat
        QString spaceGroup;         ///< as the file states it; empty when absent
        int spaceGroupNumber = 0;
        int formulaUnitsZ = 0;      ///< _cell_formula_units_Z, 0 when absent
        QString formulaSum;         ///< _chemical_formula_sum, e.g. "C16 H41 Cl4 N7 Si2"
        bool completeMolecules = false; ///< what is shown was reassembled
        /// Cell vectors in Angstrom, in the frame the atoms arrive in (the cell's
        /// origin is the origin of that frame). Zero without a cell.
        QVector3D latticeA, latticeB, latticeC;
        int asymmetricAtoms = 0;    ///< sites as written in the file, all groups
        int symmetryOperations = 0; ///< operations in the file (1 = P1)
        int unitAtoms = 0;          ///< asymmetric-unit atoms of the shown selection
        int cellAtoms = 0;          ///< atoms in one unit cell of the shown selection
        bool unitCell = false;      ///< what is shown: unit cell (true) or asymmetric unit
        int na = 1, nb = 1, nc = 1; ///< repeats actually applied
        QVector<CifDisorderGroup> disorderGroups; ///< empty when ordered
        int shownGroup = 0;         ///< group shown; 0 = all alternatives (or ordered)
        int anisotropicSites = 0;   ///< sites with anisotropic displacement parameters
        int isotropicSites = 0;     ///< sites with only U_iso / B_iso
        QStringList notes;          ///< reader notes, plain words
    };

    /// Result of parsing a structure file. @a frames / @a frameBonds hold one
    /// entry per trajectory frame (xyz/vtf/pdb multi-model; mol2 and cif single
    /// frame). A cif arrives with its symmetry operations already applied.
    struct Result {
        bool supported = false;  // extension is a known structure format
        bool ok = false;         // supported AND at least one frame parsed
        QVector<QVector<moldata::Atom>> frames;
        QVector<QVector<moldata::Bond>> frameBonds;
        QString error;           // parser error message (pdb/mol2), empty otherwise
        CifInfo cif;             // filled for a cif only
        /// Claude Generated 2026 - One thermal ellipsoid per atom of frame 0 (a cif
        /// with displacement parameters); empty otherwise.
        QVector<moldata::Ellipsoid> ellipsoids;

        int frameCount() const { return frames.size(); }
    };

    /// Parse @p path into atoms/bonds. Uses stack-local parser instances (no
    /// shared state, no leak). Returns Result with ok=false on any failure.
    /// @p cif says how a cif is built -- asymmetric unit or unit cell, repeats,
    /// disorder group. Only while the file is read is that possible, since the
    /// viewer's atom records carry no cell. Ignored for every other format.
    static Result load(const QString& path, const CifOptions& cif);
    /// Same, a cif with the defaults (asymmetric unit, major disorder group).
    static Result load(const QString& path);

    /// True if @p path has an extension load() reads. The one list the file
    /// browser, drops and the lesson import consult, so a format added to load()
    /// is opened everywhere instead of only through File > Open. Claude Generated 2026.
    static bool isSupported(const QString& path);
};
