// Minimal VTF Test - Claude Generated
// Debug segmentation fault in VTF parsing

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qDebug() << "Minimal VTF Test";
    qDebug() << "===============";
    
    QString vtfFilePath = "/home/conrad/src/qurcuma/scnp_cut.vtf";
    
    qDebug() << "Checking file existence...";
    if (!QFile::exists(vtfFilePath)) {
        qWarning() << "VTF test file not found:" << vtfFilePath;
        return 1;
    }
    
    qDebug() << "File exists, attempting to open...";
    QFile file(vtfFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << vtfFilePath;
        return 1;
    }
    
    qDebug() << "File opened successfully, size:" << file.size() << "bytes";
    
    qDebug() << "Reading first 10 lines...";
    QTextStream stream(&file);
    QString line;
    int lineCount = 0;
    
    while (!stream.atEnd() && lineCount < 10) {
        line = stream.readLine();
        lineCount++;
        qDebug() << "Line" << lineCount << ":" << line;
    }
    
    file.close();
    qDebug() << "File closed successfully";
    
    qDebug() << "Test completed successfully!";
    return 0;
}