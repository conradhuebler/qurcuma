// Claude Generated - Phase 5D - Async File Loading Worker for Large Molecular Files
#ifndef FILELOADINGWORKER_H
#define FILELOADINGWORKER_H

#include <QObject>
#include <QString>
#include <QVector3D>
#include <QVector>
#include <QColor>

/**
 * @brief Asynchronous file loading worker for blocking parsers
 *
 * Runs file parsing on a separate QThread to keep UI responsive:
 * - Supports VTF, XYZ, PDB, MOL2 formats
 * - Emits progress signals for UI updates
 * - Thread-safe data return via signals
 * - Cancellation support for large files
 *
 * Usage:
 * FileLoadingWorker *worker = new FileLoadingWorker();
 * QThread *thread = new QThread();
 * worker->moveToThread(thread);
 * connect(thread, &QThread::started, worker, &FileLoadingWorker::loadFile);
 * connect(worker, &FileLoadingWorker::finished, thread, &QThread::quit);
 * thread->start();
 */
class FileLoadingWorker : public QObject
{
    Q_OBJECT

public:
    explicit FileLoadingWorker(QObject *parent = nullptr);
    ~FileLoadingWorker();

    // File types supported
    enum FileType {
        VTF,
        XYZ,
        PDB,
        MOL2
    };

    // Molecule data structure
    struct MoleculeData {
        QVector<QVector3D> positions;
        QVector<QString> elements;
        QVector<QColor> colors;
        QVector<float> scales;
        QString filename;
        bool success = false;
        QString errorMessage;
    };

public slots:
    // Load file on worker thread
    void loadFile(const QString& filePath, FileType fileType);

    // Cancel current loading operation
    void cancel();

signals:
    // Progress signal: percent (0-100)
    void progress(int percent);

    // Finished loading (successful or failed)
    void finished(const MoleculeData& data);

    // Error signal with message
    void error(const QString& errorMessage);

private:
    // File parsers
    MoleculeData loadVTFFile(const QString& filePath);
    MoleculeData loadXYZFile(const QString& filePath);
    MoleculeData loadPDBFile(const QString& filePath);
    MoleculeData loadMOL2File(const QString& filePath);

    // Determine file type from extension
    FileType detectFileType(const QString& filePath) const;

    // Flag for cancellation
    volatile bool m_shouldCancel = false;

    // Current file being loaded (for progress tracking)
    QString m_currentFile;
};

#endif // FILELOADINGWORKER_H
