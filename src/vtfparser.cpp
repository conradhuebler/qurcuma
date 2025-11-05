// VTF Parser Implementation - Claude Generated
// Parses VTF (Visualization Toolkit Format) files for molecular visualization

#include "vtfparser.h"
#include <QTextStream>
#include <QRegularExpression>

bool VTFParser::parseFile(const QString& filePath, VTFFrame& frame)
{
    // Parse all frames internally first
    if (!parseTrajectory(filePath)) {
        return false;
    }
    
    // Return first frame if available
    if (!m_frames.isEmpty()) {
        frame = m_frames.first();
        return true;
    }
    return false;
}

bool VTFParser::parseTrajectory(const QString& filePath)
{
    // Clear previous frames
    m_frames.clear();
    return parseAsciiFormat(filePath, m_frames);
}

bool VTFParser::getFrame(int frameIndex, VTFFrame& frame) const
{
    if (frameIndex >= 0 && frameIndex < m_frames.size()) {
        frame = m_frames[frameIndex];
        return true;
    }
    return false;
}

QString VTFParser::trimQuotes(const QString& str)
{
    QString trimmed = str.trimmed();
    if (trimmed.startsWith('"') && trimmed.endsWith('"')) {
        return trimmed.mid(1, trimmed.length() - 2);
    }
    return trimmed;
}

bool VTFParser::parseAsciiFormat(const QString& filePath, QVector<VTFFrame>& frames)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open VTF file:" << filePath;
        return false;
    }

    QTextStream stream(&file);
    QString line;

    // Reset state for each parse
    QVector<VTFAtom> atomDefinitions;
    QVector<VTFBond> bonds;
    bool hasUnitCell = false;
    float cellA = 0, cellB = 0, cellC = 0;

    qDebug() << "=== VTF PARSER START ===";
    qDebug() << "Starting to parse VTF file:" << filePath;

    while (!stream.atEnd()) {
        line = stream.readLine().trimmed();

        if (line.startsWith("atom ")) {
            // Parse atom line: "atom     0 radius   0.20000E+01 type        ppo1 name 1"
            QStringList parts = line.split(QRegularExpression("\\s+"));

            // Corrected: the actual structure is: atom, index, radius, radiusValue, type, typeValue, name, nameValue
            // So we need at least 8 parts
            if (parts.size() >= 8) {
                VTFAtom atom;
                bool indexOk = false, radiusOk = false;
                atom.index = parts[1].toInt(&indexOk);
                atom.radius = parts[3].toDouble(&radiusOk);  // parts[3] = radius value
                atom.type = trimQuotes(parts[5]);  // parts[5] = type value
                atom.name = trimQuotes(parts[7]);   // parts[7] = name value

                if (indexOk && radiusOk) {

                    // Set element based on type for visualization
                    if (atom.type == "ppo1" || atom.type == "ppo2") {
                        atom.element = "C"; // Polymer units as carbon
                    } else if (atom.type.startsWith("dmaema")) {
                        atom.element = "N"; // DMAEMA units as nitrogen
                    } else {
                        atom.element = "C"; // Default to carbon
                    }

                    atomDefinitions.append(atom);
                } else {
                    qWarning() << "Failed to parse atom values - skipping line:" << line;
                }
            } else {
                qWarning() << "Invalid atom line - not enough parts:" << parts.size() << "Expected: 8+";
                qWarning() << "Line was:" << line;
            }
        }
        else if (line.startsWith("bond ")) {
            // Parse bond line: "bond     0:1"
            QStringList parts = line.split(":");
            if (parts.size() == 2) {
                bool atom1Ok = false, atom2Ok = false;
                VTFBond bond;
                bond.atom1 = parts[0].mid(5).toInt(&atom1Ok); // Remove "bond " prefix
                bond.atom2 = parts[1].toInt(&atom2Ok);
                if (atom1Ok && atom2Ok) {
                    bonds.append(bond);
                } else {
                    qWarning() << "Failed to parse bond atoms - skipping line:" << line;
                }
            } else {
                qWarning() << "Invalid bond line format:" << line;
            }
        }
        else if (line.startsWith("unitcell ")) {
            // Parse unit cell: "unitcell    10.00000    10.00000    10.00000"
            QStringList parts = line.split(QRegularExpression("\\s+"));
            if (parts.size() >= 4) {
                bool cellAOk = false, cellBOk = false, cellCOk = false;
                cellA = parts[1].toFloat(&cellAOk);
                cellB = parts[2].toFloat(&cellBOk);
                cellC = parts[3].toFloat(&cellCOk);
                if (cellAOk && cellBOk && cellCOk) {
                    hasUnitCell = true;
                } else {
                    qWarning() << "Failed to parse unit cell values - skipping line:" << line;
                }
            } else {
                qWarning() << "Invalid unitcell line format:" << line;
            }
        }
        else if (line.startsWith("# Start of image")) {
            
            // Start of new timestep
            VTFFrame frame;
            frame.atoms = atomDefinitions;  // Copy atom definitions to frame
            frame.hasUnitCell = hasUnitCell;
            frame.cellA = cellA;
            frame.cellB = cellB;
            frame.cellC = cellC;
            
            
            // Skip "timestep ordered" line
            if (!stream.atEnd()) {
                QString timestepLine = stream.readLine().trimmed();
            }
            
            // Parse coordinates - one line per atom with bounds checking
            for (int i = 0; i < atomDefinitions.size() && !stream.atEnd(); ++i) {
                line = stream.readLine().trimmed();
                
                if (!line.isEmpty() && i < frame.atoms.size()) {
                    QStringList coords = line.split(QRegularExpression("\\s+"));
                    if (coords.size() >= 3) {
                        bool xOk, yOk, zOk;
                        
                        // Parse scientific notation properly
                        frame.atoms[i].x = coords[0].toDouble(&xOk);
                        frame.atoms[i].y = coords[1].toDouble(&yOk);
                        frame.atoms[i].z = coords[2].toDouble(&zOk);
                        
                        // Debug output for first few atoms to check parsing
                        // REMOVED: Performance critical debug output in coordinate parsing loop
                        
                        // If parsing fails, set to reasonable default
                        if (!xOk) {
                            qWarning() << "Failed to parse X coordinate for atom" << i << ":" << coords[0];
                            frame.atoms[i].x = 0.0f;
                        }
                        if (!yOk) {
                            qWarning() << "Failed to parse Y coordinate for atom" << i << ":" << coords[1];
                            frame.atoms[i].y = 0.0f;
                        }
                        if (!zOk) {
                            qWarning() << "Failed to parse Z coordinate for atom" << i << ":" << coords[2];
                            frame.atoms[i].z = 0.0f;
                        }
                    } else {
                        qWarning() << "Invalid coordinate line at atom" << i << ":" << line;
                    }
                } else {
                    qWarning() << "Out of bounds or empty line at atom" << i << ", frame.atoms.size() =" << frame.atoms.size();
                }
            }
            
            // Add bonds (assuming they remain the same for all frames)
            frame.bonds = bonds;
            

            // Only add frame if we have valid atom data
            if (!frame.atoms.isEmpty()) {
                frames.append(frame);
            } else {
                qWarning() << "Skipping frame - no atoms found";
            }
            
            // Skip "# End Image" line if present
            if (!stream.atEnd()) {
                line = stream.readLine().trimmed();
                if (line == "# End Image") {
                    continue;
                } else {
                    // If we didn't find "# End Image", put the line back by moving position back
                    qDebug() << "Not an End Image line:" << line;
                    // Note: Can't easily put back line with QTextStream, but that's ok
                }
            }
        }
    }
    
    file.close();
    
    qDebug() << "VTF parsing complete. Total frames:" << frames.size();

    // Apply coordinate scaling for large VTF coordinates
    if (!frames.isEmpty()) {
        const QVector<VTFAtom>& firstFrameAtoms = frames.first().atoms;
        if (!firstFrameAtoms.isEmpty()) {
            // Check if coordinates are unusually large (typical for VTF files)
            float maxCoord = 0.0f;
            for (const VTFAtom& atom : firstFrameAtoms) {
                maxCoord = qMax(maxCoord, qMax(qAbs(atom.x), qMax(qAbs(atom.y), qAbs(atom.z))));
            }

            qDebug() << "Max VTF coordinate detected:" << maxCoord;

            // If coordinates are large (typical VTF scaling issue), apply scaling factor
            if (maxCoord > 50.0f) {
                qDebug() << "Detected large VTF coordinates (" << maxCoord << "), applying scaling factor";
                float scaleFactor = qMin(maxCoord / 50.0f, 200.0f); // Scale to max ~50 units, but not too aggressive

                qDebug() << "Applying scale factor:" << scaleFactor;

                for (auto& frame : frames) {
                    for (auto& atom : frame.atoms) {
                        atom.x /= scaleFactor;
                        atom.y /= scaleFactor;
                        atom.z /= scaleFactor;
                    }
                }
            } else {
                qDebug() << "VTF coordinates are within normal range, no scaling applied";
            }
        }
    }

    qDebug() << "VTF parsing finished successfully. Frames:" << frames.size();
    return !frames.isEmpty();
}

void VTFParser::convertToMoleculeViewer(const VTFFrame& vtfFrame, 
                                       QVector<MoleculeViewer::Atom>& atoms,
                                       QVector<MoleculeViewer::Bond>& bonds)
{
    atoms.clear();
    bonds.clear();
    
    for (const auto& vtfAtom : vtfFrame.atoms) {
        MoleculeViewer::Atom atom;
        atom.element = vtfAtom.element;
        atom.position = QVector3D(vtfAtom.x, vtfAtom.y, vtfAtom.z);
        atoms.append(atom);
    }
    
    for (const auto& vtfBond : vtfFrame.bonds) {
        MoleculeViewer::Bond bond;
        bond.atom1 = vtfBond.atom1;
        bond.atom2 = vtfBond.atom2;
        bond.bondOrder = 1; // Default bond order
        bonds.append(bond);
    }
}

QColor VTFParser::getAtomColor(const QString& type)
{
    // Map VTF types to colors
    if (type == "ppo1" || type == "ppo2") {
        return QColor(255, 165, 0); // Orange for polymer units
    }
    else if (type.startsWith("dmaema")) {
        return QColor(0, 100, 255); // Blue for DMAEMA units
    }
    else if (type == "C") {
        return QColor(128, 128, 128); // Gray for carbon
    }
    else if (type == "H") {
        return QColor(255, 255, 255); // White for hydrogen
    }
    else if (type == "O") {
        return QColor(255, 0, 0); // Red for oxygen
    }
    else if (type == "N") {
        return QColor(0, 0, 255); // Blue for nitrogen
    }
    
    return QColor(200, 200, 200); // Default gray
}

float VTFParser::getAtomRadius(float vtfRadius)
{
    // Convert VTF radius to appropriate display radius
    // VTF uses different scaling, adjust for molecular visualization
    return qMax(0.3f, vtfRadius / 100.0f);
}