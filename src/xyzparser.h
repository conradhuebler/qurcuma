// XYZ Parser - Claude Generated
// Parses XYZ files for molecular visualization with trajectory support

#pragma once

#include "view.h"
#include <QString>
#include <QVector>
#include <QFile>
#include <QTextStream>
#include <QDebug>

class XYZParser
{
public:
    struct XYZAtom {
        QString element;
        float x, y, z;
    };

    struct XYZFrame {
        QVector<XYZAtom> atoms;
        QString comment;
    };

    XYZParser() = default;

    // Parse XYZ file and return first frame
    bool parseFile(const QString& filePath, XYZFrame& frame);

    // Parse all frames from XYZ trajectory file
    bool parseTrajectory(const QString& filePath);

    // Get frame count
    int getFrameCount() const { return m_frames.size(); }

    // Get frame by index
    bool getFrame(int frameIndex, XYZFrame& frame) const;

    // Convert XYZ data to MoleculeViewer format
    static void convertToMoleculeViewer(const XYZFrame& xyzFrame, 
                                      QVector<MoleculeViewer::Atom>& atoms,
                                      QVector<MoleculeViewer::Bond>& bonds);

private:
    bool parseAsciiFormat(const QString& filePath, QVector<XYZFrame>& frames);
    
    QVector<XYZFrame> m_frames;  // Store all parsed frames
};