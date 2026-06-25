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
#include <QElapsedTimer>
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
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <functional>

#include "dialogs/nmrspectrumdialog.h"
#include "modifiabletextedit.h"
#include "widgets/breadcrumbbar.h"
#include "snapshotswidget.h"  // Claude Generated 2026 - global MoleculeSnapshot + SnapshotsWidget
#include "simulationworker.h"  // Claude Generated - for SimulationConfig
#include "lesson.h"  // Claude Generated 2026 - OER teaching scenarios (Lesson model)
class MoleculeViewer;
class DisplayPanel;  // Claude Generated 2026 - docked viewer display options (replaces the modal dialog)
class CommandPalette;  // Claude Generated 2026 - P3 Ctrl+K command palette
class RMSDWidget;  // Claude Generated 2026 - RMSD / align tool (Analysis dock)
class WorkspaceManager;  // Claude Generated Phase 4 - Workspace management
class AtomListPanel;  // Claude Generated Phase 2C - Atom list panel with table view
#ifdef USE_SFTP
class SftpItemModel;  // Claude Generated - Remote Directory Mounting
#endif
class SimulationControlWidget;  // Claude Generated - Interactive Simulation Integration
class LessonStructureModel;     // Claude Generated 2026 - in-memory lesson structure list model
class SimulationChartWidget;    // Claude Generated 2026 - live MD temperature/energy charts
class QDialog;                  // Claude Generated 2026 - host for the modeless charts dialog


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

    // Claude Generated 2026 - P2: top-level app mode. Explore = molecule viewer focus
    // (calculation toolbar hidden); Compute = calculation workflow (toolbar shown).
    enum class AppMode { Explore, Compute };

    // Claude Generated 2026 - "Use Invocation Directory" preference
    // invocationDir is captured from QDir::currentPath() in main.cpp BEFORE
    // QApplication is created. When useInvocationDirectoryEnabled() is true,
    // this directory becomes the active Working Directory.
    MainWindow(const QString& invocationDir = QString(), QWidget *parent = nullptr);
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

    /**
     * @brief Auto-start the interactive simulation in the given mode after a
     *        command-line file load (`qurcuma <file> -md` / `-opt`).
     *
     * Claude Generated 2026: Diagnostic lever for the release/AVX-512 crash —
     * lets the interactive sim be launched from bash so the crash is
     * reproducible under gdb/valgrind. No-op if no molecule was loaded.
     * buildConfig() reflects the requested mode because setMode() drives the
     * dock's mode combo. The workerStarted -> wireSimulationWorker connection
     * is already in place, so the viewer lights up exactly as a button click.
     *
     * @param mode  MolecularDynamics or GeometryOptimization
     */
    void autoStartSimulation(SimulationConfig::Mode mode);

    /**
     * @brief Switch the working directory to the directory where the molecule file
     *        resides, after it was loaded successfully.
     *
     * Claude Generated 2026: Wired to be called by loadMoleculeFile() so the user
     * automatically lands in the file's parent directory after a successful load.
     * No-op if the path is empty or does not exist.
     *
     * @param dir  Directory to switch to. If empty, falls back to the captured
     *             invocation dir (m_invocationDir).
     */
    void setWorkingDirFromArg(const QString& dir);

    /**
     * @brief Switch the working directory to a directory given on the command line
     *        (e.g. `qurcuma .`).
     *
     * Claude Generated 2026: Separate entry point from setWorkingDirFromArg() so
     * the CLI '.' case never triggers a file load. Called from main.cpp after
     * the event loop starts, so it has access to switchWorkingDirectory().
     *
     * @param dir  Absolute directory path. If empty, no-op.
     */
    void loadDirFromArg(const QString& dir);

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

    // Claude Generated 2026 - Save the (possibly MD/Opt-modified) molecule.
    // Empty path → overwrite the current XYZ source if it's a .xyz file,
    // otherwise open a Save-As dialog. Returns true on success.
    bool saveCurrentStructure();
    void saveCurrentStructureAs();

    // Claude Generated 2026 - Reload the current file to discard simulation changes.
    void reloadCurrentFile();

    // Claude Generated - Visualization settings
    void openVisualizationSettings();

    // Claude Generated 2026 - RMSD / align / reorder tool (curcuma RMSDDriver),
    // embedded in the Analysis dock. Raises the dock + RMSD tab, re-seeds the
    // reference from the current viewer frame; an optional targetFile preloads
    // the comparison structure (used by the file-manager context menu).
    void showRMSDTool(const QString& targetFile = QString());

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
    void centerMoleculeAtOrigin();

    // Claude Generated - Phase 2A: Selection commands
    void selectAllAtoms();
    void clearAtomSelection();

    // Claude Generated - Helper for shortcut synchronization with dialog
    void syncVisualizationDialog();

    // Claude Generated - Quick Win: Copy/Paste structures (moved from private to slots)
    void copyStructureToClipboard();
    void pasteStructureFromClipboard();

    // Claude Generated 2026 - Merge a molecule from a file into the current scene
    // (structure editing). addMoleculeToScene() opens a file dialog;
    // mergeFileIntoScene() parses+appends a given path (also used by the browser
    // right-click "Add to current scene").
    void addMoleculeToScene();
    void mergeFileIntoScene(const QString& filePath);

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

    // Claude Generated 2026 - "Use Invocation Directory" preference
    void toggleUseInvocationDirectory();

    // Claude Generated - Create new directory (moved from private to slots)
    void createNewDirectory();

private:
    void setupUI();
    void createToolbars();
    void createModeBar();                       // Claude Generated 2026 - P2 Explore/Compute switch
    void setAppMode(AppMode mode, bool reflow = true);  // apply mode (toolbar + dock visibility)
    void showCommandPalette();                  // Claude Generated 2026 - P3 Ctrl+K palette
    void createMenus();
    void seedRMSDReference();  // Claude Generated 2026 - re-seed RMSD reference from viewer
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

    // Claude Generated 2026 - OER teaching scenarios (Lessons). A lesson is a
    // self-contained *.qlesson.json (structures embedded as inline XYZ + their
    // simulation conditions + author/ORCID/... metadata). openLesson unpacks it
    // into a working directory so the structures appear in the file browser;
    // applyLessonConditions restores a structure's stored conditions on load.
    void openLesson(const QString& path);
    void saveLesson(const QString& path);
    void addCurrentStructureToLesson();
    void editLessonMetadata();
    void applyLessonConditions(const QString& filePath);
    // Refresh the optional in-memory lesson-structure list in the Project dock and
    // (when @p autoShow) reveal it; load one entry's geometry + conditions.
    void refreshLessonStructureView(bool autoShow = false);
    void loadLessonStructureFromIndex(const QModelIndex& index);

    // Claude Generated 2026 - Parse just the first frame of a structure file (xyz/vtf/
    // pdb/mol2) into viewer atoms/bonds; used by addMoleculeToScene() to merge.
    bool parseFirstFrame(const QString& filePath, QVector<MoleculeViewer::Atom>& atoms,
        QVector<MoleculeViewer::Bond>& bonds);

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

    // Claude Generated 2026 - Docked viewer display options (replaces the modal dialog)
    DisplayPanel* m_displayPanel = nullptr;
    QDockWidget* m_displayDock = nullptr;
    CommandPalette* m_commandPalette = nullptr;  // Claude Generated 2026 - P3 Ctrl+K
    QCompleter* m_commandCompleter;
    QToolButton* m_bookmarkButton;
    BreadcrumbBar* m_breadcrumbBar;  // Claude Generated Phase 1 - Clickable path navigation
    QLabel *m_currentProjectLabel;
    // Claude Generated - Phase 3.3: Visual state indicators
    QLabel *m_stateIcon, *m_stateIndicator;
    MoleculeViewer *m_moleculeView;
    NMRSpectrumDialog* m_nmrDialog;
    RMSDWidget* m_rmsdWidget = nullptr;  // Claude Generated 2026 - RMSD/align panel (Editors dock tab)
    AtomListPanel* m_atomListPanel = nullptr;  // Claude Generated Phase 2C - Atom list panel
    SnapshotsWidget* m_snapshotsWidget = nullptr;  // Claude Generated 2026 - Snapshot history

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

    // Claude Generated 2026 - Molecule save state. m_currentMoleculeFilePath
    // is the path of the most recently *loaded* structure (XYZ/VTF/...). The
    // save flow re-uses it as the default target if the extension is .xyz,
    // otherwise it falls back to a Save-As dialog. m_structureModified tracks
    // whether the in-memory geometry has diverged from that source (e.g. after
    // an MD/Opt run) and is consulted by loadMoleculeFile() to prompt the
    // user with Save/Discard/Cancel.
    QString m_currentMoleculeFilePath;
    bool m_structureModified = false;

    // Claude Generated 2026 - In-memory lesson being authored (Save as Lesson…).
    // Structures are appended via addCurrentStructureToLesson(); metadata via the
    // LessonMetadataDialog. See lesson.h for the data model.
    Lesson m_lesson;
    // Path of the currently open/last-saved *.qlesson.json. Set by openLesson()
    // and saveLesson(); lets "Save Lesson" overwrite it directly (no re-pick).
    QString m_lessonFilePath;
    bool saveLessonInteractive(bool forceDialog);  // resolve target + call saveLesson()
    // In-memory lesson-structure list. Shown in the EXISTING file browser
    // (m_directoryContentView) by swapping its model — no second view. A small
    // "Files | Lesson" combo in the project dock toggles the mode. Claude Generated 2026.
    LessonStructureModel* m_lessonStructureModel = nullptr;
    QComboBox* m_browserModeCombo = nullptr;
    bool m_lessonBrowseMode = false;  // true => content view shows the lesson model
    void setBrowserMode(bool lessonMode);
    bool m_centerOnLoad = true;  // shift COM to origin after loading (from VisualizationSettings)
    QAction* m_saveAction = nullptr;
    QAction* m_saveAsAction = nullptr;
    bool saveStructure(const QString& path = QString());

    // Claude Generated 2026 - Snapshot history. m_snapshots[0] is always the
    // geometry as it was when the molecule was loaded (or the editor was last
    // applied). Higher indices are user-managed or auto-stride snapshots. The
    // policy is intentionally simple for now (manual add/reset, no automatic limit).
    // MoleculeSnapshot is defined globally in snapshotswidget.h so the widget and
    // MainWindow share one type.
    QVector<MoleculeSnapshot> m_snapshots;
    void takeSnapshot(const QString& name = QString());
    void restoreSnapshot(const MoleculeSnapshot& snapshot);
    void resetToOriginalSnapshot();
    void captureInitialSnapshot(const QString& filePath,
        const QVector<MoleculeViewer::Atom>& atoms,
        const QVector<MoleculeViewer::Bond>& bonds);

#ifdef USE_SFTP
    QMenu* m_recentConnectionsMenu = nullptr;
    void updateRecentConnectionsMenu();
    void openRecentConnection(const QString& profileId);
#endif

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

    // Claude Generated 2026 - "Use Invocation Directory" preference
    QString m_invocationDir;                       // captured from QDir::currentPath() in main()
    bool m_useInvocationDirectoryEnabled = false;  // mirror of the QSettings bool
    QAction* m_useInvocationDirAction = nullptr;   // settings-menu checkable action
    void applyUseInvocationDirectoryState(bool enabled);

    // Claude Generated - Remote Directory Mounting
    QTreeWidget* m_remoteDirectoriesView = nullptr;
#ifdef USE_SFTP
    QMap<QString, SftpItemModel*> m_remoteSftpModels;
    QString m_currentRemoteMountId;
#endif

    // Claude Generated - Dock architecture rewrite (2026-04): 5 docks rahmen MoleculeViewer (CentralWidget)
    QDockWidget* m_projectDock = nullptr;           // Left: working dir + calculation files
    QDockWidget* m_navigationDock = nullptr;        // Left (tabbed): bookmarks / workspaces / remote
    QDockWidget* m_editorsDock = nullptr;           // Right: structure + input editors (internal QTabWidget)
    QDockWidget* m_atomsSimulationDock = nullptr;   // Right: atom list + simulation (internal QTabWidget)
    QDockWidget* m_outputViewDock = nullptr;        // Bottom: output log
    QDialog* m_simulationChartDialog = nullptr;     // Modeless dialog: live MD temperature/energy charts
    QTabWidget* m_editorsTabs = nullptr;            // Internal tabs inside m_editorsDock (Structure/Input/RMSD)
    QTabWidget* m_atomsSimulationTabs = nullptr;    // Internal tabs inside m_atomsSimulationDock
    QTabWidget* m_navigationTabs = nullptr;         // Internal tabs inside m_navigationDock
    QByteArray m_defaultDockState;                  // Baseline after createDockWidgets
    QHash<int, QByteArray> m_presetStates;          // keyed by LayoutPreset enum

    // Claude Generated 2026 - P2: Explore/Compute mode switch
    AppMode m_appMode = AppMode::Explore;
    QToolBar* m_modeToolbar = nullptr;          // top row: [Explore | Compute]
    QToolBar* m_calculationToolbar = nullptr;   // 2nd row: program/command/threads (Compute only)
    QToolButton* m_exploreButton = nullptr;
    QToolButton* m_computeButton = nullptr;
    SimulationControlWidget* m_simulationControlWidget = nullptr;  // Claude Generated
    SimulationChartWidget* m_simulationChartWidget = nullptr;     // Claude Generated 2026 - live T/energy charts
    SimulationConfig m_simulationConfig;             // Claude Generated - Shared config, edited from dock

    // Claude Generated - Interactive Simulation Integration
    QElapsedTimer m_simStatusBarTimer;  // Throttle status bar updates to ~5 Hz
    void wireSimulationWorker(SimulationWorker* worker);  // Claude Generated - Direct worker->view wiring
    void onSimulationConfigChanged(SimulationConfig cfg);

#ifdef USE_SFTP
    void updateRemoteDirectoriesView();
    void onRemoteDirectoryClicked(QTreeWidgetItem* item, int column);
    void onAddRemoteDirectoryClicked();
    void onRemoteFileDoubleClicked(const QModelIndex& index);
    void downloadAndLoadRemoteFile(const QString& filePath);
#endif

protected:
    // Claude Generated - Quick Win: Drag & Drop support
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    // Claude Generated 2026 - Application-level key filter: WASD = pitch/yaw, QE = roll
    // rotate the 3D scene (and Shift+WASDQE nudges the selection). Only active in the
    // viewer's Edit mode, so the keys stay free everywhere else; also skipped while a
    // text-entry widget has focus or Ctrl/Alt/Meta is held. Installed on qApp.
    bool eventFilter(QObject* obj, QEvent* event) override;
};