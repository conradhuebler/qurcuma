#include <QCoreApplication>
#include <QString>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QVector>
#include <QMap>

// Reproduce the exact parsing logic from vtfparser.cpp
struct TestAtom {
    int index;
    QString type;
    QString name;
    QString element;
    double x, y, z;
};

struct TestBond {
    int atom1, atom2;
};

struct TestFrame {
    QVector<TestAtom> atoms;
    QVector<TestBond> bonds;
};

QString trimQuotes(const QString& str) {
    QString trimmed = str.trimmed();
    if (trimmed.startsWith('"') && trimmed.endsWith('"')) {
        return trimmed.mid(1, trimmed.length() - 2);
    }
    return trimmed;
}

bool parseVTFCore(const QString& filePath, QVector<TestFrame>& frames, QString& errorMsg) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = "Failed to open VTF file: " + filePath;
        return false;
    }

    QTextStream stream(&file);
    QString line;
    
    // Reset state for each parse
    QVector<TestAtom> atomDefinitions;
    QMap<int, TestBond> bonds;
    bool parseError = false;

    qDebug() << "=== QT VTF PARSER START ===";
    qDebug() << "Starting to parse VTF file:" << filePath;

    while (!stream.atEnd() && !parseError) {
        line = stream.readLine().trimmed();
        
        if (line.startsWith("atom ")) {
            // Parse atom line: "atom     0 radius   0.20000E+01 type        ppo1 name 1"
            QStringList parts = line.split(QRegularExpression("\\s+"));
            qDebug() << "Parsing atom line:" << line.left(60) << "Parts count:" << parts.size();
            
            if (parts.size() >= 9) {
                TestAtom atom;
                atom.index = parts[1].toInt(&parseError);
                atom.x = atom.y = atom.z = 0.0; // Will be set from trajectory
                atom.type = trimQuotes(parts[6]);  // "ppo1" or "ppo2" etc.
                atom.name = trimQuotes(parts[8]);   // "1", "2", "3", "4"
                
                // Set element based on type for visualization
                if (atom.type == "ppo1" || atom.type == "ppo2") {
                    atom.element = "C"; // Polymer units as carbon
                } else if (atom.type.startsWith("dmaema")) {
                    atom.element = "N"; // DMAEMA units as nitrogen
                } else {
                    atom.element = "C"; // Default to carbon
                }
                
                atomDefinitions.append(atom);
                qDebug() << "Added atom" << atom.index << "to definitions, total:" << atomDefinitions.size();
            } else {
                qWarning() << "Invalid atom line - not enough parts:" << parts.size() << "Expected: 9+";
                parseError = true;
            }
        }
        else if (line.startsWith("# Start of image")) {
            qDebug() << "Found image section:" << line;
            qDebug() << "Found" << atomDefinitions.size() << "atom definitions";
            
            // Start of new timestep
            TestFrame frame;
            frame.atoms = atomDefinitions;  // Copy atom definitions to frame
            qDebug() << "Frame initialized with" << frame.atoms.size() << "atoms";
            
            // Skip "timestep ordered" line
            if (!stream.atEnd()) {
                QString timestepLine = stream.readLine().trimmed();
                qDebug() << "Skipping timestep line:" << timestepLine;
            }
            
            // Parse coordinates - one line per atom with bounds checking
            for (int i = 0; i < atomDefinitions.size() && !stream.atEnd(); ++i) {
                line = stream.readLine().trimmed();
                qDebug() << "Reading coordinate line" << i << ":" << line.left(50);
                
                if (!line.isEmpty() && i < frame.atoms.size()) {
                    QStringList coords = line.split(QRegularExpression("\\s+"));
                    if (coords.size() >= 3) {
                        bool xOk, yOk, zOk;
                        
                        // Parse scientific notation properly
                        frame.atoms[i].x = coords[0].toDouble(&xOk);
                        frame.atoms[i].y = coords[1].toDouble(&yOk);
                        frame.atoms[i].z = coords[2].toDouble(&zOk);
                        
                        // Debug output for first few atoms to check parsing
                        if (i < 3) {
                            qDebug() << "VTF Atom" << i << "coords:" << coords[0] << "->" << frame.atoms[i].x 
                                     << coords[1] << "->" << frame.atoms[i].y 
                                     << coords[2] << "->" << frame.atoms[i].z
                                     << "ParseOK:" << xOk << yOk << zOk;
                        }
                        
                        // If parsing fails, set to reasonable default
                        if (!xOk) frame.atoms[i].x = 0.0;
                        if (!yOk) frame.atoms[i].y = 0.0;
                        if (!zOk) frame.atoms[i].z = 0.0;
                    } else {
                        qWarning() << "Invalid coordinate line at atom" << i << ":" << line;
                    }
                } else {
                    qWarning() << "Out of bounds or empty line at atom" << i << ", frame.atoms.size() =" << frame.atoms.size();
                }
            }
            
            qDebug() << "Frame parsing complete. Atoms:" << frame.atoms.size() << "Bonds:" << frame.bonds.size();
            
            // Only add frame if we have valid atom data and no parsing errors
            if (!frame.atoms.isEmpty() && !parseError) {
                frames.append(frame);
                qDebug() << "Successfully added frame" << (frames.size()-1) << "with" << frame.atoms.size() << "atoms";
            } else {
                qWarning() << "Skipping frame - atoms:" << frame.atoms.size() << "parseError:" << parseError;
            }
            
            // Skip any "# End Image" line if present
            if (!stream.atEnd()) {
                QString endLine = stream.readLine().trimmed();
                qDebug() << "Checking for End Image:" << endLine;
            }
        }
    }
    
    file.close();
    
    qDebug() << "VTF parsing complete. Total frames:" << frames.size();
    qDebug() << "Atom definitions found:" << atomDefinitions.size();
    
    return !frames.isEmpty() && !parseError;
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    QVector<TestFrame> frames;
    QString errorMsg;
    bool success = parseVTFCore("scnp_cut.vtf", frames, errorMsg);
    
    if (success) {
        qDebug() << "=== Qt VTF PARSING SUCCESSFUL ===";
        qDebug() << "Parsed" << frames.size() << "frames";
        for (int i = 0; i < frames.size(); i++) {
            qDebug() << "Frame" << i << "has" << frames[i].atoms.size() << "atoms";
            if (!frames[i].atoms.isEmpty()) {
                qDebug() << "First atom coords:" << frames[i].atoms[0].x << frames[i].atoms[0].y << frames[i].atoms[0].z;
            }
        }
        return 0;
    } else {
        qDebug() << "=== Qt VTF PARSING FAILED ===" << errorMsg;
        return 1;
    }
}