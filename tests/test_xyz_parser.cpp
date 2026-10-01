// Test für XYZ Parser - Einheitentest für XYZ-Trajektorie-Support
#include "src/xyzparser.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTemporaryFile>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qDebug() << "=== XYZ Parser Test ===";
    
    // Test 1: Single frame parsing
    qDebug() << "\n--- Test 1: Single Frame Parsing ---";
    
    XYZParser parser;
    XYZParser::XYZFrame frame;
    
    QString singleFrameContent = "1\nTest comment\nC    0.0    0.0    0.0";
    QTemporaryFile tempFile1;
    tempFile1.open();
    tempFile1.write(singleFrameContent.toUtf8());
    tempFile1.close();
    
    if (parser.parseFile(tempFile1.fileName(), frame)) {
        qDebug() << "Single frame parsing successful!";
        qDebug() << "Atoms count:" << frame.atoms.size();
        if (frame.atoms.size() > 0) {
            qDebug() << "First atom:" << frame.atoms[0].element 
                     << frame.atoms[0].x << frame.atoms[0].y << frame.atoms[0].z;
        }
    } else {
        qDebug() << "Single frame parsing failed!";
    }
    
    // Test 2: Multi-frame trajectory parsing
    qDebug() << "\n--- Test 2: Multi-Frame Trajectory Parsing ---";
    
    QString multiFrameContent = 
        "4\n"
        "Frame 1: Methane molecule\n"
        "C    0.000000    0.000000    0.000000\n"
        "H    1.000000    0.000000    0.000000\n"
        "H   -0.333333    0.942809    0.000000\n"
        "H   -0.333333   -0.471405    0.816497\n"
        "4\n"
        "Frame 2: Methane molecule (slight movement)\n"
        "C    0.050000    0.020000    0.010000\n"
        "H    1.050000    0.020000    0.010000\n"
        "H   -0.283333    0.962809    0.010000\n"
        "H   -0.283333   -0.451405    0.826497\n"
        "4\n"
        "Frame 3: Methane molecule (further movement)\n"
        "C    0.100000    0.040000    0.020000\n"
        "H    1.100000    0.040000    0.020000\n"
        "H   -0.233333    0.982809    0.020000\n"
        "H   -0.233333   -0.431405    0.836497";
    
    QTemporaryFile tempFile2;
    tempFile2.open();
    tempFile2.write(multiFrameContent.toUtf8());
    tempFile2.close();
    
    if (parser.parseTrajectory(tempFile2.fileName())) {
        qDebug() << "Multi-frame trajectory parsing successful!";
        qDebug() << "Frame count:" << parser.getFrameCount();
        
        // Test accessing individual frames
        for (int i = 0; i < parser.getFrameCount(); ++i) {
            XYZParser::XYZFrame testFrame;
            if (parser.getFrame(i, testFrame)) {
                qDebug() << "Frame" << i+1 << "atoms:" << testFrame.atoms.size();
                if (testFrame.atoms.size() > 0) {
                    qDebug() << "  First atom:" << testFrame.atoms[0].element 
                             << testFrame.atoms[0].x << testFrame.atoms[0].y << testFrame.atoms[0].z;
                }
            }
        }
    } else {
        qDebug() << "Multi-frame trajectory parsing failed!";
    }
    
    // Test 3: Conversion to MoleculeViewer format
    qDebug() << "\n--- Test 3: MoleculeViewer Conversion ---";
    
    QVector<MoleculeViewer::Atom> mvAtoms;
    QVector<MoleculeViewer::Bond> mvBonds;
    
    XYZParser::XYZFrame testFrame;
    if (parser.getFrame(0, testFrame)) {
        XYZParser::convertToMoleculeViewer(testFrame, mvAtoms, mvBonds);
        qDebug() << "Conversion successful!";
        qDebug() << "MoleculeViewer atoms:" << mvAtoms.size();
        qDebug() << "MoleculeViewer bonds:" << mvBonds.size();
        
        for (const auto& atom : mvAtoms) {
            qDebug() << "  Atom:" << atom.element 
                     << "Position:" << atom.position.x() << atom.position.y() << atom.position.z();
        }
    }
    
    qDebug() << "\n=== Test completed ===";
    
    // Exit after tests
    app.quit();
    return 0;
}