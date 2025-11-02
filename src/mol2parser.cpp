// mol2parser.cpp - MOL2 File Parser Implementation
// Claude Generated - Phase 5C: Tripos MOL2 format support

#include "mol2parser.h"
#include <QStringList>
#include <QFileInfo>
#include <QRegularExpression>

bool MOL2Parser::parseFile(const QString& filePath, MOL2Molecule& molecule)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = QString("Cannot open file: %1").arg(filePath);
        return false;
    }

    QTextStream in(&file);
    molecule.atoms.clear();
    molecule.bonds.clear();

    QString line;
    while (!in.atEnd()) {
        line = in.readLine().trimmed();

        // Look for section headers
        if (line == "@<TRIPOS>MOLECULE") {
            int atomCount = 0, bondCount = 0;
            if (!parseMoleculeSection(in, molecule, atomCount, bondCount)) {
                m_lastError = "Failed to parse MOLECULE section";
                file.close();
                return false;
            }
        } else if (line == "@<TRIPOS>ATOM") {
            if (!parseAtomSection(in, molecule, molecule.atoms.capacity())) {
                m_lastError = "Failed to parse ATOM section";
                file.close();
                return false;
            }
        } else if (line == "@<TRIPOS>BOND") {
            if (!parseBondSection(in, molecule, molecule.bonds.capacity())) {
                m_lastError = "Failed to parse BOND section";
                file.close();
                return false;
            }
        }
    }

    file.close();

    if (molecule.atoms.isEmpty()) {
        m_lastError = "No atoms found in MOL2 file";
        return false;
    }

    return true;
}

bool MOL2Parser::parseMoleculeSection(QTextStream& in, MOL2Molecule& molecule, int& atomCount, int& bondCount)
{
    QString line;

    // First line after @<TRIPOS>MOLECULE is the molecule name
    line = in.readLine();
    if (line.isEmpty()) {
        return false;
    }
    molecule.name = line.trimmed();

    // Second line: counts and type
    line = in.readLine();
    if (line.isEmpty()) {
        return false;
    }

    QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return false;
    }

    atomCount = parts[0].toInt();
    bondCount = parts.size() > 1 ? parts[1].toInt() : 0;
    molecule.type = parts.size() > 5 ? parts[5] : "SMALL";

    // Skip comment and other lines until next section
    while (!in.atEnd()) {
        qint64 pos = in.pos();
        line = in.readLine().trimmed();

        if (line.startsWith("@")) {
            // Seek back to re-read this line in main loop
            in.seek(pos);
            break;
        }

        // Try to parse as molecule type or property
        if (line == "NO_CHARGES") {
            // Skip
        } else if (!line.isEmpty() && !line.startsWith("#")) {
            molecule.comment = line;
        }
    }

    return true;
}

bool MOL2Parser::parseAtomSection(QTextStream& in, MOL2Molecule& molecule, int atomCount)
{
    QString line;
    int parsed = 0;

    while (!in.atEnd() && parsed < atomCount) {
        line = in.readLine().trimmed();

        if (line.isEmpty() || line.startsWith("@")) {
            // End of atom section
            if (line.startsWith("@")) {
                // Seek back to re-read section header
                qint64 pos = in.pos();
                in.seek(pos - line.length() - 1);
            }
            break;
        }

        if (line.startsWith("#")) {
            // Comment line
            continue;
        }

        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 5) {
            // Not enough fields
            continue;
        }

        MOL2Atom atom;
        try {
            // Fields: atom_id name x y z atom_type [charge ...]
            // atom_id is index (skip it)
            atom.name = parts[1];
            atom.x = parts[2].toFloat();
            atom.y = parts[3].toFloat();
            atom.z = parts[4].toFloat();
            atom.type = parts[5];

            // Charge is optional (field 7)
            if (parts.size() > 6) {
                atom.charge = parts[6];
            }

            molecule.atoms.append(atom);
            parsed++;

        } catch (...) {
            // Skip malformed line
            continue;
        }
    }

    return parsed > 0;
}

bool MOL2Parser::parseBondSection(QTextStream& in, MOL2Molecule& molecule, int bondCount)
{
    QString line;
    int parsed = 0;

    while (!in.atEnd()) {
        line = in.readLine().trimmed();

        if (line.isEmpty() || line.startsWith("@")) {
            // End of bond section
            break;
        }

        if (line.startsWith("#")) {
            // Comment line
            continue;
        }

        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 3) {
            // Not enough fields
            continue;
        }

        try {
            MOL2Bond bond;
            // Fields: bond_id atom1 atom2 bond_type [status ...]
            bond.atom1 = parts[1].toInt() - 1;  // Convert to 0-based
            bond.atom2 = parts[2].toInt() - 1;  // Convert to 0-based
            bond.bondType = parseBondType(parts[3]);

            // Sanity check
            if (bond.atom1 >= 0 && bond.atom2 >= 0 && bond.atom1 != bond.atom2) {
                molecule.bonds.append(bond);
                parsed++;
            }

        } catch (...) {
            // Skip malformed line
            continue;
        }
    }

    return true;  // Bonds are optional
}

int MOL2Parser::parseBondType(const QString& typeStr)
{
    if (typeStr == "1") return 1;      // Single
    if (typeStr == "2") return 2;      // Double
    if (typeStr == "3") return 3;      // Triple
    if (typeStr == "ar") return 4;     // Aromatic (use 4 to represent)
    if (typeStr == "am") return 1;     // Amide (treat as single)

    return 1;  // Default to single bond
}

QString MOL2Parser::extractElementFromSybylType(const QString& sybylType)
{
    // Sybyl types: C.ar, N.3, O.2, S.3, P.3, H, etc.
    // Extract element symbol (before the dot)

    int dotPos = sybylType.indexOf('.');
    if (dotPos > 0) {
        return sybylType.left(dotPos).toUpper();
    }

    // No dot, entire string is element symbol
    return sybylType.toUpper();
}

void MOL2Parser::convertToMoleculeViewer(const MOL2Molecule& mol2Molecule,
                                        QVector<MoleculeViewer::Atom>& atoms,
                                        QVector<MoleculeViewer::Bond>& bonds)
{
    atoms.clear();
    bonds.clear();

    // Convert atoms
    for (const MOL2Atom& mol2Atom : mol2Molecule.atoms) {
        MoleculeViewer::Atom atom;
        atom.element = extractElementFromSybylType(mol2Atom.type);
        atom.position = QVector3D(mol2Atom.x, mol2Atom.y, mol2Atom.z);
        atom.charge = 0.0f;  // Could parse from mol2Atom.charge if needed
        atoms.append(atom);
    }

    // Convert bonds
    for (const MOL2Bond& mol2Bond : mol2Molecule.bonds) {
        // Skip invalid bonds
        if (mol2Bond.atom1 < 0 || mol2Bond.atom2 < 0 ||
            mol2Bond.atom1 >= atoms.size() || mol2Bond.atom2 >= atoms.size()) {
            continue;
        }

        MoleculeViewer::Bond bond;
        bond.atom1 = mol2Bond.atom1;
        bond.atom2 = mol2Bond.atom2;
        bond.bondOrder = mol2Bond.bondType;  // 1=single, 2=double, 3=triple, 4=aromatic
        bonds.append(bond);
    }
}
