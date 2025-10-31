#include <QCoreApplication>
#include <QString>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

// Simplified VTF parser to test Qt QString handling
bool parseVTFCore(const QString& filePath, QString& errorMsg) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = "Failed to open VTF file: " + filePath;
        return false;
    }

    QTextStream stream(&file);
    QString line;
    
    QVector<QString> atomDefinitions;
    bool parseError = false;
    int frameCount = 0;

    qDebug() << "Starting VTF parsing with Qt...";

    while (!stream.atEnd() && !parseError) {
        line = stream.readLine().trimmed();
        
        if (line.startsWith("atom ")) {
            qDebug() << "Found atom definition:" << line.left(50);
            atomDefinitions.append(line);
        }
        else if (line.startsWith("# Start of image")) {
            qDebug() << "Found image section:" << line;
            frameCount++;
            
            // Skip "timestep ordered" line
            if (!stream.atEnd()) {
                QString timestepLine = stream.readLine().trimmed();
                qDebug() << "Skipping timestep line:" << timestepLine;
            }
            
            // Parse a few coordinates to test
            for (int i = 0; i < 3 && !stream.atEnd(); ++i) {
                line = stream.readLine().trimmed();
                if (!line.isEmpty()) {
                    QStringList coords = line.split(QRegularExpression("\\s+"));
                    if (coords.size() >= 3) {
                        bool xOk, yOk, zOk;
                        double x = coords[0].toDouble(&xOk);
                        double y = coords[1].toDouble(&yOk);
                        double z = coords[2].toDouble(&zOk);
                        
                        qDebug() << "Coords" << i << ":" << coords[0] << "->" << x << "OK:" << xOk
                                 << coords[1] << "->" << y << "OK:" << yOk
                                 << coords[2] << "->" << z << "OK:" << zOk;
                    } else {
                        qDebug() << "Invalid coords line:" << line;
                    }
                }
            }
            
            qDebug() << "Frame" << (frameCount-1) << "complete";
        }
    }
    
    file.close();
    
    qDebug() << "VTF parsing complete. Total frames:" << frameCount;
    qDebug() << "Atom definitions found:" << atomDefinitions.size();
    
    return frameCount > 0 && !parseError;
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    QString errorMsg;
    bool success = parseVTFCore("scnp_cut.vtf", errorMsg);
    
    if (success) {
        qDebug() << "=== Qt VTF PARSING SUCCESSFUL ===";
        return 0;
    } else {
        qDebug() << "=== Qt VTF PARSING FAILED ===" << errorMsg;
        return 1;
    }
}