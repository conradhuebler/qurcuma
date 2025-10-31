// VTF Parser - Claude Generated
// Specialized parsing of VTF (Visualization Toolkit Format) files for molecular visualization
// Supports both static structures and trajectory data from molecular dynamics

#pragma once

#include "view.h"
#include <QString>
#include <QVector>
#include <QFile>
#include <QDebug>

class VTFParser
{
public:
    struct VTFAtom {
        int index;
        QString element;
        float radius;
        QString type;
        QString name;
        float x, y, z;
    };

    struct VTFBond {
        int atom1;
        int atom2;
    };

    struct VTFFrame {
        QVector<VTFAtom> atoms;
        QVector<VTFBond> bonds;
        bool hasUnitCell = false;
        float cellA, cellB, cellC;
    };

    VTFParser() = default;

    // Parse VTF file and return first frame
    bool parseFile(const QString& filePath, VTFFrame& frame);

    // Parse all frames from VTF file and store internally
    bool parseTrajectory(const QString& filePath);

    // Get frame count
    int getFrameCount() const { return m_frames.size(); }

    // Get frame by index
    bool getFrame(int frameIndex, VTFFrame& frame) const;

    // Convert VTF data to MoleculeViewer format
    static void convertToMoleculeViewer(const VTFFrame& vtfFrame, 
                                      QVector<MoleculeViewer::Atom>& atoms,
                                      QVector<MoleculeViewer::Bond>& bonds);

    // Get atom color based on VTF type
    static QColor getAtomColor(const QString& type);

    // Get atom radius from VTF data
    static float getAtomRadius(float vtfRadius);

private:
    bool parseAsciiFormat(const QString& filePath, QVector<VTFFrame>& frames);
    QString trimQuotes(const QString& str);

    QVector<VTFFrame> m_frames;  // Store all parsed frames
};