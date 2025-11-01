#pragma once

#include "settings.h"
#include "vtfparser.h"
#include "xyzparser.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDateTime>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMainWindow>
#include <QMap>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QProgressDialog>
#include <QSplitter>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <functional>

#include "dialogs/nmrspectrumdialog.h"
#include "modifiabletextedit.h"
class MoleculeViewer;


struct CalculationEntry {
    QString id;          // Eindeutige ID (z.B. Zeitstempel)
    QString program;
    QString command;
    QString structureFile;
    QString inputFile;
    QString outputFile;
    QDateTime timestamp;
    QString status;      // "started", "completed", "error"
    // Weitere Metadaten
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // Claude Generated - Phase 2.2: Workflow state management
    enum class WorkflowState {
        NoDirectory,          // No calculation directory selected
        DirectoryReady,       // Directory exists, can edit files
        CalculationRunning,   // Process executing
        CalculationComplete,  // Process finished successfully
        CalculationError      // Process failed
    };

    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void runCommand();
    void programSelected(int index);
    void projectSelected(const QModelIndex &index);
    void processOutput();
    void processError();
    void configurePrograms();
    void runSimulation();
    void startNewCalculation();  // Neue Funktion

    // Frame navigation slots
    void onFrameChanged(int frameIndex);
    void onTrajectoryLoaded(int frameCount);
    void onPreviousFrame();
    void onNextFrame();
    void onFrameSliderChanged(int value);

    // Keyboard shortcuts - Claude Generated Phase 1.2
    void cancelCalculation();
    void switchEditorTab();
    void saveCurrentEditor();

    // Claude Generated - Visualization settings
    void openVisualizationSettings();

    // Claude Generated - Rendering shortcuts (1-4 for rendering modes)
    void setRenderingModeBallAndStick();
    void setRenderingModeSpaceFilling();
    void setRenderingModeWireframe();
    void setRenderingModeSticks();

    // Claude Generated - Size adjustment shortcuts
    void increaseAtomSize();
    void decreaseAtomSize();
    void increaseBondThickness();
    void decreaseBondThickness();

    // Claude Generated - Focus & centering commands
    void fitMoleculeInView();
    void centerViewOnSelection();

private:
    void setupUI();
    void createToolbars();
    void createMenus();
    void setupProjectViewContextMenu();
    void setupConnections();
    void setupShortcuts();  // Claude Generated - Phase 1.2
    void loadSettings();
    bool checkProgramPath(const QString &program);

    bool setupCalculationDirectory();
    void updateOutputView(const QString& logFile, bool scrollToBottom = false);

    // Claude Generated - Phase 4.1: Enhanced error dialog
    void showEnhancedError(const QString& title, const QString& problem, const QString& solution,
                          std::function<void()> actionCallback = nullptr);

    void createNewDirectory();
    void setupProgramSpecificDirectory(const QString &dirPath, const QString &program);
    void updateDirectoryContent();  // Claude Generated - removed unused path parameter

    QPair<int, int> countImaginaryFrequencies(const QString &filename);
    void initializeProgramCommands();
    void updateCommandLineVisibility(const QString &program);
    void setupContextMenu();
    void openWithVisualizer(const QString &filePath, const QString &visualizer);
    void orcaPlotVib(const QString &outputFile, int freqNumber);
    void syncRightView();  // Claude Generated - removed unused path parameter
    void saveCalculationInfo();
    void loadCalculationInfo(const QString &path);
    QList<CalculationEntry> loadCalculationHistory(const QString &path);
    void addCalculationToHistory(const CalculationEntry &entry);
    QString generateUniqueFileName(const QString &baseFileName, const QString &extension);
    // Path helpers - Claude Generated for clarity
    QString currentCalculationDir() const {
        return QDir(m_workingDirectory).filePath(m_currentCalculationDir);
    }
    QString getCalculationDirName() const {
        return m_currentCalculationDir;
    }
    bool isValidCalculationDir() const {
        return !m_currentCalculationDir.isEmpty() &&
               m_currentCalculationDir != "." &&
               m_currentCalculationDir != "..";
    }
    QStringList currentSubdirectories() const;

    void setupBookmarkView();
    void updateBookmarkView();
    void switchWorkingDirectory(const QString& path);

    // Claude Generated - Quick Win: Recent files management
    void addToRecentFiles(const QString& path);
    void updateRecentFilesMenu();
    void openRecentFile(const QString& path);

    void updatePathLabel(const QString& path);
    void toggleLeftPanel();
    void updateWorkflowState(WorkflowState state);  // Claude Generated - Phase 2.2

    QListWidget* m_bookmarkListView;
    QListView* m_projectListView;
    QListView* m_directoryContentView;
    QLineEdit* m_commandInput;
    QLineEdit *m_inputFileEdit, *m_inputFileEditExtension;
    QLineEdit *m_structureFileEdit, *m_structureFileEditExtension;
    QComboBox* m_programSelector;
    ModifiableTextEdit* m_structureView;  // Claude Generated - Phase 2.3
    ModifiableTextEdit* m_inputView;      // Claude Generated - Phase 2.3
    QTextEdit* m_outputView;
    QPushButton *m_newCalculationButton, *m_chooseDirectory, *m_runCalculation;
    QCheckBox* m_uniqueFileNames;
    QSpinBox* m_threads;
    QFileSystemModel* m_projectModel;
    QFileSystemModel* m_directoryContentModel;
    QProcess* m_currentProcess;
    Settings m_settings;
    QMap<QString, QStringList> m_programCommands;
    QCompleter* m_commandCompleter;
    QToolButton* m_bookmarkButton;
    QSplitter* m_splitter;
    QLabel *m_currentPathLabel, *m_currentProjectLabel;
    // Claude Generated - Phase 3.3: Visual state indicators
    QLabel *m_stateIcon, *m_stateIndicator;
    MoleculeViewer *m_moleculeView;
    NMRSpectrumDialog* m_nmrDialog;

    // VTF Frame Navigation
    QSlider* m_frameSlider;
    QLabel* m_frameLabel;
    QSpinBox* m_frameJumpBox;  // Claude Generated - Quick Win: Frame jump input
    VTFParser* m_vtfParser;
    XYZParser* m_xyzParser;

    QStringList m_simulationPrograms{ "curcuma", "orca", "xtb" };
    QStringList m_visualizerPrograms{ "iboview", "avogadro" };

    QString m_workingDirectory;
    QString m_currentCalculationDir; // Aktuelles Berechnungsverzeichnis
    int m_lastLeftPanelWidth = 0;
    QVector<QPair<int, double>> m_frequencies;

    // Claude Generated - Phase 2.2: Workflow state
    WorkflowState m_workflowState = WorkflowState::NoDirectory;

    // Claude Generated - Phase 1.3: Progress dialog for calculations
    QProgressDialog* m_progressDialog = nullptr;

    // Claude Generated - Quick Win: Calculation timer
    QTimer* m_calculationTimer = nullptr;
    QLabel* m_timerLabel = nullptr;
    int m_elapsedSeconds = 0;

    // Claude Generated - Quick Win: Recent files
    QMenu* m_recentFilesMenu = nullptr;
    QStringList m_recentFiles;

    // Claude Generated - Quick Win: Auto-save drafts
    QTimer* m_autoSaveTimer = nullptr;
    void autoSaveDrafts();
    void loadDrafts();

    // Claude Generated - Quick Win: Copy/Paste structures
    void pasteStructureFromClipboard();

    // Claude Generated - Quick Win: Zoom to fit molecule
    void zoomToMolecule();

    // Claude Generated - Quick Fix: Clear output view
    void clearOutputView();

    // Claude Generated - Quick Fix: Copy current path to clipboard
    void copyCurrentPath();

    // Claude Generated - Quick Fix: Show about dialog
    void showAboutDialog();

    // Claude Generated - Visual Polish: Dark mode
    bool m_darkModeEnabled = false;
    QAction* m_darkModeAction = nullptr;  // Store checkbox reference
    void applyStylesheet(bool darkMode);
    void toggleDarkMode();

protected:
    // Claude Generated - Quick Win: Drag & Drop support
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};