// moleculefileloader.cpp - Unified structure-file reader implementation.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026.

#include "moleculefileloader.h"

#include "moleculebridge.h"   // moleculeToAtoms
#include "mol2parser.h"

#include <src/tools/cif.h>

#include <QDebug>
#include "pdbparser.h"
#include "vtfparser.h"
#include "xyzparser.h"

#include <QFile>
#include <QMatrix3x3>

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <QFileInfo>

namespace {

/// Principal axes of a displacement tensor. The eigenvectors become the
/// columns of the rotation (made right-handed), the square roots of the
/// eigenvalues the RMS displacements along them. A non-positive-definite
/// tensor -- a refinement problem, not a shape -- is clamped to a sliver
/// rather than dropped, so it stays visible. Claude Generated 2026.
moldata::Ellipsoid ellipsoidFrom(const Eigen::Matrix3d& u)
{
    moldata::Ellipsoid e;
    if (u.isZero(1e-12))
        return e;
    const Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> solver(u);
    Eigen::Matrix3d axes = solver.eigenvectors();
    if (axes.determinant() < 0.0)
        axes.col(2) = -axes.col(2);
    const Eigen::Vector3d values = solver.eigenvalues();
    const auto rms = [](double lambda) { return float(std::sqrt(std::max(lambda, 1e-4))); };
    e.rmsAxes = QVector3D(rms(values(0)), rms(values(1)), rms(values(2)));
    QMatrix3x3 m;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            m(row, col) = float(axes(row, col));
    e.rotation = QQuaternion::fromRotationMatrix(m);
    e.valid = true;
    return e;
}

}  // namespace

bool MoleculeFileLoader::isSupported(const QString& path)
{
    static const QStringList suffixes = { QStringLiteral("xyz"), QStringLiteral("vtf"),
        QStringLiteral("pdb"), QStringLiteral("mol2"), QStringLiteral("cif") };
    return suffixes.contains(QFileInfo(path).suffix().toLower());
}

MoleculeFileLoader::Result MoleculeFileLoader::load(const QString& path)
{
    return load(path, CifOptions());
}

MoleculeFileLoader::Result MoleculeFileLoader::load(const QString& path, const CifOptions& options)
{
    Result r;
    if (path.isEmpty() || !QFile::exists(path))
        return r;

    const QString suffix = QFileInfo(path).suffix().toLower();

    if (suffix == "xyz") {
        r.supported = true;
        XYZParser parser;
        if (parser.parseTrajectory(path)) {
            const int n = parser.getFrameCount();
            for (int i = 0; i < n; ++i) {
                XYZParser::XYZFrame frame;
                if (!parser.getFrame(i, frame))
                    continue;
                QVector<moldata::Atom> atoms;
                QVector<moldata::Bond> bonds;
                XYZParser::convertToMoleculeViewer(frame, atoms, bonds);
                r.frames.append(atoms);
                r.frameBonds.append(bonds);
            }
        }
    } else if (suffix == "vtf") {
        r.supported = true;
        VTFParser parser;
        if (parser.parseTrajectory(path)) {
            const int n = parser.getFrameCount();
            for (int i = 0; i < n; ++i) {
                VTFParser::VTFFrame frame;
                if (!parser.getFrame(i, frame))
                    continue;
                QVector<moldata::Atom> atoms;
                QVector<moldata::Bond> bonds;
                VTFParser::convertToMoleculeViewer(frame, atoms, bonds);
                r.frames.append(atoms);
                r.frameBonds.append(bonds);
            }
        }
    } else if (suffix == "pdb") {
        r.supported = true;
        PDBParser parser;
        PDBParser::PDBFrame frame;
        if (parser.parseFile(path, frame)) {
            QVector<moldata::Atom> atoms;
            QVector<moldata::Bond> bonds;
            PDBParser::convertToMoleculeViewer(frame, atoms, bonds, parser.getBonds());
            r.frames.append(atoms);
            r.frameBonds.append(bonds);
        } else {
            r.error = parser.getLastError();
        }
    } else if (suffix == "mol2") {
        r.supported = true;
        MOL2Parser parser;
        MOL2Parser::MOL2Molecule molecule;
        if (parser.parseFile(path, molecule)) {
            QVector<moldata::Atom> atoms;
            QVector<moldata::Bond> bonds;
            MOL2Parser::convertToMoleculeViewer(molecule, atoms, bonds);
            r.frames.append(atoms);
            r.frameBonds.append(bonds);
        } else {
            r.error = parser.getLastError();
        }
    } else if (suffix == "cif") {
        // Claude Generated 2026 - Read through curcuma: a CIF is a unit cell plus an
        // asymmetric unit plus symmetry operations, and reimplementing that here
        // would be a second reader to keep in step with the first. What comes back
        // is the asymmetric unit or the unit cell, one disorder group or all, as
        // the options ask.
        r.supported = true;
        const curcuma::CifData cif = curcuma::ReadCifData(path.toStdString());
        if (!cif.ok()) {
            r.error = QString::fromStdString(cif.error);
        } else {
            CifInfo& info = r.cif;
            info.present = true;
            info.hasCell = cif.cell.valid;
            info.a = cif.cell.a;
            info.b = cif.cell.b;
            info.c = cif.cell.c;
            info.alpha = cif.cell.alpha;
            info.beta = cif.cell.beta;
            info.gamma = cif.cell.gamma;
            if (info.hasCell) {
                const auto column = [&cif](int i) {
                    return QVector3D(float(cif.cell.lattice(0, i)), float(cif.cell.lattice(1, i)),
                        float(cif.cell.lattice(2, i)));
                };
                info.latticeA = column(0);
                info.latticeB = column(1);
                info.latticeC = column(2);
            }
            info.spaceGroup = QString::fromStdString(cif.space_group);
            info.spaceGroupNumber = cif.space_group_number;
            info.formulaUnitsZ = cif.formula_units_z;
            info.formulaSum = QString::fromStdString(cif.formula_sum);
            info.asymmetricAtoms = int(cif.sites.size());
            for (const curcuma::CifSite& site : cif.sites) {
                if (site.has_aniso)
                    ++info.anisotropicSites;
                else if (site.has_iso)
                    ++info.isotropicSites;
            }
            info.symmetryOperations = int(cif.operations.size());
            for (const curcuma::CifDisorderGroup& group : cif.disorder_groups)
                info.disorderGroups.append({ group.group, group.sites, group.mean_occupancy });
            for (const std::string& note : cif.notes) {
                info.notes << QString::fromStdString(note);
                qWarning().noquote() << "cif:" << QString::fromStdString(note);
            }

            // Claude Generated 2026 - Which disorder group: the major one unless
            // told otherwise; an ordered structure has none to choose.
            int group = options.disorderGroup;
            if (group == CifOptions::kMajorGroup)
                group = cif.majorDisorderGroup();
            if (cif.disorder_groups.empty())
                group = CifOptions::kAllGroups;
            info.shownGroup = group;

            curcuma::CifBuildOptions build;
            build.select_disorder_group = group != CifOptions::kAllGroups;
            build.disorder_group = group;
            build.content = curcuma::CifContent::AsymmetricUnit;
            std::vector<Eigen::Matrix3d> unitU;
            const curcuma::Molecule unit = curcuma::BuildCif(cif, build, &unitU);
            build.content = curcuma::CifContent::UnitCell;
            build.complete_molecules = options.completeMolecules;
            info.completeMolecules = options.completeMolecules && options.unitCell;
            std::vector<Eigen::Matrix3d> cellU;
            const curcuma::Molecule cell = curcuma::BuildCif(cif, build, &cellU);
            info.unitAtoms = int(unit.AtomCount());
            info.cellAtoms = int(cell.AtomCount());
            // Without a cell there is no unit cell to show, only the file's atoms.
            info.unitCell = options.unitCell && info.hasCell;

            curcuma::Molecule molecule = info.unitCell ? cell : unit;
            std::vector<Eigen::Matrix3d> displacements = info.unitCell ? cellU : unitU;
            const int na = qMax(1, options.na);
            const int nb = qMax(1, options.nb);
            const int nc = qMax(1, options.nc);
            if (info.unitCell && (na > 1 || nb > 1 || nc > 1)) {
                std::string cellError;
                curcuma::Molecule big = curcuma::Supercell(cell, na, nb, nc, &cellError);
                if (cellError.empty()) {
                    molecule = big;
                    info.na = na;
                    info.nb = nb;
                    info.nc = nc;
                    // Supercell() copies the cell's atoms block by block, in
                    // order; the tensors follow the same way (a translation does
                    // not turn them).
                    std::vector<Eigen::Matrix3d> repeated;
                    repeated.reserve(displacements.size() * size_t(na * nb * nc));
                    for (int copy = 0; copy < na * nb * nc; ++copy)
                        repeated.insert(repeated.end(), displacements.begin(), displacements.end());
                    displacements = repeated;
                } else {
                    info.notes << QString::fromStdString(cellError);
                }
            }
            r.frames.append(moleculeToAtoms(molecule));
            r.frameBonds.append(QVector<moldata::Bond>());   // detected from the geometry
            if (info.anisotropicSites + info.isotropicSites > 0
                && displacements.size() == size_t(molecule.AtomCount())) {
                r.ellipsoids.reserve(int(displacements.size()));
                for (const Eigen::Matrix3d& u : displacements)
                    r.ellipsoids.append(ellipsoidFrom(u));
            }
        }
    }

    r.ok = !r.frames.isEmpty();
    return r;
}
