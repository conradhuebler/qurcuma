// mol2parser.h - MOL2 File Parser
// Claude Generated - Phase 5C: Tripos MOL2 format support

#pragma once

#include "view.h"
#include <QString>
#include <QVector>
#include <QFile>
#include <QTextStream>
#include <QDebug>

/**
 * MOL2Parser - Reads Tripos MOL2 molecular structure files
 *
 * MOL2 Format Overview:
 * - Space-delimited text format (easier to parse than PDB)
 * - Section-based structure with @<TRIPOS>SECTION headers
 * - Common sections: MOLECULE, ATOM, BOND
 *
 * Key Sections:
 *   @<TRIPOS>MOLECULE
 *     - Name, atom count, bond count, etc.
 *
 *   @<TRIPOS>ATOM
 *     - atom_id name x y z atom_type [charge [status]]
 *     - atom_type uses Sybyl notation (C.ar, N.3, O.2, etc.)
 *
 *   @<TRIPOS>BOND
 *     - bond_id atom1 atom2 bond_type [status]
 *     - bond_type: 1 (single), 2 (double), 3 (triple), ar (aromatic)
 */
class MOL2Parser
{
public:
    struct MOL2Atom {
        QString name;             // Atom name
        QString type;             // Sybyl atom type (C.ar, N.3, etc.)
        float x, y, z;            // Coordinates
        QString charge;           // Charge information
    };

    struct MOL2Bond {
        int atom1;                // Atom index (1-based from file, will convert to 0-based)
        int atom2;                // Atom index (1-based from file)
        int bondType;             // 1=single, 2=double, 3=triple, 4=aromatic
    };

    struct MOL2Molecule {
        QString name;             // Molecule name
        QString comment;          // Comment/description
        QVector<MOL2Atom> atoms;
        QVector<MOL2Bond> bonds;
        QString type;             // Molecule type (SMALL, PROTEIN, etc.)
    };

    MOL2Parser() = default;

    /**
     * Parse MOL2 file
     * @param filePath Path to MOL2 file
     * @param molecule Output molecule structure
     * @return true if parsing succeeded
     */
    bool parseFile(const QString& filePath, MOL2Molecule& molecule);

    /**
     * Convert MOL2 data to MoleculeViewer format
     * @param mol2Molecule MOL2 molecule data
     * @param atoms Output atom array
     * @param bonds Output bond array
     */
    static void convertToMoleculeViewer(const MOL2Molecule& mol2Molecule,
                                       QVector<MoleculeViewer::Atom>& atoms,
                                       QVector<MoleculeViewer::Bond>& bonds);

    /**
     * Extract element symbol from Sybyl atom type
     * Examples: C.ar -> C, N.3 -> N, O.2 -> O, S.3 -> S
     */
    static QString extractElementFromSybylType(const QString& sybylType);

    /**
     * Get error message from last parse attempt
     */
    QString getLastError() const { return m_lastError; }

private:
    /**
     * Parse MOLECULE section
     */
    bool parseMoleculeSection(QTextStream& in, MOL2Molecule& molecule, int& atomCount, int& bondCount);

    /**
     * Parse ATOM section
     */
    bool parseAtomSection(QTextStream& in, MOL2Molecule& molecule, int atomCount);

    /**
     * Parse BOND section
     */
    bool parseBondSection(QTextStream& in, MOL2Molecule& molecule, int bondCount);

    /**
     * Convert Sybyl bond type to integer (1=single, 2=double, 3=triple, 4=aromatic)
     */
    static int parseBondType(const QString& typeStr);

    QString m_lastError;  // Error message for debugging
};
