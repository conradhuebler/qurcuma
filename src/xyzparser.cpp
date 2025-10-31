// XYZ Parser Implementation - Claude Generated
// Parses XYZ files for molecular visualization with trajectory support

#include "xyzparser.h"
#include <QRegularExpression>

bool XYZParser::parseFile(const QString& filePath, XYZFrame& frame)
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

bool XYZParser::parseTrajectory(const QString& filePath)
{
    // Clear previous frames
    m_frames.clear();
    return parseAsciiFormat(filePath, m_frames);
}

bool XYZParser::getFrame(int frameIndex, XYZFrame& frame) const
{
    if (frameIndex >= 0 && frameIndex < m_frames.size()) {
        frame = m_frames[frameIndex];
        return true;
    }
    return false;
}

bool XYZParser::parseAsciiFormat(const QString& filePath, QVector<XYZFrame>& frames)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open XYZ file:" << filePath;
        return false;
    }

    QTextStream stream(&file);
    QString line;
    
    while (!stream.atEnd()) {
        // Read atom count for this frame
        line = stream.readLine().trimmed();
        if (line.isEmpty()) {
            continue; // Skip empty lines
        }
        
        bool ok;
        int numAtoms = line.toInt(&ok);
        if (!ok || numAtoms <= 0) {
            qWarning() << "Invalid atom count in XYZ file:" << line;
            continue;
        }
        
        // Read comment line
        QString comment = stream.readLine().trimmed();
        
        // Create new frame
        XYZFrame frame;
        frame.comment = comment;
        frame.atoms.reserve(numAtoms);
        
        // Read atoms for this frame
        for (int i = 0; i < numAtoms && !stream.atEnd(); ++i) {
            line = stream.readLine().trimmed();
            if (!line.isEmpty()) {
                QStringList parts = line.split(QRegularExpression("\\s+"));
                if (parts.size() >= 4) {
                    XYZAtom atom;
                    atom.element = parts[0];
                    atom.x = parts[1].toFloat();
                    atom.y = parts[2].toFloat();
                    atom.z = parts[3].toFloat();
                    frame.atoms.append(atom);
                } else {
                    qWarning() << "Invalid atom line in XYZ file:" << line;
                }
            }
        }
        
        // Only add frame if we successfully read all atoms
        if (frame.atoms.size() == numAtoms) {
            frames.append(frame);
        } else {
            qWarning() << "Incomplete frame in XYZ file. Expected" << numAtoms << "atoms, got" << frame.atoms.size();
        }
    }
    
    file.close();
    return !frames.isEmpty();
}

void XYZParser::convertToMoleculeViewer(const XYZFrame& xyzFrame, 
                                       QVector<MoleculeViewer::Atom>& atoms,
                                       QVector<MoleculeViewer::Bond>& bonds)
{
    atoms.clear();
    bonds.clear();
    
    for (const auto& xyzAtom : xyzFrame.atoms) {
        MoleculeViewer::Atom atom;
        atom.element = xyzAtom.element;
        atom.position = QVector3D(xyzAtom.x, xyzAtom.y, xyzAtom.z);
        atoms.append(atom);
    }
    
    // Note: XYZ files typically don't contain bond information
    // Bonds will be auto-detected by MoleculeViewer if needed
}