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

    // Write single frame to XYZ file (Phase 4B - Bond Editing)
    static bool writeFile(const QString& filePath, const XYZFrame& frame);

    // Write all frames to XYZ trajectory file (Phase 4B - Bond Editing)
    static bool writeTrajectory(const QString& filePath, const QVector<XYZFrame>& frames);

    // Convert MoleculeViewer data to XYZ format (Phase 4B - Auto-save)
    static bool convertFromMoleculeViewer(const QVector<MoleculeViewer::Atom>& atoms,
                                         const QString& comment,
                                         XYZFrame& xyzFrame);

private:
    bool parseAsciiFormat(const QString& filePath, QVector<XYZFrame>& frames);

    QVector<XYZFrame> m_frames;  // Store all parsed frames
};