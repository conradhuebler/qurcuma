// pdbparser.cpp - PDB File Parser Implementation
// Claude Generated - Phase 5C: Protein Data Bank format support

#include "pdbparser.h"
#include <QStringList>
#include <QFileInfo>
#include <cmath>

bool PDBParser::parseFile(const QString& filePath, PDBFrame& frame)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = QString("Cannot open file: %1").arg(filePath);
        return false;
    }

    m_frames.clear();
    m_bonds.clear();

    QTextStream in(&file);
    PDBFrame currentFrame;
    currentFrame.title = "";
    currentFrame.remark = "";

    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.isEmpty()) continue;

        QString record = line.mid(0, 6).trimmed();  // Record name

        if (record == "TITLE") {
            // Extract title (columns 11-80)
            currentFrame.title += line.mid(10);
        } else if (record == "REMARK") {
            // Store remark
            if (!currentFrame.remark.isEmpty()) {
                currentFrame.remark += "\n";
            }
            currentFrame.remark += line.mid(10);
        } else if (record == "ATOM" || record == "HETATM") {
            // Parse atom record
            PDBAtom atom;
            if (parseAtomRecord(line, atom)) {
                currentFrame.atoms.append(atom);
            }
        } else if (record == "CONECT") {
            // Parse connectivity record
            parseConectRecord(line);
        } else if (record == "ENDMDL") {
            // End of model - save and start new
            if (!currentFrame.atoms.isEmpty()) {
                m_frames.append(currentFrame);
                currentFrame.atoms.clear();
            }
        } else if (record == "END") {
            // End of file
            break;
        }
    }

    // Save last frame if no ENDMDL was encountered
    if (!currentFrame.atoms.isEmpty()) {
        m_frames.append(currentFrame);
    }

    file.close();

    if (m_frames.isEmpty()) {
        m_lastError = "No atoms found in PDB file";
        return false;
    }

    // If no CONECT records, try distance-based detection
    if (m_bonds.isEmpty()) {
        detectBonds(m_frames[0]);
    }

    frame = m_frames[0];
    return true;
}

bool PDBParser::parseTrajectory(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = QString("Cannot open file: %1").arg(filePath);
        return false;
    }

    m_frames.clear();
    m_bonds.clear();

    QTextStream in(&file);
    PDBFrame currentFrame;

    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.isEmpty()) continue;

        QString record = line.mid(0, 6).trimmed();

        if (record == "ATOM" || record == "HETATM") {
            PDBAtom atom;
            if (parseAtomRecord(line, atom)) {
                currentFrame.atoms.append(atom);
            }
        } else if (record == "CONECT") {
            parseConectRecord(line);
        } else if (record == "ENDMDL") {
            if (!currentFrame.atoms.isEmpty()) {
                m_frames.append(currentFrame);
                currentFrame.atoms.clear();
            }
        } else if (record == "END") {
            break;
        }
    }

    // Save last frame
    if (!currentFrame.atoms.isEmpty()) {
        m_frames.append(currentFrame);
    }

    file.close();

    if (m_frames.isEmpty()) {
        m_lastError = "No atoms found in PDB file";
        return false;
    }

    // Distance-based bond detection for first frame
    if (m_bonds.isEmpty()) {
        detectBonds(m_frames[0]);
    }

    return true;
}

bool PDBParser::getFrame(int frameIndex, PDBFrame& frame) const
{
    if (frameIndex < 0 || frameIndex >= m_frames.size()) {
        return false;
    }
    frame = m_frames[frameIndex];
    return true;
}

bool PDBParser::parseAtomRecord(const QString& line, PDBAtom& atom)
{
    // PDB format uses fixed columns (1-based in spec, 0-based in code)
    if (line.length() < 54) {  // Minimum length for coordinates
        return false;
    }

    try {
        // Serial number (columns 7-11)
        int serial = line.mid(6, 5).trimmed().toInt();

        // Atom name (columns 13-16) - includes space for chirality
        QString atomName = line.mid(12, 4).trimmed();

        // Alternate location (column 17) - skip if not blank or 'A'
        QChar altLoc = line.length() > 16 ? line[16] : ' ';
        if (altLoc != ' ' && altLoc != 'A') {
            return false;  // Skip alternate conformations
        }

        // Residue name (columns 18-20)
        QString residueName = line.mid(17, 3).trimmed();

        // Chain identifier (column 22)
        char chain = line.length() > 21 ? line[21].toLatin1() : ' ';

        // Residue number (columns 23-26)
        int residueNumber = line.mid(22, 4).trimmed().toInt();

        // Coordinates (columns 31-38, 39-46, 47-54)
        float x = line.mid(30, 8).trimmed().toFloat();
        float y = line.mid(38, 8).trimmed().toFloat();
        float z = line.mid(46, 8).trimmed().toFloat();

        // Occupancy (columns 55-60) - default 1.0
        float occupancy = 1.0f;
        if (line.length() >= 60) {
            QString occStr = line.mid(54, 6).trimmed();
            if (!occStr.isEmpty()) {
                occupancy = occStr.toFloat();
            }
        }

        // Temperature factor (columns 61-66) - default 0.0
        float temperature = 0.0f;
        if (line.length() >= 66) {
            QString tempStr = line.mid(60, 6).trimmed();
            if (!tempStr.isEmpty()) {
                temperature = tempStr.toFloat();
            }
        }

        // Element symbol (columns 77-78) - try explicit, otherwise derive from name
        QString element = "";
        if (line.length() >= 78) {
            element = line.mid(76, 2).trimmed();
        }
        if (element.isEmpty()) {
            element = extractElementSymbol(atomName);
        }

        atom.element = element;
        atom.name = atomName;
        atom.residueName = residueName;
        atom.chain = chain;
        atom.residueNumber = residueNumber;
        atom.x = x;
        atom.y = y;
        atom.z = z;
        atom.occupancy = occupancy;
        atom.temperature = temperature;

        return true;

    } catch (...) {
        return false;
    }
}

void PDBParser::parseConectRecord(const QString& line)
{
    // CONECT records: columns 7-11 (main atom), then pairs of columns 12-16, 17-21, 22-26, 27-31
    if (line.length() < 11) return;

    int atom1 = line.mid(6, 5).trimmed().toInt();

    // Parse bonded atom indices
    QVector<int> bonded;
    for (int i = 0; i < 4; ++i) {
        int startCol = 11 + (i * 5);
        if (line.length() > startCol + 5) {
            QString bondStr = line.mid(startCol, 5).trimmed();
            if (!bondStr.isEmpty()) {
                int atom2 = bondStr.toInt();
                if (atom2 > 0 && atom2 != atom1) {
                    bonded.append(atom2);
                }
            }
        }
    }

    // Add bonds (avoid duplicates)
    for (int atom2 : bonded) {
        PDBBond bond;
        bond.atom1 = qMin(atom1, atom2);
        bond.atom2 = qMax(atom1, atom2);

        // Check if bond already exists
        bool exists = false;
        for (const PDBBond& existing : m_bonds) {
            if (existing.atom1 == bond.atom1 && existing.atom2 == bond.atom2) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            m_bonds.append(bond);
        }
    }
}

QString PDBParser::extractElementSymbol(const QString& atomName, const QString& explicit_element)
{
    if (!explicit_element.isEmpty()) {
        return explicit_element;
    }

    // Heuristic: first 1-2 characters are usually element symbol
    // Handle common cases: CA->C, CB->C, OG->O, ND1->N, etc.
    if (atomName.length() < 2) {
        return atomName.toUpper();
    }

    QString first = atomName[0].toUpper();
    QString second = atomName[1].toLower();

    // Two-letter elements
    QStringList twoLetterElements = {"He", "Li", "Be", "Ne", "Na", "Mg", "Al", "Si", "Cl", "Ar", "Ca", "Ti", "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn", "Br", "Kr", "Sr", "Zr", "Ag", "Cd", "Sn", "Xe", "Ba", "Pt", "Au", "Hg", "Pb"};

    QString twoLetter = first + second;
    if (twoLetterElements.contains(twoLetter)) {
        return twoLetter;
    }

    // Single-letter elements
    return first;
}

float PDBParser::getCovalentRadius(const QString& element)
{
    // Covalent radii (in Angstroms) from CRC Handbook
    static const QMap<QString, float> radii = {
        {"H", 0.31f},  {"C", 0.76f},  {"N", 0.71f},  {"O", 0.66f},
        {"P", 1.07f},  {"S", 1.05f},  {"Cl", 1.02f}, {"Br", 1.20f},
        {"I", 1.39f},  {"Fe", 1.32f}, {"Cu", 1.32f}, {"Zn", 1.22f},
        {"Se", 1.22f}, {"As", 1.19f}, {"F", 0.64f},  {"B", 0.84f},
    };

    return radii.value(element, 1.5f);  // Default 1.5Å for unknown elements
}

void PDBParser::detectBonds(const PDBFrame& frame)
{
    m_bonds.clear();

    const float BOND_DISTANCE_CUTOFF = 1.8f;  // Angstroms

    for (int i = 0; i < frame.atoms.size(); ++i) {
        for (int j = i + 1; j < frame.atoms.size(); ++j) {
            const PDBAtom& a1 = frame.atoms[i];
            const PDBAtom& a2 = frame.atoms[j];

            // Skip bonds between different chains
            if (a1.chain != a2.chain && a1.chain != ' ' && a2.chain != ' ') {
                continue;
            }

            // Calculate distance
            float dx = a2.x - a1.x;
            float dy = a2.y - a1.y;
            float dz = a2.z - a1.z;
            float distance = std::sqrt(dx*dx + dy*dy + dz*dz);

            // Sum of covalent radii + threshold
            float maxDist = getCovalentRadius(a1.element) + getCovalentRadius(a2.element) + 0.4f;

            if (distance < maxDist && distance > 0.1f) {  // >0.1 to avoid zero distance
                PDBBond bond;
                bond.atom1 = i;
                bond.atom2 = j;
                m_bonds.append(bond);
            }
        }
    }
}

void PDBParser::convertToMoleculeViewer(const PDBFrame& pdbFrame,
                                       QVector<MoleculeViewer::Atom>& atoms,
                                       QVector<MoleculeViewer::Bond>& bonds,
                                       const QVector<PDBBond>& pdbBonds)
{
    atoms.clear();
    bonds.clear();

    // Convert atoms
    for (const PDBAtom& pdbAtom : pdbFrame.atoms) {
        MoleculeViewer::Atom atom;
        atom.element = pdbAtom.element;
        atom.position = QVector3D(pdbAtom.x, pdbAtom.y, pdbAtom.z);
        atom.charge = 0.0f;  // PDB doesn't typically contain charge
        atoms.append(atom);
    }

    // Convert bonds
    for (const PDBBond& pdbBond : pdbBonds) {
        MoleculeViewer::Bond bond;
        bond.atom1 = pdbBond.atom1;
        bond.atom2 = pdbBond.atom2;
        bond.bondOrder = 1;  // Default single bond
        bonds.append(bond);
    }
}
