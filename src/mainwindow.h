#pragma once

#include "core/tool.h"  // Claude Generated 2026 - ToolAutonomy
#include "docks/dockconfig.h"  // Claude Generated 2026 - Dock system restructuring
#include "settings.h"
#include "vtfparser.h"
#include "xyzparser.h"
#include "pdbparser.h"  // Claude Generated - Phase 5C
#include "mol2parser.h"  // Claude Generated - Phase 5C
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDateTime>
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
#include <QSet>
#include <QMap>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointer>  // Claude Generated - For dialog pointer management
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QProgressDialog>
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
class QActionGroup;  // Claude Generated 2026 - NCI source radio group
class DisplayPanel;  // Claude Generated 2026 - docked viewer display options (replaces the modal dialog)
class CommandPalette;  // Claude Generated 2026 - P3 Ctrl+K command palette
class RMSDWidget;  // Claude Generated 2026 - RMSD / align tool (Analysis dock)
class WorkspaceManager;  // Claude Generated Phase 4 - Workspace management
class AtomListPanel;  // Claude Generated Phase 2C - Atom list panel with table view
class DockManager;          // Claude Generated 2026 - owns all docks and layout presets
class OutputDock;           // Claude Generated 2026 - Output dock wrapper
class SimulationDock;       // Claude Generated 2026 - Simulation dock wrapper
class DisplayDock; // Claude Generated 2026 - Structure & Display dock wrapper
class ProjectDock;            // Claude Generated 2026 - Project dock wrapper
class ImageGalleryDock;       // Claude Generated 2026 - batch border-trim gallery (bottom)
class NciDock;                // Claude Generated 2026 - non-covalent interaction dock
class NciAnalysisWorker;      // Claude Generated 2026 - off-thread NCI analysis
class QThread;
class QSortFilterProxyModel;  // Claude Generated 2026 - ProjectDock file filter proxy
#ifdef USE_SFTP
class SftpItemModel;  // Claude Generated - Remote Directory Mounting
#endif
class SimulationControlWidget;  // Claude Generated - Interactive Simulation Integration
class LessonController;          // Claude Generated 2026 - WP T4 lesson feature controller
class ChartDock;                // Claude Generated 2026 - live charts dock
class QDialog;                  // Claude Generated 2026 - host for the modeless charts dialog
class CalculationRunner;        // Claude Generated 2026 - WP T3 external-process orchestration
class ToolDispatcher;           // Claude Generated 2026 - tool layer (core/tooldispatcher.h)
class CurcumaJob;               // Claude Generated 2026 - in-process calculation
struct ToolResult;              // Claude Generated 2026 - core/tool.h
#ifdef USE_LLM
class ChatDock;                 // Claude Generated 2026 - docks/chatdock.h
class LlmClient;                // Claude Generated 2026 - llm/llmclient.h
class LlmSession;               // Claude Generated 2026 - llm/llmsession.h
struct ToolSpec;                // Claude Generated 2026 - core/tool.h
#endif


// CalculationEntry + the calculations.json persistence live here now.
#include "calculationhistory.h"

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
     * @brief Switch the working directory to a directory given on the command line
     *        (e.g. `qurcuma .`).
     *
     * Claude Generated 2026: The CLI '.'/<dir> case switches the working directory
     * without loading a file. Called from main.cpp after the event loop starts, so
     * it has access to switchWorkingDirectory().
     *
     * @param dir  Absolute directory path. If empty, no-op.
     */
    void loadDirFromArg(const QString& dir);

private slots:
    void runCommand();
    void programSelected(int index);
    void projectSelected(const QModelIndex &index);
    // Claude Generated 2026 - WP T3: finish handling for a CalculationRunner run.
    void onCalculationFinished(const CalculationEntry& entry, int exitCode);
    void configurePrograms();
    void configureOperatorMetadata();  // Claude Generated 2026 - operator name/ORCID/institution/license
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
    /// Let a focused text widget answer Ctrl+C itself; true when it did. A helper,
    /// not a slot: the Edit-menu Copy calls it before falling back to the
    /// structure. Claude Generated 2026.
    bool copySelectedTextFromFocusWidget();
#ifdef USE_LLM
    /// How much the assistant may do unasked; set in the Assistant dock, kept in
    /// QSettings, consulted by approveToolCall(). Claude Generated 2026.
    ToolAutonomy m_autonomy = ToolAutonomy::Ask;
#endif

    void setupUI();
    void createToolbars();
    void createModeBar();                       // Claude Generated 2026 - P2 Explore/Compute switch
    void setAppMode(DockConfig::AppMode mode, bool reflow = true);  // apply mode (toolbar + dock visibility)
    void showCommandPalette();
    /// Show a tool's answer in the Output dock and the status bar. Claude Generated 2026.
    void showToolResult(const QString& name, const ToolResult& result);
#ifdef USE_LLM
    /// Create the client and session, attach the dock, load the endpoint profiles.
    /// Claude Generated 2026.
    void setupAssistant();
    /// Apply the named endpoint profile to the client, reading its key from the
    /// environment. Reports what is missing rather than failing silently.
    void applyLlmProfile(const QString& name);
    /// Ask before a tool that calculates, writes or mutates. Claude Generated 2026.
    bool approveToolCall(const ToolSpec& spec, const QJsonObject& args);
#endif                  // Claude Generated 2026 - P3 Ctrl+K palette
    void createMenus();
    void seedRMSDReference();  // Claude Generated 2026 - re-seed RMSD reference from viewer
    void setupProjectViewContextMenu();
    void setupConnections();
    void setupShortcuts();  // Claude Generated - Phase 1.2
    void loadSettings();

    bool setupCalculationDirectory();
    void updateOutputView(const QString& logFile, bool scrollToBottom = false);

    // Claude Generated - Phase 4.1: Enhanced error dialog
    void showEnhancedError(const QString& title, const QString& problem, const QString& solution,
                          std::function<void()> actionCallback = nullptr);

    // Claude Generated - SFTP: Load molecule file (local or remote)
    void loadMoleculeFile(const QString& filePath);

    // Claude Generated 2026 - WP T4: the OER-lesson feature (open/save/add/apply +
    // Files|Lesson browse mode + metadata/detail editors) lives in LessonController.
    // This handles the MainWindow bookkeeping after an in-memory lesson structure is
    // loaded into the viewer (clear file path, reset modified state, snapshot).
    void onLessonStructureLoaded(const QString& name);

    // Claude Generated 2026 - Bidirectional structure sync (viewer <-> atom table
    // <-> structure text editor). The viewer is the canonical store; m_structSyncing
    // breaks feedback loops.
    void updateAtomTableFromViewer();     // push viewer geometry -> atom table
    void updateStructureTextFromViewer(); // push viewer geometry -> text editor (XYZ)
    void applyStructureTextToViewer();    // parse editor text -> viewer ("Apply")

    // Claude Generated 2026 - Parse just the first frame of a structure file (xyz/vtf/
    // pdb/mol2) into viewer atoms/bonds; used by addMoleculeToScene() to merge.
    bool parseFirstFrame(const QString& filePath, QVector<MoleculeViewer::Atom>& atoms,
        QVector<MoleculeViewer::Bond>& bonds);

    void setupProgramSpecificDirectory(const QString &dirPath, const QString &program);
    void updateDirectoryContent();  // Claude Generated - removed unused path parameter

    QPair<int, int> countImaginaryFrequencies(const QString &filename);
    void updateCommandLineVisibility(const QString &program);
    void setupContextMenu();
    void openWithVisualizer(const QString &filePath, const QString &visualizer);
    void orcaPlotVib(const QString &outputFile, int freqNumber);
    void syncRightView();  // Claude Generated - removed unused path parameter
    // Path helpers - Claude Generated for clarity
    QString currentCalculationDir() const {
        return QDir(m_workingDirectory).filePath(m_currentCalculationDir);
    }
    bool isValidCalculationDir() const {
        return !m_currentCalculationDir.isEmpty() &&
               m_currentCalculationDir != "." &&
               m_currentCalculationDir != "..";
    }
    QStringList currentSubdirectories() const;

    void switchWorkingDirectory(const QString& path);

    // Claude Generated - Quick Win: Recent files management
    void addToRecentFiles(const QString& path);
    void updateRecentFilesMenu();
    void openRecentFile(const QString& path);

    void updatePathLabel(const QString& path);  // Claude Generated Phase 1 - Updates breadcrumb bar
    void toggleLeftPanel();
    void updateWorkflowState(WorkflowState state);  // Claude Generated - Phase 2.2

    // Claude Generated Phase 3.2 - Bookmark and workspace slots
    void refreshBookmarkTree();  // Phase 3: delegates to BookmarkWidget
    void saveCurrentWorkspace();
    void onWorkspaceItemClicked(QListWidgetItem* item);
    void onWorkspaceContextMenu(const QPoint& pos);
    void restoreWorkspaceState(const Settings::Workspace& ws);
    void updateWorkspaceList();

    // Claude Generated - UI Restructuring: Layout preset management
    void applyLayoutPreset(DockConfig::LayoutPreset preset);
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
    QPushButton *m_newCalculationButton, *m_chooseDirectory, *m_runCalculation;
    QCheckBox* m_uniqueFileNames;
    QSpinBox* m_threads;
    QFileSystemModel* m_projectModel;
    QFileSystemModel* m_directoryContentModel;
    QSortFilterProxyModel* m_directoryContentProxyModel = nullptr;
    Settings m_settings;
    // Claude Generated 2026 - WP T3: owns the calculation QProcess + completer commands.
    CalculationRunner* m_calculationRunner = nullptr;
    ToolDispatcher* m_toolDispatcher = nullptr;  // Claude Generated 2026 - runs tools on the right thread
    CurcumaJob* m_curcumaJob = nullptr;          // Claude Generated 2026 - one calculation at a time
#ifdef USE_LLM
    // Claude Generated 2026 - The assistant. The session owns the conversation and
    // the approval policy; the dock is only a view onto it.
    ChatDock* m_chatDock = nullptr;
    LlmClient* m_llmClient = nullptr;
    LlmSession* m_llmSession = nullptr;
    QSet<QString> m_toolsAllowedForSession;  ///< "allow for this session", per tool
#endif

    // Claude Generated 2026 - Docked viewer display options (replaces the modal dialog)
    DisplayPanel* m_displayPanel = nullptr;
    CommandPalette* m_commandPalette = nullptr;  // Claude Generated 2026 - P3 Ctrl+K
    QCompleter* m_commandCompleter;
    BreadcrumbBar* m_breadcrumbBar;  // Claude Generated Phase 1 - Clickable path navigation
    QLabel *m_currentProjectLabel;
    // Claude Generated - Phase 3.3: Visual state indicators
    QLabel *m_stateIcon, *m_stateIndicator;
    MoleculeViewer *m_moleculeView;
    NMRSpectrumDialog* m_nmrDialog;
    RMSDWidget* m_rmsdWidget = nullptr;  // Claude Generated 2026 - RMSD/align panel (Editors dock tab)
    AtomListPanel* m_atomListPanel = nullptr;  // Claude Generated Phase 2C - Atom list panel
    SnapshotsWidget* m_snapshotsWidget = nullptr;  // Claude Generated 2026 - Snapshot history

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

    // Claude Generated 2026 - WP T3: periodic re-read of the running calculation's
    // log file (the process redirects stdout/stderr there) into the output dock.
    QTimer* m_outputUpdateTimer = nullptr;
    QString m_currentOutputFile;

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
    bool m_structSyncing = false;  // Claude Generated 2026 - re-entrancy guard for viewer/table/text sync

    // Resolves a view index from the content list to a filesystem path. Handles
    // the QSortFilterProxyModel introduced by ProjectDock. Claude Generated 2026.
    QString filePathFromContentIndex(const QModelIndex& viewIndex) const;

    // Claude Generated 2026 - WP T4: the OER-lesson feature (data + open/save/add/apply
    // + Files|Lesson browse mode + the metadata/detail editors) is owned by
    // LessonController. MainWindow keeps only the two mode buttons — the eventFilter
    // drop-target compares against m_lessonModeBtn — and delegates the rest.
    LessonController* m_lessonController = nullptr;
    QToolButton* m_filesModeBtn = nullptr;
    QToolButton* m_lessonModeBtn = nullptr;
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
    /// Claude Generated 2026 - Fill the confinement container with randomly placed
    /// copies of library molecules (Build strip "Fill", Molecule menu).
    void fillContainer();
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

    // Claude Generated 2026 - Dock system restructuring: manager owns all docks,
    // layout presets and the Explore/Compute mode. MainWindow coordinates via signals.
    DockManager* m_dockManager = nullptr;

    // Claude Generated - Dock architecture rewrite (2026-04): 5 docks rahmen MoleculeViewer (CentralWidget)
    // NOTE: these are being migrated into DockManager / src/docks/ wrappers.
    ProjectDock* m_projectDock = nullptr;           // Left: Project dock with Files/Bookmarks/Workspaces/Remote segments
    DisplayDock* m_displayDock = nullptr; // Right: [Structure | Atoms] segment + Display panel
    SimulationDock* m_simulationDock = nullptr;     // Right: Simulation/Snapshots/RMSD/Input tabs (tabified with Structure&Display)
    OutputDock* m_outputViewDock = nullptr;         // Bottom: output log
    ImageGalleryDock* m_imageGalleryDock = nullptr; // Bottom (tabified): batch border-trim gallery
    NciDock* m_nciDock = nullptr;                   // Right (tabified): non-covalent interaction contacts
    ChartDock* m_chartDock = nullptr;               // Live charts (owned by DockManager)
    QTabWidget* m_simulationTabs = nullptr;         // Internal tabs inside m_simulationDock

    // Claude Generated 2026 - P2: Explore/Compute mode switch
    DockConfig::AppMode m_appMode = DockConfig::AppMode::Explore;
    QToolBar* m_modeToolbar = nullptr;          // top row: [Explore | Compute]
    QToolBar* m_calculationToolbar = nullptr;   // 2nd row: program/command/threads (Compute only)
    QToolButton* m_exploreButton = nullptr;
    QToolButton* m_computeButton = nullptr;
    SimulationControlWidget* m_simulationControlWidget = nullptr;  // Claude Generated
    SimulationConfig m_simulationConfig;             // Claude Generated - Shared config, edited from dock

    // Claude Generated - Interactive Simulation Integration
    QElapsedTimer m_simStatusBarTimer;  // Throttle status bar updates to ~5 Hz
    void wireSimulationWorker(SimulationWorker* worker);  // Claude Generated - Direct worker->view wiring

    // Claude Generated 2026 - Non-covalent interaction analysis. The worker lives
    // on its own thread for the whole session so an analysis can run while an MD
    // does (the simulation thread is busy with its MD timer).
    void setupNciAnalysis();
    /// Send the current frame to the analysis worker. @p source is 2 (GFN-FF
    /// parameters) or 3 (population analysis); anything else is ignored.
    void startNciAnalysis(int source);
    QThread* m_nciThread = nullptr;
    NciAnalysisWorker* m_nciWorker = nullptr;
    quint64 m_nciRequestId = 0;        // monotonic; late results for old frames are dropped
    bool m_nciLiveMd = false;          // keep the GFN-FF contact list live during MD
    bool m_nciSelectionSyncing = false; // guards table <-> viewer selection feedback
    // Claude Generated 2026 - NCI quick access: one shared action set feeds the
    // Display menu, the viewer-bar button dropdown and the command palette.
    QAction* m_nciToggleAction = nullptr;   // checkable, shortcut N
    QMenu* m_nciSourceMenu = nullptr;       // Off/Geometry/GFN-FF/GFN2 radio group
    QActionGroup* m_nciSourceGroup = nullptr;
    int m_lastNciSource = 1;                // source restored on toggle-on (1 = geometry)
    /// Toggle the NCI overlay: off -> last-used source, on -> off.
    void toggleNciOverlay();
    /// Apply a source picked in the menu/bar dropdown (>= 2 starts the analysis).
    void setNciSourceFromUi(int source);
    // Claude Generated 2026 - Shared display actions (Display menu + viewport
    // context menu); checked states mirror the viewer's signals.
    QActionGroup* m_renderStyleGroup = nullptr;
    QActionGroup* m_colorSchemeGroup = nullptr;
    QActionGroup* m_labelModeGroup = nullptr;
    QAction* m_fitViewAction = nullptr;
    QMenu* m_displayMenu = nullptr;         // reused as the viewport context menu
    /// Esc: cancel a running calculation, else clear selection/measurement.
    void handleEscape();
    /// Build/show the viewport context menu from the shared display actions.
    void showViewportContextMenu(const QPoint& globalPos, int atomIndex);
    /// One-click PNG export (viewer-bar Photo button, context menu).
    void quickExportPhoto();
    /// File ▸ New Scene: clear the scene and enter Build mode. Claude Generated 2026.
    void newScene();
    /// Edit ▸ Undo (Ctrl+Z): restore + consume the newest snapshot. Claude Generated 2026.
    void undoLastSnapshot();
    // Claude Generated 2026 - Permanent status-bar indicators (file · atoms · frame),
    // so the current state survives the transient showMessage() notices.
    QLabel* m_statusFileLabel = nullptr;
    QLabel* m_statusAtomsLabel = nullptr;
    QLabel* m_statusFrameLabel = nullptr;
    void updateStatusIndicators();
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