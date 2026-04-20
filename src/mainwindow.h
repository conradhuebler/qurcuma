#pragma once

#include "settings.h"
#include "vtfparser.h"
#include "xyzparser.h"
#include "pdbparser.h"  // Claude Generated - Phase 5C
#include "mol2parser.h"  // Claude Generated - Phase 5C
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDateTime>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMainWindow>
#include <QMap>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointer>  // Claude Generated - For dialog pointer management
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QProgressDialog>
#include <QSplitter>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <functional>

#include "dialogs/nmrspectrumdialog.h"
#include "modifiabletextedit.h"
#include "widgets/breadcrumbbar.h"
class MoleculeViewer;
class VisualizationSettingsDialog;  // Claude Generated - For shortcut synchronization
class WorkspaceManager;  // Claude Generated Phase 4 - Workspace management
class AtomListPanel;  // Claude Generated Phase 2C - Atom list panel with table view
class SftpItemModel;  // Claude Generated - Remote Directory Mounting
class SimulationControlWidget;  // Claude Generated - Interactive Simulation Integration
class SimulationDialog;  // Claude Generated - Interactive Simulation Integration
#include "simulationworker.h"  // Claude Generated - for SimulationConfig


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

    // Claude Generated - UI Restructuring: Layout presets for flexible dock arrangement
    enum class LayoutPreset {
        Visualization,  // 3D viewer focus with minimal UI
        Editing,        // Editors and file browser focus
        Calculation,    // Output view and calculation workflow
        Analysis        // All panels visible, balanced layout
    };

    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    /**
     * @brief Load a molecule file after the event loop has started.
     *
     * Claude Generated (Apr 2026): Called from main.cpp via QTimer::singleShot
     * to handle command-line file arguments. Must be called after the event loop
     * starts so Qt3D is fully initialized.
     *
     * @param path  Absolute or relative path to the molecule file (.xyz, .vtf, .pdb, .mol2)
     */
    void loadFileFromArg(const QString& path);

private slots:
    void runCommand();
    void programSelected(int index);
    void projectSelected(const QModelIndex &index);
    void processOutput();
    void processError();
    void configurePrograms();
    void runSimulation();
    void startNewCalculation();  // Neue Funktion


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

    // Claude Generated - Phase 2A: Selection commands
    void selectAllAtoms();
    void clearAtomSelection();

    // Claude Generated - Helper for shortcut synchronization with dialog
    void syncVisualizationDialog();

    // Claude Generated - Quick Win: Copy/Paste structures (moved from private to slots)
    void copyStructureToClipboard();
    void pasteStructureFromClipboard();

    // Claude Generated - Quick Win: Zoom to fit molecule (moved from private to slots)
    void zoomToMolecule();

    // Claude Generated - Quick Fix: Clear output view (moved from private to slots)
    void clearOutputView();

    // Claude Generated - Quick Fix: Copy current path to clipboard
    void copyCurrentPath();

    // Claude Generated - Quick Fix: Show about dialog
    void showAboutDialog();

    // Claude Generated - Visual Polish: Dark mode
    void toggleDarkMode();

    // Claude Generated - Create new directory (moved from private to slots)
    void createNewDirectory();

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

    // Claude Generated - SFTP: Load molecule file (local or remote)
    void loadMoleculeFile(const QString& filePath);

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

    void updatePathLabel(const QString& path);  // Claude Generated Phase 1 - Updates breadcrumb bar
    void toggleLeftPanel();
    void updateWorkflowState(WorkflowState state);  // Claude Generated - Phase 2.2

    // Claude Generated Phase 3.2 - Bookmark and workspace slots
    void updateBookmarkTree();
    void onBookmarkItemClicked(QTreeWidgetItem* item, int column);
    void onBookmarkContextMenu(const QPoint& pos);
    void saveCurrentWorkspace();
    void onWorkspaceItemClicked(QListWidgetItem* item);
    void onWorkspaceContextMenu(const QPoint& pos);
    void restoreWorkspaceState(const Settings::Workspace& ws);
    void updateWorkspaceList();
    void updateWorkspaceMenu(QMenu* menu);

    // Claude Generated - UI Restructuring: Layout preset management
    void applyLayoutPreset(LayoutPreset preset);
    void applyVisualizationLayout();
    void applyEditingLayout();
    void applyCalculationLayout();
    void applyAnalysisLayout();
    void createDockWidgets();  // Helper to create all dock widgets

    QTreeWidget* m_bookmarkTreeView;  // Claude Generated Phase 3.2 - Replaced QListWidget
    QListWidget* m_workspaceListView;  // Claude Generated Phase 4.3
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

    // Claude Generated - For shortcut dialog synchronization
    QPointer<VisualizationSettingsDialog> m_visualizationDialog;
    QCompleter* m_commandCompleter;
    QToolButton* m_bookmarkButton;
    BreadcrumbBar* m_breadcrumbBar;  // Claude Generated Phase 1 - Clickable path navigation
    QLabel *m_currentProjectLabel;
    // Claude Generated - Phase 3.3: Visual state indicators
    QLabel *m_stateIcon, *m_stateIndicator;
    MoleculeViewer *m_moleculeView;
    NMRSpectrumDialog* m_nmrDialog;
    AtomListPanel* m_atomListPanel = nullptr;  // Claude Generated Phase 2C - Atom list panel

    // VTF/XYZ Parser
    VTFParser* m_vtfParser;
    XYZParser* m_xyzParser;

    QStringList m_simulationPrograms{ "curcuma", "orca", "xtb" };
    QStringList m_visualizerPrograms{ "iboview", "avogadro" };

    QString m_workingDirectory;
    QString m_currentCalculationDir; // Aktuelles Berechnungsverzeichnis
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
    QVector<Settings::RecentFileEntry> m_recentFiles;  // Claude Generated Phase 2 - Now with timestamps

    // Claude Generated - Phase SFTP Integration: Recent remote connections
    QMenu* m_recentConnectionsMenu = nullptr;
    void updateRecentConnectionsMenu();  // Populate recent connections menu
    void openRecentConnection(const QString& profileId);  // Open SFTP dialog with profile pre-selected

    // Claude Generated Phase 4 - Workspace management
    WorkspaceManager* m_workspaceManager = nullptr;
    QMenu* m_workspaceMenu = nullptr;

    // Claude Generated - Quick Win: Auto-save drafts
    QTimer* m_autoSaveTimer = nullptr;
    void autoSaveDrafts();
    void loadDrafts();

    // Claude Generated - Visual Polish: Dark mode
    bool m_darkModeEnabled = false;
    QAction* m_darkModeAction = nullptr;  // Store checkbox reference
    void applyStylesheet(bool darkMode);

    // Claude Generated - Remote Directory Mounting
    QTreeWidget* m_remoteDirectoriesView = nullptr;
    QMap<QString, SftpItemModel*> m_remoteSftpModels;
    QString m_currentRemoteMountId;

    // Claude Generated - Dock architecture rewrite (2026-04): 5 docks rahmen MoleculeViewer (CentralWidget)
    QDockWidget* m_projectDock = nullptr;           // Left: working dir + calculation files
    QDockWidget* m_navigationDock = nullptr;        // Left (tabbed): bookmarks / workspaces / remote
    QDockWidget* m_editorsDock = nullptr;           // Right: structure + input editors (internal QTabWidget)
    QDockWidget* m_atomsSimulationDock = nullptr;   // Right: atom list + simulation (internal QTabWidget)
    QDockWidget* m_outputViewDock = nullptr;        // Bottom: output log
    QTabWidget* m_editorsTabs = nullptr;            // Internal tabs inside m_editorsDock
    QTabWidget* m_atomsSimulationTabs = nullptr;    // Internal tabs inside m_atomsSimulationDock
    QTabWidget* m_navigationTabs = nullptr;         // Internal tabs inside m_navigationDock
    QByteArray m_defaultDockState;                  // Baseline after createDockWidgets
    QHash<int, QByteArray> m_presetStates;          // keyed by LayoutPreset enum
    SimulationControlWidget* m_simulationControlWidget = nullptr;  // Claude Generated
    SimulationDialog* m_simulationDialog = nullptr;  // Claude Generated
    SimulationConfig m_simulationConfig;             // Claude Generated - Shared config: dock widget and dialog stay synchronized

    // Claude Generated - Interactive Simulation Integration
    void openSimulationDialog(SimulationConfig::Mode mode);
    void wireSimulationWorker(SimulationWorker* worker);  // Claude Generated - Direct worker->view wiring
    void onSimulationConfigChanged(SimulationConfig cfg);

    // Note: m_atomListPanel is already a dock widget (AtomListPanel inherits QDockWidget)
    void updateRemoteDirectoriesView();
    void onRemoteDirectoryClicked(QTreeWidgetItem* item, int column);
    void onAddRemoteDirectoryClicked();
    void onRemoteFileDoubleClicked(const QModelIndex& index);
    void downloadAndLoadRemoteFile(const QString& filePath);

protected:
    // Claude Generated - Quick Win: Drag & Drop support
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
};