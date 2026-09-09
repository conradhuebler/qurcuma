// moleculefileloader.cpp - Unified structure-file reader implementation.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026.

#include "moleculefileloader.h"

#include "mol2parser.h"
#include "pdbparser.h"
#include "vtfparser.h"
#include "xyzparser.h"

#include <QFile>
#include <QFileInfo>

MoleculeFileLoader::Result MoleculeFileLoader::load(const QString& path)
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
    }

    r.ok = !r.frames.isEmpty();
    return r;
}
