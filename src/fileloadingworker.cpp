// Claude Generated - Phase 5D - Async File Loading Worker Implementation
#include "fileloadingworker.h"
#include "vtfparser.h"
#include "xyzparser.h"
#include "pdbparser.h"
#include "mol2parser.h"
#include <QDebug>
#include <QFileInfo>

FileLoadingWorker::FileLoadingWorker(QObject *parent)
    : QObject(parent)
{
}

FileLoadingWorker::~FileLoadingWorker()
{
}

void FileLoadingWorker::loadFile(const QString& filePath, FileType fileType)
{
    m_currentFile = filePath;
    m_shouldCancel = false;

    emit progress(0);

    MoleculeData result;

    try {
        // Load based on file type
        switch (fileType) {
        case VTF:
            result = loadVTFFile(filePath);
            break;
        case XYZ:
            result = loadXYZFile(filePath);
            break;
        case PDB:
            result = loadPDBFile(filePath);
            break;
        case MOL2:
            result = loadMOL2File(filePath);
            break;
        default:
            result.errorMessage = "Unknown file type";
            result.success = false;
            break;
        }

        if (m_shouldCancel) {
            result.errorMessage = "Loading cancelled by user";
            result.success = false;
        }

        if (result.success) {
            emit progress(100);
        }

        emit finished(result);

    } catch (const std::exception& e) {
        qWarning() << "Exception during file loading:" << e.what();
        result.errorMessage = QString("Exception: %1").arg(e.what());
        result.success = false;
        emit error(result.errorMessage);
        emit finished(result);
    } catch (...) {
        qWarning() << "Unknown exception during file loading";
        result.errorMessage = "Unknown exception during file loading";
        result.success = false;
        emit error(result.errorMessage);
        emit finished(result);
    }
}

void FileLoadingWorker::cancel()
{
    m_shouldCancel = true;
}

FileLoadingWorker::FileType FileLoadingWorker::detectFileType(const QString& filePath) const
{
    QString ext = QFileInfo(filePath).suffix().toLower();

    if (ext == "vtf") return VTF;
    if (ext == "xyz") return XYZ;
    if (ext == "pdb") return PDB;
    if (ext == "mol2") return MOL2;

    return VTF;  // Default
}

FileLoadingWorker::MoleculeData FileLoadingWorker::loadVTFFile(const QString& filePath)
{
    MoleculeData result;
    result.filename = QFileInfo(filePath).fileName();

    try {
        VTFParser parser;
        VTFParser::VTFFrame frame;

        if (!parser.parseFile(filePath, frame)) {
            result.errorMessage = "Failed to parse VTF file";
            result.success = false;
            return result;
        }

        // Extract atom data from VTF frame
        for (const auto& atom : frame.atoms) {
            result.positions.append(QVector3D(atom.x, atom.y, atom.z));
            result.elements.append(atom.element);
            result.colors.append(VTFParser::getAtomColor(atom.type));
            result.scales.append(VTFParser::getAtomRadius(atom.radius));
        }

        emit progress(50);

        if (m_shouldCancel) {
            result.errorMessage = "Loading cancelled";
            result.success = false;
            return result;
        }

        result.success = true;
        return result;

    } catch (const std::exception& e) {
        result.errorMessage = QString("VTF parsing error: %1").arg(e.what());
        result.success = false;
        return result;
    }
}

FileLoadingWorker::MoleculeData FileLoadingWorker::loadXYZFile(const QString& filePath)
{
    MoleculeData result;
    result.filename = QFileInfo(filePath).fileName();

    // XYZ format support to be implemented in Phase 5E
    // For now, delegate to VTF parser mechanism
    result.errorMessage = "XYZ async loading deferred to Phase 5E";
    result.success = false;
    return result;
}

FileLoadingWorker::MoleculeData FileLoadingWorker::loadPDBFile(const QString& filePath)
{
    MoleculeData result;
    result.filename = QFileInfo(filePath).fileName();

    try {
        PDBParser parser;
        PDBParser::PDBFrame frame;

        if (!parser.parseFile(filePath, frame)) {
            result.errorMessage = "Failed to parse PDB file: " + parser.getLastError();
            result.success = false;
            return result;
        }

        // Extract atom data from PDB frame
        for (const auto& atom : frame.atoms) {
            result.positions.append(QVector3D(atom.x, atom.y, atom.z));
            result.elements.append(atom.element);
            result.colors.append(QColor(200, 200, 200));
            result.scales.append(1.0f);
        }

        emit progress(50);

        if (m_shouldCancel) {
            result.errorMessage = "Loading cancelled";
            result.success = false;
            return result;
        }

        result.success = true;
        return result;

    } catch (const std::exception& e) {
        result.errorMessage = QString("PDB parsing error: %1").arg(e.what());
        result.success = false;
        return result;
    }
}

FileLoadingWorker::MoleculeData FileLoadingWorker::loadMOL2File(const QString& filePath)
{
    MoleculeData result;
    result.filename = QFileInfo(filePath).fileName();

    // MOL2 format async loading deferred to Phase 5E
    // Requires element symbol extraction from Sybyl atom types
    result.errorMessage = "MOL2 async loading deferred to Phase 5E";
    result.success = false;
    return result;
}
