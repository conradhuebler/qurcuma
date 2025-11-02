// pdbparser.h - PDB File Parser
// Claude Generated - Phase 5C: Protein Data Bank format support

#pragma once

#include "view.h"
#include <QString>
#include <QVector>
#include <QFile>
#include <QTextStream>
#include <QDebug>

/**
 * PDBParser - Reads Protein Data Bank (PDB) format files
 *
 * PDB Format Details:
 * - Fixed-width columns (each field has specific column positions)
 * - ATOM records (residues) at columns 1-6, 7-11 (serial), 13-16 (name), etc.
 * - HETATM records (heteroatoms) follow same format as ATOM
 * - CONECT records provide explicit bond connectivity
 * - Multi-model files contain multiple structures (NMR ensembles)
 *
 * Record Format (key fields):
 *   Columns 1-6:     Record name (ATOM or HETATM)
 *   Columns 7-11:    Atom serial number
 *   Columns 13-16:   Atom name
 *   Column 17:       Alternate location indicator
 *   Columns 18-20:   Residue name
 *   Column 22:       Chain identifier
 *   Columns 23-26:   Residue sequence number
 *   Columns 31-38:   X coordinate
 *   Columns 39-46:   Y coordinate
 *   Columns 47-54:   Z coordinate
 *   Columns 55-60:   Occupancy
 *   Columns 61-66:   Temperature factor
 *   Columns 77-78:   Element symbol
 */
class PDBParser
{
public:
    struct PDBAtom {
        QString element;          // Element symbol (H, C, N, O, etc.)
        QString name;             // Atom name (CA, CB, O, etc.)
        QString residueName;      // 3-letter residue code (ALA, GLY, etc.)
        char chain;               // Chain identifier (A, B, etc.)
        int residueNumber;        // Residue sequence number
        float x, y, z;            // Coordinates in Angstroms
        float occupancy;          // Occupancy factor (0-1)
        float temperature;        // B-factor (temperature factor)
    };

    struct PDBFrame {
        QVector<PDBAtom> atoms;
        QString title;            // TITLE record from file
        QString remark;           // REMARK records
    };

    struct PDBBond {
        int atom1;                // Serial number of first atom
        int atom2;                // Serial number of second atom
    };

    PDBParser() = default;

    /**
     * Parse single model from PDB file
     * @param filePath Path to PDB file
     * @param frame Output frame (first model if multi-model)
     * @return true if parsing succeeded
     */
    bool parseFile(const QString& filePath, PDBFrame& frame);

    /**
     * Parse all models from PDB file (for NMR ensembles)
     * @param filePath Path to PDB file
     * @return true if parsing succeeded
     */
    bool parseTrajectory(const QString& filePath);

    /**
     * Get number of models in file
     */
    int getFrameCount() const { return m_frames.size(); }

    /**
     * Get frame by model index
     * @param frameIndex Model number (0-based)
     * @param frame Output frame
     * @return true if frame exists
     */
    bool getFrame(int frameIndex, PDBFrame& frame) const;

    /**
     * Get bonds from CONECT records or distance-based detection
     * @return Vector of bonds (atom serial numbers)
     */
    const QVector<PDBBond>& getBonds() const { return m_bonds; }

    /**
     * Convert PDB data to MoleculeViewer format
     * @param pdbFrame PDB frame data
     * @param atoms Output atom array
     * @param bonds Output bond array
     */
    static void convertToMoleculeViewer(const PDBFrame& pdbFrame,
                                       QVector<MoleculeViewer::Atom>& atoms,
                                       QVector<MoleculeViewer::Bond>& bonds,
                                       const QVector<PDBBond>& pdbBonds);

    /**
     * Detect bonds based on distance (<1.8 Å)
     * Covalent radii from CRC Handbook
     */
    void detectBonds(const PDBFrame& frame);

    /**
     * Get error message from last parse attempt
     */
    QString getLastError() const { return m_lastError; }

private:
    /**
     * Parse ATOM/HETATM record
     * Fixed-width format per PDB specification
     */
    bool parseAtomRecord(const QString& line, PDBAtom& atom);

    /**
     * Parse CONECT (connectivity) record
     */
    void parseConectRecord(const QString& line);

    /**
     * Extract element symbol from atom name
     * Uses heuristics if not provided in columns 77-78
     */
    QString extractElementSymbol(const QString& atomName, const QString& explicit_element = "");

    /**
     * Get covalent radius for element (in Angstroms)
     */
    static float getCovalentRadius(const QString& element);

    QVector<PDBFrame> m_frames;   // All models from file
    QVector<PDBBond> m_bonds;     // Explicit bonds from CONECT records
    QString m_lastError;          // Error message for debugging
};
