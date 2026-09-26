#include "settings.h"
#include "elementdata.h"  // Claude Generated 2026 - element validation for the context menu
#include "fragmentlibrary.h"  // Claude Generated 2026 - builder fragment templates
#include "selectionmanager.h"  // Claude Generated - Phase 2A
#include "atomlistpanel.h"  // Claude Generated - Phase 2C
#ifdef USE_SFTP
#include "sftpmodel.hpp"
#include "dialogs/sftpdialog.h"
#endif
#include "simulationcontrolwidget.h"  // Claude Generated - Interactive Simulation Integration
#include "snapshotswidget.h"  // Claude Generated 2026 - Snapshot history foundation
// Claude Generated 2026 - WP T4: lesson widgets/model/dialog moved into LessonController.
// Claude Generated 2026 - Phase 6: SimulationDialog removed; the dock widget is the sole sim UI.
#include <algorithm>  // Claude Generated - for std::min/std::max
#include <QAbstractSpinBox>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QCompleter>
#include <QDialog>
#include <QDir>
#include <QFormLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QDockWidget>  // Claude Generated - Phase 2C - For AtomListPanel dock
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QInputDialog>
#include <QScopeGuard>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMap>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointer>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QCloseEvent>
#include <QScrollBar>
#include <QButtonGroup>
#include <QSettings>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringListModel>
#include <QSysInfo>
#include <QTableWidget>
#include <QThread>
#include <QTime>
#include <QTimer>
#include <QElapsedTimer>
#include <QToolBar>
#include <QToolButton>

#include <QFile>
#include <QTextStream>
#include <QString>
#include "view.h"
#include "moleculefileloader.h"  // Claude Generated 2026 - unified structure-file reader
#include "calculationrunner.h"  // Claude Generated 2026 - WP T3 external-process orchestration
#include "lessoncontroller.h"  // Claude Generated 2026 - WP T4 lesson feature controller
#include "frequencydialog.h"
#include "displaypanel.h"
#include "widgets/commandpalette.h"
#include "widgets/simulationchart.h"  // Claude Generated 2026 - live MD temperature/energy charts

#include "dialogs/nmrspectrumdialog.h"
#include "rmsdwidget.h"  // Claude Generated 2026 - RMSD / align tool (Analysis dock)
#ifdef USE_SFTP
#include "dialogs/sftpdialog.h"
#endif
#include "workspacemanager.h"  // Claude Generated Phase 4
#include "docks/dockmanager.h"  // Claude Generated 2026 - Dock system restructuring
#include "docks/simulationdock.h"  // Claude Generated 2026 - Dock system restructuring
#include "docks/structuredock.h"  // Claude Generated 2026 - Dock system restructuring
#include "docks/appearancedock.h"  // Claude Generated 2026 - UX stage 4
#include "docks/outputdock.h"  // Claude Generated 2026 - Dock system restructuring
#include "docks/bookmarkwidget.h"  // Claude Generated 2026 - Dock system restructuring
#include "docks/workspacepanel.h"  // Claude Generated 2026 - Dock system restructuring
#include "docks/remotedirectoriespanel.h"  // Claude Generated 2026 - Dock system restructuring
#include "docks/projectdock.h"  // Claude Generated 2026 - Dock system restructuring
#include "docks/imagegallerydock.h"
#include "docks/ncidock.h"
#include "ncianalysisworker.h"
#include "nciwidget.h"  // Claude Generated 2026 - batch border-trim gallery
#include "mainwindow.h"

// Claude Generated - Conditional debug logging
#ifdef QT_DEBUG
#define DEBUG_LOG qDebug()
#else
#define DEBUG_LOG if(false) qDebug()
#endif

// Claude Generated 2026 - "Use Invocation Directory" preference.
// invocationDir is captured from QDir::currentPath() in main.cpp BEFORE
// QApplication is created. When useInvocationDirectoryEnabled() is true,
// this directory becomes the active Working Directory.
MainWindow::MainWindow(const QString& invocationDir, QWidget *parent)
    : QMainWindow(parent)
{
    m_invocationDir = invocationDir;
    // Claude Generated 2026 - WP T3: the runner owns the calculation QProcess and the
    // per-program completer commands. Created first so setupConnections() can wire it.
    m_calculationRunner = new CalculationRunner(this);

    // Claude Generated 2026 - Dock system restructuring: manager owns all docks,
    // presets and Explore/Compute mode. Construction happens before setupUI() so
    // createDockWidgets() can delegate to it in later phases.
    m_dockManager = new DockManager(this, this);

    // Claude Generated - Quick Fix: Set window title and version
    setWindowTitle("Qurcuma 1.0 - Molecular Visualization");

    setupUI();
    createToolbars();
    createMenus();
    createModeBar();   // Claude Generated 2026 - P2/P4: menu-bar corner widget (after the menu bar exists)
    setupConnections();
    // Claude Generated 2026 - App-level key filter for WASD/QE scene rotation (see eventFilter).
    qApp->installEventFilter(this);
    setupProjectViewContextMenu();  // Enable right-click on calculation directories
    setupShortcuts();  // Claude Generated - Phase 1.2
    loadDrafts();      // Claude Generated - Quick Win: Auto-save drafts

    // Arbeitsverzeichnis aus Settings laden (must be AFTER createDockWidgets which creates m_projectListView)
    m_workingDirectory = m_settings.workingDirectory();
    if (!m_workingDirectory.isEmpty()) {
        m_projectModel->setRootPath(m_workingDirectory);
        m_projectListView->setRootIndex(m_projectModel->index(m_workingDirectory));
    }
    QString lastDir = m_settings.lastUsedWorkingDirectory();
    if (!lastDir.isEmpty() && QDir(lastDir).exists()) {
        switchWorkingDirectory(lastDir);
    }

    // Claude Generated 2026 - "Use Invocation Directory" preference.
    // The invocation dir was captured at the top of the constructor; here we
    // read the persisted toggle, reflect it on the menu action, and apply
    // switchWorkingDirectory if the user opted in. If the dir is missing
    // (e.g. USB stick gone), switchWorkingDirectory shows a warning and the
    // last-used dir stays.
    m_useInvocationDirectoryEnabled = m_settings.useInvocationDirectoryEnabled();
    if (m_useInvocationDirAction) {
        m_useInvocationDirAction->setChecked(m_useInvocationDirectoryEnabled);
    }
    if (m_useInvocationDirectoryEnabled
        && !m_invocationDir.isEmpty()
        && QDir(m_invocationDir).exists()) {
        switchWorkingDirectory(m_invocationDir);
    }

    m_nmrDialog = new NMRSpectrumDialog(this);

    // Claude Generated - Visual Polish: Load dark mode setting and update checkbox
    m_darkModeEnabled = m_settings.darkModeEnabled();
    if (m_darkModeAction) {
        m_darkModeAction->setChecked(m_darkModeEnabled);
    }
    applyStylesheet(m_darkModeEnabled);
}

MainWindow::~MainWindow()
{
    // Claude Generated 2026 - The NCI analysis thread outlives individual jobs, so
    // it has to be stopped here; QThread would otherwise be destroyed while running.
    if (m_nciThread) {
        m_nciThread->quit();
        m_nciThread->wait();
    }
}

void MainWindow::setupUI()
{
    // Claude Generated - Quick Win: Enable drag and drop
    setAcceptDrops(true);

    // Claude Generated (2026-04) - Dock rewrite: MoleculeViewer is the real central widget.
    // Replaces the old 1x1 dummy — fixes dock resize math and eliminates the tab-support hack.
    m_moleculeView = new MoleculeViewer;
    // Claude Generated 2026 - The ONLY place persisted display settings are pushed
    // into the viewer: the last session's state, saved on exit (closeEvent). From here
    // on the viewer is the source of truth; the Display panel only reads
    // (syncFromViewer), looks set their own fields (applyLook), Reset applies defaults.
    m_settings.dropLegacyDisplayPresetsOnce();  // UX stage 3: old presets are not migrated
    const Settings::VisualizationSettings vizSettings = m_settings.getVisualizationSettings();
    m_moleculeView->applyDisplaySettings(vizSettings);
    m_moleculeView->setInstancingThreshold(vizSettings.instancingThreshold);
    m_moleculeView->setBeadTypeColors(m_settings.beadTypeColors());
    m_moleculeView->setNciPalette(m_settings.nciPalette());
    m_centerOnLoad = vizSettings.centerOnLoad;
    m_nciLiveMd = vizSettings.nciLiveMd;
    setCentralWidget(m_moleculeView);

    // Claude Generated 2026 - Dock refactor: set dock options and tab positions
    // BEFORE creating/placing docks so tabify/split calls inherit the right config.
    QMainWindow::DockOptions dockOptions = QMainWindow::AllowTabbedDocks |
                                           QMainWindow::AnimatedDocks |
                                           QMainWindow::AllowNestedDocks;
    // Claude Generated 2026 - GroupedDragging (drag a whole tab group; floating tab groups)
    // stays off under Wayland. Re-docking a floating tab group crashes inside Qt 6.11:
    // QMainWindowLayout::animationFinished() moves the group's docks into a new sub-layout
    // and calls reparentWidgets() on it; after that call the sub-layout pointer reads back
    // as nullptr (the relayout it triggers has modified the item), and Qt then calls
    // setTabBarShape() on it. Floating tab groups are also found by window geometry, which
    // Wayland does not report, so they could not be formed reliably there anyway. Qt creates
    // the group windows only with this option (and when restoring a saved layout with one).
    if (!QGuiApplication::platformName().startsWith(QLatin1String("wayland"), Qt::CaseInsensitive))
        dockOptions |= QMainWindow::GroupedDragging;
    setDockOptions(dockOptions);

    setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
    setTabPosition(Qt::RightDockWidgetArea, QTabWidget::North);
    setTabPosition(Qt::TopDockWidgetArea, QTabWidget::North);
    setTabPosition(Qt::BottomDockWidgetArea, QTabWidget::North);

    // Claude Generated - UI Restructuring: Create all dock widgets
    createDockWidgets();

    // Setup context menu for file list
    setupContextMenu();

    // Update initial state
#ifdef USE_SFTP
    updateRemoteDirectoriesView();
#endif

    // Claude Generated 2026 - Every other shortcut sits on a menu QAction (createMenus),
    // so it is visible, palette-listed and registered exactly once. Esc steps back
    // one level (handleEscape) and has no menu home.
    new QShortcut(Qt::Key_Escape, this, this, &MainWindow::handleEscape);               // cancel calc / clear selection

    // Claude Generated 2026 - P3 command palette: Ctrl+K is carried by the View ▸ Command
    // Palette menu action (P4); no standalone QShortcut here to avoid an ambiguous overload.

    // Initial updates
    updatePathLabel(m_workingDirectory);
    refreshBookmarkTree();

    // Window settings
    resize(1400, 900);  // Larger default size for flexible docking
    setWindowTitle("Qurcuma");

    // Claude Generated (2026-04) - Dock rewrite: capture baseline after Qt finished
    // placement, then prefer the globally persisted layout from QSettings. Without
    // one (first run, reset layout version) the saved mode lays out the docks.
    // State capture/restore is owned by DockManager.
    QTimer::singleShot(0, this, [this]() {
        // DockManager owns layout persistence: it restores both window geometry
        // and dock state (no separate geometry restore here — that double-restored).
        // Claude Generated 2026 - UX stage 4 changed the dock set; workspaces saved before
        // it keep their directory and name but drop their dock layout once (operator
        // decision). Must run before restoreSavedLayout(), which stores the new version.
        if (m_workspaceManager
            && QSettings().value(DockConfig::UiLayoutVersionKey, 0).toInt() < DockConfig::UiLayoutVersion) {
            for (Settings::Workspace ws : m_workspaceManager->listWorkspaces()) {
                if (!ws.dockState.isEmpty()) {
                    ws.dockState.clear();
                    m_workspaceManager->saveWorkspace(ws);
                }
            }
        }
        bool restored = false;
        if (m_dockManager) {
            m_dockManager->captureBaselineState();
            restored = m_dockManager->restoreSavedLayout();
        }
        // Claude Generated 2026 - Enforce the saved mode last so the calculation toolbar,
        // dock visibility and lesson browser match it (default Explore on first run).
        // Without a restored layout the mode also sizes the docks.
        int savedMode = QSettings().value(DockConfig::UiAppModeKey,
                                          static_cast<int>(DockConfig::AppMode::Explore)).toInt();
        if (savedMode < 0 || savedMode > static_cast<int>(DockConfig::AppMode::Teaching))
            savedMode = static_cast<int>(DockConfig::AppMode::Explore);
        setAppMode(static_cast<DockConfig::AppMode>(savedMode), /*reflow=*/!restored);
    });
}

void MainWindow::createToolbars()
{
    // Claude Generated (2026-04) - Dock rewrite: program controls moved from Top dock
    // into a proper toolbar. Main toolbar carries the full calculation command line.
    QToolBar* toolbar = new QToolBar(tr("Calculation"), this);
    m_calculationToolbar = toolbar; // Claude Generated 2026 - P2: hidden in Explore mode
    toolbar->setObjectName("CalculationToolbar");
    toolbar->setMovable(true);
    toolbar->setIconSize(QSize(18, 18));

    toolbar->addWidget(new QLabel(tr("Program: ")));
    m_programSelector = new QComboBox;
    m_programSelector->addItems(m_simulationPrograms);
    m_programSelector->setToolTip(tr("Choose computational chemistry program"));
    m_programSelector->setMinimumWidth(110);
    toolbar->addWidget(m_programSelector);

    toolbar->addSeparator();

    m_commandInput = new QLineEdit;
    m_commandInput->setPlaceholderText(tr("Enter command..."));
    m_commandInput->setToolTip(tr("Program-specific command arguments"));
    m_commandInput->setMinimumWidth(240);
    m_commandCompleter = new QCompleter(this);
    m_commandCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_commandCompleter->setFilterMode(Qt::MatchContains);
    m_commandInput->setCompleter(m_commandCompleter);
    toolbar->addWidget(m_commandInput);

    toolbar->addSeparator();

    toolbar->addWidget(new QLabel(tr(" Threads: ")));
    m_threads = new QSpinBox;
    m_threads->setRange(1, QThread::idealThreadCount());
    m_threads->setValue(1);
    m_threads->setToolTip(tr("Number of parallel threads for calculation"));
    toolbar->addWidget(m_threads);

    m_uniqueFileNames = new QCheckBox(tr("Unique filenames"));
    m_uniqueFileNames->setToolTip(tr("Append timestamp to output filenames"));
    toolbar->addWidget(m_uniqueFileNames);

    m_runCalculation = new QPushButton(tr("Start"));
    m_runCalculation->setIcon(QIcon::fromTheme("system-run"));
    m_runCalculation->setToolTip(tr("Start calculation with selected program (Ctrl+R)"));
    toolbar->addWidget(m_runCalculation);

    toolbar->addSeparator();

    m_timerLabel = new QLabel("00:00:00");
    m_timerLabel->setStyleSheet("font-weight: bold; color: #0066cc;");
    m_timerLabel->setMinimumWidth(70);
    m_timerLabel->setToolTip(tr("Elapsed calculation time"));
    toolbar->addWidget(m_timerLabel);

    // NMR Spectra lives in the Tools menu (UX stage 5).
    addToolBar(Qt::TopToolBarArea, toolbar);
}

void MainWindow::setupContextMenu()
{
    connect(m_directoryContentView, &QListView::customContextMenuRequested,
        [this](const QPoint& pos) {
            QModelIndex index = m_directoryContentView->indexAt(pos);
            if (!index.isValid())
                return;

            // Claude Generated 2026 - Lesson mode: the view shows in-memory lesson
            // structures, so offer Load / Remove instead of the file actions.
            if (m_lessonController->browseMode()) {
                QMenu menu(this);
                QAction* loadAct = menu.addAction(tr("Load Structure"));
                QAction* removeAct = menu.addAction(tr("Remove from Lesson"));
                QAction* chosen = menu.exec(m_directoryContentView->viewport()->mapToGlobal(pos));
                if (chosen == loadAct) {
                    m_lessonController->loadStructureFromIndex(index);
                } else if (chosen == removeAct) {
                    m_lessonController->removeStructure(index.row());
                }
                return;
            }

            QString filePath = filePathFromContentIndex(index);
            if (filePath.endsWith(".xyz", Qt::CaseInsensitive))
            {    
                QMenu contextMenu(this);
                
                // Dateiname zur Information anzeigen
                QAction *fileNameAction = contextMenu.addAction(QFileInfo(filePath).fileName());
                fileNameAction->setEnabled(false);
                contextMenu.addSeparator();
                
                QAction *avogadroAction = contextMenu.addAction(tr("Open with Avogadro"));
                QAction *iboviewAction = contextMenu.addAction(tr("Open with IboView"));

                connect(avogadroAction, &QAction::triggered,
                    [this, filePath]() { openWithVisualizer(filePath, "avogadro"); });
                connect(iboviewAction, &QAction::triggered,
                    [this, filePath]() { openWithVisualizer(filePath, "iboview"); });

                // Claude Generated 2026 - Overlay this file onto the current structure (RMSD/Align).
                contextMenu.addSeparator();
                QAction *rmsdAction = contextMenu.addAction(
                    QIcon::fromTheme("view-object-histogram-linear"),
                    tr("Overlay onto current (RMSD/Align)…"));
                connect(rmsdAction, &QAction::triggered,
                    [this, filePath]() { showRMSDTool(filePath); });

                // Claude Generated 2026 - merge this file into the current scene (editing).
                QAction *mergeAction = contextMenu.addAction(tr("Add to current scene"));
                connect(mergeAction, &QAction::triggered, this,
                    [this, filePath]() { mergeFileIntoScene(filePath); });

                // Claude Generated 2026 - Add this file straight into the lesson.
                QAction *lessonAction = contextMenu.addAction(tr("Add to Lesson"));
                connect(lessonAction, &QAction::triggered, this,
                    [this, filePath]() { m_lessonController->addFile(filePath); });

                contextMenu.exec(m_directoryContentView->viewport()->mapToGlobal(pos));
            } else if (filePath.endsWith(".vtf", Qt::CaseInsensitive))
            {
                QMenu contextMenu(this);
                
                // Dateiname zur Information anzeigen
                QAction *fileNameAction = contextMenu.addAction(QFileInfo(filePath).fileName());
                fileNameAction->setEnabled(false);
                contextMenu.addSeparator();
                
                QAction *visualizerAction = contextMenu.addAction(tr("Open with 3D Viewer"));

                connect(visualizerAction, &QAction::triggered,
                    [this, filePath]() {
                        // Claude Generated 2026 - Route through loadMoleculeFile
                        // so snapshots, save-path, and simulation dock are synced.
                        loadMoleculeFile(filePath);
                    });

                // Claude Generated 2026 - Overlay this file onto the current structure (RMSD/Align).
                contextMenu.addSeparator();
                QAction *rmsdAction = contextMenu.addAction(
                    QIcon::fromTheme("view-object-histogram-linear"),
                    tr("Overlay onto current (RMSD/Align)…"));
                connect(rmsdAction, &QAction::triggered,
                    [this, filePath]() { showRMSDTool(filePath); });

                // Claude Generated 2026 - merge this file into the current scene (editing).
                QAction *mergeAction = contextMenu.addAction(tr("Add to current scene"));
                connect(mergeAction, &QAction::triggered, this,
                    [this, filePath]() { mergeFileIntoScene(filePath); });

                // Claude Generated 2026 - Add this file straight into the lesson.
                QAction *lessonAction = contextMenu.addAction(tr("Add to Lesson"));
                connect(lessonAction, &QAction::triggered, this,
                    [this, filePath]() { m_lessonController->addFile(filePath); });

                contextMenu.exec(m_directoryContentView->viewport()->mapToGlobal(pos));
            } else if (filePath.endsWith(".pdb", Qt::CaseInsensitive))
            {
                // Claude Generated - Phase 5C: PDB file support
                QMenu contextMenu(this);
                QAction *fileNameAction = contextMenu.addAction(QFileInfo(filePath).fileName());
                fileNameAction->setEnabled(false);
                contextMenu.addSeparator();

                QAction *visualizerAction = contextMenu.addAction(tr("Open with 3D Viewer"));

                connect(visualizerAction, &QAction::triggered,
                    [this, filePath]() { loadMoleculeFile(filePath); });

                // Claude Generated 2026 - Overlay this file onto the current structure (RMSD/Align).
                contextMenu.addSeparator();
                QAction *rmsdAction = contextMenu.addAction(
                    QIcon::fromTheme("view-object-histogram-linear"),
                    tr("Overlay onto current (RMSD/Align)…"));
                connect(rmsdAction, &QAction::triggered,
                    [this, filePath]() { showRMSDTool(filePath); });

                // Claude Generated 2026 - merge this file into the current scene (editing).
                QAction *mergeAction = contextMenu.addAction(tr("Add to current scene"));
                connect(mergeAction, &QAction::triggered, this,
                    [this, filePath]() { mergeFileIntoScene(filePath); });

                // Claude Generated 2026 - Add this file straight into the lesson.
                QAction *lessonAction = contextMenu.addAction(tr("Add to Lesson"));
                connect(lessonAction, &QAction::triggered, this,
                    [this, filePath]() { m_lessonController->addFile(filePath); });

                contextMenu.exec(m_directoryContentView->viewport()->mapToGlobal(pos));
            } else if (filePath.endsWith(".mol2", Qt::CaseInsensitive))
            {
                // Claude Generated - Phase 5C: MOL2 file support
                QMenu contextMenu(this);
                QAction *fileNameAction = contextMenu.addAction(QFileInfo(filePath).fileName());
                fileNameAction->setEnabled(false);
                contextMenu.addSeparator();

                QAction *visualizerAction = contextMenu.addAction(tr("Open with 3D Viewer"));

                connect(visualizerAction, &QAction::triggered,
                    [this, filePath]() { loadMoleculeFile(filePath); });

                // Claude Generated 2026 - Overlay this file onto the current structure (RMSD/Align).
                contextMenu.addSeparator();
                QAction *rmsdAction = contextMenu.addAction(
                    QIcon::fromTheme("view-object-histogram-linear"),
                    tr("Overlay onto current (RMSD/Align)…"));
                connect(rmsdAction, &QAction::triggered,
                    [this, filePath]() { showRMSDTool(filePath); });

                // Claude Generated 2026 - merge this file into the current scene (editing).
                QAction *mergeAction = contextMenu.addAction(tr("Add to current scene"));
                connect(mergeAction, &QAction::triggered, this,
                    [this, filePath]() { mergeFileIntoScene(filePath); });

                // Claude Generated 2026 - Add this file straight into the lesson.
                QAction *lessonAction = contextMenu.addAction(tr("Add to Lesson"));
                connect(lessonAction, &QAction::triggered, this,
                    [this, filePath]() { m_lessonController->addFile(filePath); });

                contextMenu.exec(m_directoryContentView->viewport()->mapToGlobal(pos));
            }else if(filePath.endsWith(".gbw", Qt::CaseInsensitive) || filePath.endsWith(".loc", Qt::CaseInsensitive) || filePath.endsWith(".ges", Qt::CaseInsensitive))
            {
                QMenu contextMenu(this);
                QAction *fileNameAction = contextMenu.addAction(tr("Open with IboView"));
                connect(fileNameAction, &QAction::triggered, [this, filePath]() { openWithVisualizer(filePath, "iboview"); });
                contextMenu.exec(m_directoryContentView->viewport()->mapToGlobal(pos));

            }else if(QFileInfo(filePath).fileName().contains(".molden", Qt::CaseInsensitive))
            {
                QMenu contextMenu(this);
                QAction *fileNameAction = contextMenu.addAction(tr("Open with IboView"));
                connect(fileNameAction, &QAction::triggered, [this, filePath]() { openWithVisualizer(filePath, "iboview"); });
                contextMenu.exec(m_directoryContentView->viewport()->mapToGlobal(pos));
            }else if(filePath.endsWith(".hess", Qt::CaseInsensitive))
            {
                QMenu contextMenu(this);

                QPair<int, int>frequencies = countImaginaryFrequencies(filePath);
                QAction *freq_action = contextMenu.addAction(tr("Imaginary Frequencies: %1\nRegular Frequencies: %2").arg(frequencies.first).arg(frequencies.second));
                freq_action->setEnabled(false);
                contextMenu.addSeparator();
                QAction *plotvib = contextMenu.addAction(tr("Generate Vibrational Modes"));

                connect(plotvib, &QAction::triggered, [this, filePath, frequencies]() 
                {
                    // Verwendung:
                    FrequencyInputDialog dialog(m_frequencies, this);
                    if (dialog.exec() == QDialog::Accepted) {
                        int selectedNumber = dialog.getSelectedNumber();
                        orcaPlotVib(filePath, selectedNumber + 5); // orca starts counting with 0 and the first 6 are not vibrational modes
                    }
                });

                contextMenu.exec(m_directoryContentView->viewport()->mapToGlobal(pos));

            } else if (filePath.endsWith(".out", Qt::CaseInsensitive)) {
                // Claude Generated 2026 - Matched on the file suffix; the old contains("out")
                // test also hit any file inside a folder whose path contains "out".
                QMenu contextMenu(this);
                QAction* nmrstruktur = contextMenu.addAction(tr("Add to NMR Spectrum"));
                connect(nmrstruktur, &QAction::triggered, [this, filePath]() {
                    if (QMessageBox::question(this, tr("NMR Spectra"), tr("Add the file of this name from every subdirectory of the working directory? No adds only this file."), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
                        QString dirname = QFileInfo(filePath).dir().path().split(QDir::separator()).last();
                        for (const QString& subdir : this->currentSubdirectories()) {
                            QString current = filePath;
                            current.replace(dirname, subdir);
                            m_nmrDialog->addStructure(current, subdir);
                        }
                    } else {
                        QString dirname = QFileInfo(filePath).dir().path().split(QDir::separator()).last();
                        m_nmrDialog->addStructure(filePath, dirname);
                    }
                });

                contextMenu.exec(m_directoryContentView->viewport()->mapToGlobal(pos));
            }
        });
}

QStringList MainWindow::currentSubdirectories() const
{
    QStringList subdirs;
    QDirIterator it(m_workingDirectory, QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        it.next();
        subdirs << it.fileName();
    }
    return subdirs;
}

// Claude Generated 2026 - P2/P4: prominent Explore/Compute mode switch, placed in the
// menu-bar corner so it has a fixed position and never reflows when the calculation
// toolbar is shown/hidden (the former dedicated toolbar shared the top area and shifted).
void MainWindow::createModeBar()
{
    QWidget* modeWidget = new QWidget(this);
    QHBoxLayout* row = new QHBoxLayout(modeWidget);
    row->setContentsMargins(2, 1, 6, 1);
    row->setSpacing(0);

    auto* group = new QButtonGroup(this);
    group->setExclusive(true);

    auto makeBtn = [&](const QString& text, const QString& tip) {
        auto* b = new QToolButton(modeWidget);
        b->setText(text);
        b->setToolTip(tip);
        b->setCheckable(true);
        b->setMinimumWidth(96);
        group->addButton(b);
        row->addWidget(b);
        return b;
    };
    m_exploreButton = makeBtn(tr("🔬 Explore"),
        tr("View and edit molecules: Project and Structure panels"));
    m_computeButton = makeBtn(tr("⚙ Compute"),
        tr("Run calculations: calculation toolbar plus the Simulation and Output panels"));
    m_teachingButton = makeBtn(tr("🎓 Teaching"),
        tr("Explore with the lesson browser in the Project panel"));

    modeWidget->setStyleSheet(QStringLiteral(
        "QToolButton { padding: 3px 14px; border: 1px solid palette(mid); }"
        "QToolButton:checked { background: palette(highlight); color: palette(highlighted-text);"
        " font-weight: bold; }"));

    connect(m_exploreButton, &QToolButton::clicked, this, [this]() { setAppMode(DockConfig::AppMode::Explore); });
    connect(m_computeButton, &QToolButton::clicked, this, [this]() { setAppMode(DockConfig::AppMode::Compute); });
    connect(m_teachingButton, &QToolButton::clicked, this, [this]() { setAppMode(DockConfig::AppMode::Teaching); });

    if (menuBar())
        menuBar()->setCornerWidget(modeWidget, Qt::TopRightCorner);
}

// Claude Generated 2026 - Apply a top-level mode, the only layout switch (UX stage 4b).
// Sets the calculation toolbar, dock visibility and the Project panel's browser
// explicitly (deterministic); reflow=false keeps restored sizes on startup.
void MainWindow::setAppMode(DockConfig::AppMode mode, bool reflow)
{
    const DockConfig::AppMode previous = m_appMode;
    m_appMode = mode;

    const std::pair<QToolButton*, DockConfig::AppMode> buttons[] = {
        { m_exploreButton, DockConfig::AppMode::Explore },
        { m_computeButton, DockConfig::AppMode::Compute },
        { m_teachingButton, DockConfig::AppMode::Teaching }
    };
    for (const auto& [button, buttonMode] : buttons) {
        if (!button)
            continue;
        button->blockSignals(true);
        button->setChecked(buttonMode == mode);
        button->blockSignals(false);
    }
    if (m_appModeGroup)
        for (QAction* a : m_appModeGroup->actions())
            a->setChecked(a->data().toInt() == static_cast<int>(mode));
    QSettings().setValue(DockConfig::UiAppModeKey, static_cast<int>(mode));

    if (m_calculationToolbar)
        m_calculationToolbar->setVisible(mode == DockConfig::AppMode::Compute);

    // Dock visibility and reflow are owned by DockManager.
    if (m_dockManager)
        m_dockManager->setAppMode(mode, reflow);

    // Teaching = Explore with the lesson browser; leaving it returns to the files.
    if (m_lessonController) {
        if (mode == DockConfig::AppMode::Teaching)
            m_lessonController->setBrowserMode(true);
        else if (previous == DockConfig::AppMode::Teaching)
            m_lessonController->setBrowserMode(false);
    }

    QString name;
    switch (mode) {
    case DockConfig::AppMode::Explore:  name = tr("Explore"); break;
    case DockConfig::AppMode::Compute:  name = tr("Compute"); break;
    case DockConfig::AppMode::Teaching: name = tr("Teaching"); break;
    }
    statusBar()->showMessage(tr("Mode: %1").arg(name), 2000);
}

// Claude Generated 2026 - P3: recursively collect leaf menu actions as palette commands.
static void collectMenuCommands(QMenu* menu, const QString& path, QVector<CommandPalette::Command>& out)
{
    if (!menu)
        return;
    for (QAction* a : menu->actions()) {
        if (a->isSeparator())
            continue;
        QString text = a->text();
        text.remove('&');
        if (a->menu()) {
            const QString sub = path.isEmpty() ? text : (path + QStringLiteral(" ▸ ") + text);
            collectMenuCommands(a->menu(), sub, out);
        } else if (!text.isEmpty()) {
            CommandPalette::Command c;
            c.title = text;
            c.context = path;
            QStringList keys;
            for (const QKeySequence& k : a->shortcuts())
                keys << k.toString(QKeySequence::NativeText);
            c.shortcut = keys.join(QStringLiteral(", "));
            c.enabled = a->isEnabled();
            QPointer<QAction> ap(a);
            c.run = [ap]() { if (ap) ap->trigger(); };
            out.append(c);
        }
    }
}

// Claude Generated 2026 - Every leaf action of the menu bar, in menu order. The one
// source of the command palette and Help ▸ Keyboard Shortcuts.
static QVector<CommandPalette::Command> collectMenuBarCommands(QMenuBar* bar)
{
    QVector<CommandPalette::Command> cmds;
    if (!bar)
        return cmds;
    for (QAction* topAct : bar->actions()) {
        if (!topAct->menu())
            continue;
        QString top = topAct->text();
        top.remove('&');
        collectMenuCommands(topAct->menu(), top, cmds);
    }
    return cmds;
}

void MainWindow::showCommandPalette()
{
    if (!m_commandPalette)
        m_commandPalette = new CommandPalette(this);
    // Claude Generated 2026 - UX stage 5: every command has a menu QAction (Photo,
    // Measure, Select All, Deselect, the modes included), so the menu bar is the
    // palette's only source and no entry appears twice.
    m_commandPalette->setCommands(collectMenuBarCommands(menuBar()));
    m_commandPalette->popUp();
}

// Claude Generated 2026 - Help ▸ Keyboard Shortcuts: every menu-bar action that has a
// key, in menu order. Generated from the actions, so it cannot drift from them.
void MainWindow::showKeyboardShortcuts()
{
    QVector<CommandPalette::Command> rows;
    for (const CommandPalette::Command& c : collectMenuBarCommands(menuBar()))
        if (!c.shortcut.isEmpty())
            rows.append(c);

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Keyboard Shortcuts"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* table = new QTableWidget(rows.size(), 3, &dialog);
    table->setHorizontalHeaderLabels({ tr("Shortcut"), tr("Command"), tr("Menu") });
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    for (int r = 0; r < rows.size(); ++r) {
        table->setItem(r, 0, new QTableWidgetItem(rows[r].shortcut));
        table->setItem(r, 1, new QTableWidgetItem(rows[r].title));
        table->setItem(r, 2, new QTableWidgetItem(rows[r].context));
    }
    table->resizeColumnsToContents();
    table->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(table);

    auto* note = new QLabel(tr("In the viewport: Esc steps back one level (drops a carried "
                               "fragment, clears the selection, leaves the tool). The keys of "
                               "the Edit and Build tools are listed in their tooltips on the "
                               "viewer bar."), &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.resize(620, 560);
    dialog.exec();
}

// Claude Generated 2026 - UX stage 5: seven menus File · Edit · View · Structure ·
// Simulation · Tools · Help. Every entry is a QAction, so the Ctrl+K palette and
// Help ▸ Keyboard Shortcuts (both harvested from the menu bar) list it. The quick
// toggles, Style/Look/Hydrogens/Hide Molecules menus are shared with the viewer bar
// and the viewport context menu, so every entry point shows the same checked state.
void MainWindow::createMenus()
{
    QMenuBar *menuBar = new QMenuBar;
    setMenuBar(menuBar);

    // ------------------------------------------------------------------ File
    QMenu *fileMenu = menuBar->addMenu(tr("&File"));

    // Claude Generated 2026 - Empty scene for the molecule builder.
    QAction *newSceneAction = fileMenu->addAction(QIcon::fromTheme("document-new"), tr("&New Scene"));
    newSceneAction->setToolTip(tr("Clear the scene and start building from scratch "
                                  "(enters Build mode; the old structure stays in Snapshots)."));
    connect(newSceneAction, &QAction::triggered, this, &MainWindow::newScene);

    // Claude Generated 2026 - Local file open. The action uses the current Working
    // Directory as the dialog's start path; loading the file does not change it.
    QAction *openFileAction = fileMenu->addAction(QIcon::fromTheme("document-open"), tr("&Open File..."));
    openFileAction->setShortcut(QKeySequence::Open);  // Ctrl+O / Cmd+O
    connect(openFileAction, &QAction::triggered, this, [this]() {
        const QString startDir = m_workingDirectory.isEmpty()
                                 ? QDir::homePath()
                                 : m_workingDirectory;
        const QString path = QFileDialog::getOpenFileName(this,
            tr("Open Molecule File"),
            startDir,
            tr("Molecule Files (*.xyz *.vtf *.pdb *.mol2);;All Files (*)"));
        if (path.isEmpty()) return;
        loadMoleculeFile(path);
    });

    // Claude Generated - Quick Win: Recent files menu
    m_recentFilesMenu = fileMenu->addMenu(QIcon::fromTheme("document-open-recent"), tr("&Recent Files"));
    m_recentFilesMenu->setEnabled(false);

#ifdef USE_SFTP
    QAction *openRemoteAction = fileMenu->addAction(QIcon::fromTheme("folder-remote"), tr("Open &Remote File..."));
    openRemoteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
    connect(openRemoteAction, &QAction::triggered, this, [this]() {
        SftpDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            QString localPath = dialog.getLocalPath();
            if (!localPath.isEmpty()) {
                loadMoleculeFile(localPath);
                statusBar()->showMessage(tr("Loaded remote file: %1").arg(QFileInfo(localPath).fileName()), 3000);
                updateRecentConnectionsMenu();
            }
        }
    });
    m_recentConnectionsMenu = fileMenu->addMenu(QIcon::fromTheme("network-server"), tr("Recent Remote &Connections"));
    m_recentConnectionsMenu->setEnabled(false);
    updateRecentConnectionsMenu();
#endif

    fileMenu->addSeparator();

    // Claude Generated 2026 - Save / Save As. The Save action overwrites the
    // source XYZ when the source is a .xyz file; otherwise it falls through
    // to a Save-As dialog. Save As always opens the dialog.
    m_saveAction = fileMenu->addAction(QIcon::fromTheme("document-save"), tr("&Save"));
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setToolTip(tr("Save the current structure. Overwrites the source "
                               "XYZ, or opens a Save As dialog for other formats."));
    m_saveAction->setEnabled(false);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveCurrentStructure);

    m_saveAsAction = fileMenu->addAction(QIcon::fromTheme("document-save-as"), tr("Save &As..."));
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    m_saveAsAction->setToolTip(tr("Save the current structure to a new XYZ file"));
    m_saveAsAction->setEnabled(false);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveCurrentStructureAs);

    fileMenu->addSeparator();

    // Claude Generated 2026 - High-quality image export: offscreen render of the 3D
    // viewer at an arbitrary resolution (true supersampling), with white/transparent
    // background options.
    QAction* exportImageAction = fileMenu->addAction(
        QIcon::fromTheme("camera-photo"), tr("&Export Image..."));
    exportImageAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    connect(exportImageAction, &QAction::triggered, this, [this]() {
        if (m_moleculeView)
            m_moleculeView->exportImageDialog(m_workingDirectory, &m_settings);
    });
    m_quickPhotoAction = fileMenu->addAction(QIcon::fromTheme("camera-photo"), tr("Quick P&hoto"));
    m_quickPhotoAction->setToolTip(tr("Save a PNG of the current view into the working "
                                      "directory, without a dialog."));
    connect(m_quickPhotoAction, &QAction::triggered, this, &MainWindow::quickExportPhoto);

    fileMenu->addSeparator();

    // Claude Generated 2026 - Lesson (OER teaching scenario) menu. A lesson is a
    // self-contained *.qlesson.json: several structures, each with its full
    // simulation conditions, plus author/ORCID/institution metadata.
    QMenu* lessonMenu = fileMenu->addMenu(QIcon::fromTheme("x-office-presentation"), tr("&Lesson"));
    QAction* openLessonAction = lessonMenu->addAction(tr("&Open Lesson..."));
    connect(openLessonAction, &QAction::triggered, this, [this]() {
        const QString startDir = m_workingDirectory.isEmpty() ? QDir::homePath() : m_workingDirectory;
        const QString path = QFileDialog::getOpenFileName(this, tr("Open Lesson"),
            startDir, tr("Qurcuma Lesson (*.qlesson.json *.json);;All Files (*)"));
        if (!path.isEmpty()) m_lessonController->openLesson(path);
    });
    QAction* addStructAction = lessonMenu->addAction(tr("&Add Current Structure to Lesson..."));
    connect(addStructAction, &QAction::triggered, this,
        [this]() { m_lessonController->addCurrentStructure(m_currentMoleculeFilePath); });
    QAction* metaAction = lessonMenu->addAction(tr("Lesson &Metadata..."));
    connect(metaAction, &QAction::triggered, this, [this]() { m_lessonController->editMetadata(); });
    lessonMenu->addSeparator();
    // Save: overwrite the currently open lesson file directly; Save As: always
    // prompt. Both route through saveLessonInteractive() (Claude Generated 2026).
    QAction* saveLessonAction = lessonMenu->addAction(tr("&Save Lesson"));
    connect(saveLessonAction, &QAction::triggered, this,
        [this]() { m_lessonController->saveLessonInteractive(/*forceDialog=*/false); });
    QAction* saveLessonAsAction = lessonMenu->addAction(tr("Save Lesson &As..."));
    connect(saveLessonAsAction, &QAction::triggered, this,
        [this]() { m_lessonController->saveLessonInteractive(/*forceDialog=*/true); });

    // Claude Generated Phase 4.5 - Workspace menu (saved list appended by updateWorkspaceList)
    m_workspaceMenu = fileMenu->addMenu(QIcon::fromTheme("window-duplicate"), tr("&Workspaces"));

    // Claude Generated 2026 - No shortcut here: Ctrl+Shift+S is Save As (QKeySequence::SaveAs
    // on Linux desktops), and a key bound twice fires neither action.
    QAction *saveWorkspaceAction = m_workspaceMenu->addAction(QIcon::fromTheme("document-save"), tr("&Save Current Workspace..."));
    connect(saveWorkspaceAction, &QAction::triggered, this, &MainWindow::saveCurrentWorkspace);

    // Claude Generated 2026 - Pick a saved workspace by name; restores it the same way a
    // click in the Project dock's workspace list does.
    QAction *loadWorkspaceAction = m_workspaceMenu->addAction(QIcon::fromTheme("document-open"), tr("&Load Workspace..."));
    loadWorkspaceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(loadWorkspaceAction, &QAction::triggered, this, [this]() {
        if (!m_workspaceManager)
            return;
        const auto workspaces = m_workspaceManager->listWorkspaces();
        if (workspaces.isEmpty()) {
            statusBar()->showMessage(tr("No saved workspaces"), 2000);
            return;
        }
        QStringList names;
        for (const auto& ws : workspaces)
            names << ws.name;
        bool ok = false;
        const QString picked = QInputDialog::getItem(this, tr("Load Workspace"), tr("Workspace:"),
                                                     names, 0, false, &ok);
        const int index = ok ? names.indexOf(picked) : -1;
        if (index >= 0)
            restoreWorkspaceState(workspaces.at(index));
    });

    m_workspaceMenu->addSeparator();

    fileMenu->addSeparator();
    // Claude Generated - Visual Polish: Menu icons
    QAction *quitAction = fileMenu->addAction(QIcon::fromTheme("application-exit"), tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    // ------------------------------------------------------------------ Edit
    QMenu *editMenu = menuBar->addMenu(tr("&Edit"));

    // Claude Generated 2026 - Ctrl+Z restores (and consumes) the newest snapshot,
    // keeping the Snapshots tab in sync. Text widgets keep their own Ctrl+Z (they
    // accept the ShortcutOverride first). Snapshot 0 (original) is never consumed.
    QAction *undoAction = editMenu->addAction(QIcon::fromTheme("edit-undo"), tr("&Undo (Snapshot)"));
    undoAction->setShortcut(QKeySequence::Undo);
    undoAction->setToolTip(tr("Restore the newest snapshot (move/build/bond edits "
                              "create them automatically) and remove it from the list."));
    connect(undoAction, &QAction::triggered, this, &MainWindow::undoLastSnapshot);
    editMenu->addSeparator();

    // Claude Generated 2026 - context-aware Copy/Paste: in viewer Edit mode they act on
    // the selected atoms/molecule (in-app); otherwise on the structure text (clipboard).
    QAction *copyAction = editMenu->addAction(QIcon::fromTheme("edit-copy"), tr("&Copy"));
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setToolTip(tr("Edit mode: copy the selected atoms. Otherwise: copy the structure text."));
    connect(copyAction, &QAction::triggered, this, [this]() {
        if (m_moleculeView && m_moleculeView->editMode() && !m_moleculeView->getSelectedAtoms().isEmpty()) {
            m_moleculeView->copySelection();
            statusBar()->showMessage(tr("Copied %1 atom(s)").arg(m_moleculeView->getSelectedAtoms().size()), 2000);
        } else {
            copyStructureToClipboard();
        }
    });

    QAction *pasteAction = editMenu->addAction(QIcon::fromTheme("edit-paste"), tr("&Paste"));
    pasteAction->setShortcut(QKeySequence::Paste);
    pasteAction->setToolTip(tr("Edit mode: paste the copied atoms into the scene. Otherwise: paste structure text."));
    connect(pasteAction, &QAction::triggered, this, [this]() {
        if (m_moleculeView && m_moleculeView->editMode()) {
            m_moleculeView->pasteClipboard();
            statusBar()->showMessage(tr("Pasted into scene — drag to place, then Resolve clashes if needed"), 2500);
        } else {
            pasteStructureFromClipboard();
        }
    });

    QAction *deleteSelAction = editMenu->addAction(QIcon::fromTheme("edit-delete"), tr("&Delete Selection"));
    deleteSelAction->setShortcut(QKeySequence::Delete);
    deleteSelAction->setToolTip(tr("Delete the selected atoms (Edit mode, single-frame structures)."));
    connect(deleteSelAction, &QAction::triggered, this, [this]() {
        if (m_moleculeView && m_moleculeView->editMode())
            m_moleculeView->deleteSelection();
    });

    editMenu->addSeparator();

    QAction *selectAllAction = editMenu->addAction(QIcon::fromTheme("edit-select-all"), tr("Select &All Atoms"));
    selectAllAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_A));
    connect(selectAllAction, &QAction::triggered, this, &MainWindow::selectAllAtoms);

    m_deselectAction = editMenu->addAction(tr("D&eselect All"));
    m_deselectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
    m_deselectAction->setToolTip(tr("Clear the atom selection and measurement marks (also Esc)."));
    connect(m_deselectAction, &QAction::triggered, this, &MainWindow::clearAtomSelection);

    editMenu->addSeparator();

    // Claude Generated 2026 - Preferences (the former Settings menu, UX stage 5).
    QMenu* preferencesMenu = editMenu->addMenu(QIcon::fromTheme("preferences-system"), tr("P&references"));

    // "Use Invocation Directory": checkbox state is updated in the constructor after
    // settings are loaded.
    m_useInvocationDirAction = preferencesMenu->addAction(QIcon::fromTheme("go-home"), tr("Use &Invocation Directory"));
    m_useInvocationDirAction->setCheckable(true);
    m_useInvocationDirAction->setToolTip(
        tr("If enabled, treat the directory from which qurcuma was launched "
           "as the active Working Directory on each launch."));
    connect(m_useInvocationDirAction, &QAction::triggered,
            this, &MainWindow::toggleUseInvocationDirectory);

    // Claude Generated - Visual Polish: Dark mode toggle (checkbox state set later after loading settings)
    m_darkModeAction = preferencesMenu->addAction(QIcon::fromTheme("weather-clear-night"), tr("&Dark Mode"));
    m_darkModeAction->setCheckable(true);
    connect(m_darkModeAction, &QAction::triggered, this, &MainWindow::toggleDarkMode);

    preferencesMenu->addSeparator();
    QAction* centerOnLoadAction = preferencesMenu->addAction(tr("&Center Molecule on Load"));
    centerOnLoadAction->setCheckable(true);
    centerOnLoadAction->setChecked(m_centerOnLoad);
    centerOnLoadAction->setToolTip(tr("When opening a file, translate all frames so the "
                                      "mass-weighted centre of mass is at the origin."));
    connect(centerOnLoadAction, &QAction::toggled, this, [this](bool on) {
        m_centerOnLoad = on;
        if (m_lessonController)
            m_lessonController->setCenterOnLoad(on);
        Settings::VisualizationSettings vs = m_settings.getVisualizationSettings();
        vs.centerOnLoad = on;
        m_settings.setVisualizationSettings(vs);
    });
    QMenu* rotationMenu = preferencesMenu->addMenu(tr("Mouse &Rotation"));
    auto* rotationGroup = new QActionGroup(this);
    const QVector<QPair<int, QString>> rotationModes = {
        { static_cast<int>(MoleculeViewer::RotationMode::Model), tr("Rotate Molecule (camera fixed)") },
        { static_cast<int>(MoleculeViewer::RotationMode::CameraOrbit), tr("Rotate Camera (orbit)") },
    };
    for (const auto& r : rotationModes) {
        QAction* a = rotationMenu->addAction(r.second);
        a->setCheckable(true);
        a->setData(r.first);
        rotationGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, mode = r.first]() {
            if (m_moleculeView)
                m_moleculeView->setRotationMode(mode);
        });
    }
    // The mode is restored with the last session after the menus exist; read it on open.
    connect(rotationMenu, &QMenu::aboutToShow, this, [this, rotationGroup]() {
        const int current = m_moleculeView ? m_moleculeView->getRotationMode() : 0;
        for (QAction* a : rotationGroup->actions())
            a->setChecked(a->data().toInt() == current);
    });
    QAction *cursorLockAction = preferencesMenu->addAction(tr("&Lock Cursor While Dragging"));
    cursorLockAction->setCheckable(true);
    cursorLockAction->setChecked(m_moleculeView ? m_moleculeView->dragCursorLock() : true);
    cursorLockAction->setToolTip(tr("Pin the cursor at the press point during a move so the drag never runs off-screen (relative drag)."));
    connect(cursorLockAction, &QAction::toggled, this, [this](bool on) {
        if (m_moleculeView) m_moleculeView->setDragCursorLock(on);
    });

    preferencesMenu->addSeparator();
    // Claude Generated 2026 - Operator metadata (name/ORCID/institution/license),
    // reused as default authorship for image exports and lessons.
    QAction *operatorAction = preferencesMenu->addAction(QIcon::fromTheme("user-identity"), tr("Operator Metadata..."));
    operatorAction->setToolTip(tr("Set your name, ORCID, institution and default license. "
                                  "Used as authorship for exported images and lessons."));
    connect(operatorAction, &QAction::triggered, this, &MainWindow::configureOperatorMetadata);

    // ------------------------------------------------------------------ View
    QMenu *viewMenu = menuBar->addMenu(tr("&View"));

    // Mode: radio items, checked from setAppMode() (corner buttons, palette, startup).
    QMenu* modeMenu = viewMenu->addMenu(tr("&Mode"));
    m_appModeGroup = new QActionGroup(this);
    const struct { DockConfig::AppMode mode; QString label; } modes[] = {
        { DockConfig::AppMode::Explore, tr("&Explore") },
        { DockConfig::AppMode::Compute, tr("&Compute") },
        { DockConfig::AppMode::Teaching, tr("&Teaching") },
    };
    for (const auto& m : modes) {
        QAction* a = modeMenu->addAction(m.label);
        a->setCheckable(true);
        a->setData(static_cast<int>(m.mode));
        a->setChecked(m.mode == m_appMode);
        m_appModeGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, mode = m.mode]() { setAppMode(mode); });
    }

    viewMenu->addSeparator();

    // Quick toggles: NCI, hydrogen bonds, hydrogen display, hidden molecules, labels.
    m_nciToggleAction = viewMenu->addAction(QIcon::fromTheme("draw-connector"), tr("&NCI Overlay"));
    m_nciToggleAction->setCheckable(true);
    m_nciToggleAction->setShortcut(Qt::Key_N);
    m_nciToggleAction->setToolTip(tr("Show non-covalent interactions (hydrogen/halogen bonds, "
                                     "pi stacking, contacts) as dashed lines in the 3D view."));
    connect(m_nciToggleAction, &QAction::triggered, this, &MainWindow::toggleNciOverlay);

    m_nciSourceMenu = viewMenu->addMenu(tr("NCI So&urce"));
    m_nciSourceGroup = new QActionGroup(this);
    const QVector<QPair<int, QString>> nciSources = {
        { 0, tr("Off") },
        { 1, tr("Geometry (distance/angle)") },
        { 2, tr("GFN-FF parameters") },
        { 3, tr("Population analysis (GFN2)") },
    };
    for (const auto& src : nciSources) {
        QAction* a = m_nciSourceMenu->addAction(src.second);
        a->setCheckable(true);
        a->setData(src.first);
        a->setChecked(src.first == 0);
        m_nciSourceGroup->addAction(a);
        connect(a, &QAction::triggered, this,
                [this, source = src.first]() { setNciSourceFromUi(source); });
    }

    // Claude Generated 2026 - Hydrogen-bond quick toggle, the most used contact kind.
    // Switching it on also shows the overlay, so the key always has a visible effect.
    m_hbondToggleAction = viewMenu->addAction(tr("Hydrogen &Bonds"));
    m_hbondToggleAction->setCheckable(true);
    m_hbondToggleAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_N));
    m_hbondToggleAction->setToolTip(tr("Show or hide hydrogen bonds in the NCI overlay "
                                       "(switches the overlay on if it is off)."));
    m_hbondToggleAction->setChecked(m_moleculeView && m_moleculeView->getNciOptions().hydrogenBonds);
    connect(m_hbondToggleAction, &QAction::triggered, this, [this](bool on) {
        if (!m_moleculeView)
            return;
        nci::Options o = m_moleculeView->getNciOptions();
        o.hydrogenBonds = on;
        m_moleculeView->setNciOptions(o);
        if (on && m_moleculeView->getNciSource() == 0)
            toggleNciOverlay();
    });
    if (m_moleculeView)
        connect(m_moleculeView, &MoleculeViewer::nciOptionsChanged, this,
            [this](const nci::Options& o) { m_hbondToggleAction->setChecked(o.hydrogenBonds); });

    QAction* nciOptionsAction = viewMenu->addAction(tr("NCI Op&tions…"));
    nciOptionsAction->setToolTip(tr("Open the Interactions dock with its options "
                                    "(kind filters, thresholds, colours)."));
    connect(nciOptionsAction, &QAction::triggered, this, [this]() {
        if (!m_nciDock)
            return;
        m_nciDock->show();
        m_nciDock->raise();
        m_nciDock->expandOptions();
    });

    // Claude Generated 2026 - Hydrogen display quick toggle (visual only; skeletal-formula
    // convention for "Polar"). H cycles the modes; in Build mode H stays the element key,
    // because the builder's key filter accepts the ShortcutOverride first (eventFilter).
    QMenu* hydrogenMenu = viewMenu->addMenu(tr("H&ydrogens"));
    m_hydrogenMenu = hydrogenMenu;  // also the bar's H button
    QAction* cycleHydrogensAction = hydrogenMenu->addAction(tr("Cycle Hydrogen Display"));
    cycleHydrogensAction->setShortcut(Qt::Key_H);
    cycleHydrogensAction->setToolTip(tr("All hydrogens, then polar hydrogens only (C-H hidden), "
                                        "then none. Display only; the structure keeps its H."));
    connect(cycleHydrogensAction, &QAction::triggered, this, [this]() {
        if (!m_moleculeView)
            return;
        m_moleculeView->cycleHydrogenDisplay();
        switch (m_moleculeView->getHydrogenDisplay()) {
        case MoleculeViewer::HydrogenDisplay::All:
            statusBar()->showMessage(tr("Hydrogens: all shown"), 2000); break;
        case MoleculeViewer::HydrogenDisplay::Polar:
            statusBar()->showMessage(tr("Hydrogens: polar only (C-H hidden)"), 2000); break;
        case MoleculeViewer::HydrogenDisplay::None:
            statusBar()->showMessage(tr("Hydrogens: hidden"), 2000); break;
        }
    });
    hydrogenMenu->addSeparator();
    m_hydrogenDisplayGroup = new QActionGroup(this);
    const QVector<QPair<int, QString>> hydrogenModes = {
        { int(MoleculeViewer::HydrogenDisplay::All), tr("All Hydrogens") },
        { int(MoleculeViewer::HydrogenDisplay::Polar), tr("Polar Hydrogens Only (hide C-H)") },
        { int(MoleculeViewer::HydrogenDisplay::None), tr("No Hydrogens") },
    };
    const int currentHydrogens = m_moleculeView ? int(m_moleculeView->getHydrogenDisplay()) : 0;
    for (const auto& h : hydrogenModes) {
        QAction* a = hydrogenMenu->addAction(h.second);
        a->setCheckable(true);
        a->setData(h.first);
        a->setChecked(h.first == currentHydrogens);  // restored at startup (setupUI)
        m_hydrogenDisplayGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, mode = h.first]() {
            if (m_moleculeView)
                m_moleculeView->setHydrogenDisplay(static_cast<MoleculeViewer::HydrogenDisplay>(mode));
        });
    }

    // Claude Generated 2026 - Hide molecules by kind (solvent etc.), display only. The list
    // comes from the loaded structure each time the menu opens (also from the bar button
    // and the viewport context menu, which share this QMenu).
    m_moleculeKindsMenu = viewMenu->addMenu(tr("Hide M&olecules"));
    connect(m_moleculeKindsMenu, &QMenu::aboutToShow, this, &MainWindow::populateMoleculeKindsMenu);

    QMenu* labelMenu = viewMenu->addMenu(tr("Atom Lab&els"));
    m_labelModeGroup = new QActionGroup(this);
    const QVector<QPair<int, QString>> labelModes = {
        { int(MoleculeViewer::AtomLabel::None), tr("No Labels") },
        { int(MoleculeViewer::AtomLabel::Element), tr("Element") },
        { int(MoleculeViewer::AtomLabel::Type), tr("Type (bead)") },
        { int(MoleculeViewer::AtomLabel::Index), tr("Index") },
    };
    for (const auto& l : labelModes) {
        QAction* a = labelMenu->addAction(l.second);
        a->setCheckable(true);
        a->setData(l.first);
        a->setChecked(l.first == 0);
        m_labelModeGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, mode = l.first]() {
            if (m_moleculeView)
                m_moleculeView->setAtomLabelMode(static_cast<MoleculeViewer::AtomLabel>(mode));
        });
    }

    viewMenu->addSeparator();

    // Style: the four drawing modes (keys 1-4), then atom and bond sizes. Also the
    // viewer bar's Style button.
    QMenu* renderStyleMenu = viewMenu->addMenu(tr("&Style"));
    m_renderStyleMenu = renderStyleMenu;
    m_renderStyleGroup = new QActionGroup(this);
    const struct { int mode; QString label; QKeySequence key; void (MainWindow::*slot)(); } styles[] = {
        { 0, tr("&Ball and Stick"), QKeySequence(Qt::Key_1), &MainWindow::setRenderingModeBallAndStick },
        { 1, tr("&Space Filling"), QKeySequence(Qt::Key_2), &MainWindow::setRenderingModeSpaceFilling },
        { 2, tr("&Wireframe"), QKeySequence(Qt::Key_3), &MainWindow::setRenderingModeWireframe },
        { 3, tr("S&ticks Only"), QKeySequence(Qt::Key_4), &MainWindow::setRenderingModeSticks },
    };
    for (const auto& s : styles) {
        QAction* a = renderStyleMenu->addAction(s.label);
        a->setCheckable(true);
        a->setShortcut(s.key);
        a->setData(s.mode);
        a->setChecked(s.mode == (m_moleculeView ? int(m_moleculeView->getRenderingMode()) : 0));
        m_renderStyleGroup->addAction(a);
        connect(a, &QAction::triggered, this, s.slot);
    }
    renderStyleMenu->addSeparator();
    QAction* atomsBiggerAction = renderStyleMenu->addAction(tr("Increase Atom Size"));
    atomsBiggerAction->setShortcuts({ QKeySequence(Qt::Key_Plus), QKeySequence(Qt::Key_Equal) });
    connect(atomsBiggerAction, &QAction::triggered, this, &MainWindow::increaseAtomSize);
    QAction* atomsSmallerAction = renderStyleMenu->addAction(tr("Decrease Atom Size"));
    atomsSmallerAction->setShortcut(QKeySequence(Qt::Key_Minus));
    connect(atomsSmallerAction, &QAction::triggered, this, &MainWindow::decreaseAtomSize);
    QAction* bondsThickerAction = renderStyleMenu->addAction(tr("Thicker Bonds"));
    bondsThickerAction->setShortcuts({ QKeySequence(Qt::Key_Period), QKeySequence(Qt::SHIFT | Qt::Key_Greater) });
    connect(bondsThickerAction, &QAction::triggered, this, &MainWindow::increaseBondThickness);
    QAction* bondsThinnerAction = renderStyleMenu->addAction(tr("Thinner Bonds"));
    bondsThinnerAction->setShortcuts({ QKeySequence(Qt::Key_Comma), QKeySequence(Qt::SHIFT | Qt::Key_Less) });
    connect(bondsThinnerAction, &QAction::triggered, this, &MainWindow::decreaseBondThickness);

    // Colour scheme: shown inside the Look menu (populateLookMenu), owned by the window.
    m_colorSchemeMenu = new QMenu(tr("&Colour Scheme"), this);
    m_colorSchemeGroup = new QActionGroup(this);
    const QVector<QPair<int, QString>> schemes = {
        { int(MoleculeViewer::ColorScheme::CPK), tr("CPK (Element Colors)") },
        { int(MoleculeViewer::ColorScheme::Monochrome), tr("Monochrome") },
        { int(MoleculeViewer::ColorScheme::ByCharge), tr("By Charge") },
        { int(MoleculeViewer::ColorScheme::ByType), tr("By Type (CG beads)") },
        { int(MoleculeViewer::ColorScheme::Custom), tr("Custom") },
    };
    for (const auto& s : schemes) {
        QAction* a = m_colorSchemeMenu->addAction(s.second);
        a->setCheckable(true);
        a->setData(s.first);
        a->setChecked(s.first == (m_moleculeView ? int(m_moleculeView->getColorScheme()) : 0));
        m_colorSchemeGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, scheme = s.first]() {
            if (m_moleculeView)
                m_moleculeView->setColorScheme(static_cast<MoleculeViewer::ColorScheme>(scheme));
            syncVisualizationDialog();
        });
    }

    // Claude Generated 2026 - Looks (UX stage 3): one menu for the viewer bar's Look
    // button, the View menu, the viewport context menu and the palette. Rebuilt on
    // every opening so user looks and the check mark on the active look are current.
    m_lookMenu = viewMenu->addMenu(tr("&Look"));
    connect(m_lookMenu, &QMenu::aboutToShow, this, &MainWindow::populateLookMenu);
    populateLookMenu();  // once now, so the palette finds the looks before the menu opened

    // Checked states mirror the viewer, whatever path changed it (panel, bar, key).
    auto checkByData = [](QActionGroup* group, int value) {
        for (QAction* a : group->actions())
            if (a->data().toInt() == value)
                a->setChecked(true);
    };
    if (m_moleculeView) {
        connect(m_moleculeView, &MoleculeViewer::renderingModeChanged, this,
            [this, checkByData](MoleculeViewer::RenderingMode mode) { checkByData(m_renderStyleGroup, int(mode)); });
        connect(m_moleculeView, &MoleculeViewer::colorSchemeChanged, this,
            [this, checkByData](MoleculeViewer::ColorScheme scheme) { checkByData(m_colorSchemeGroup, int(scheme)); });
        connect(m_moleculeView, &MoleculeViewer::atomLabelModeChanged, this,
            [this, checkByData](int mode) { checkByData(m_labelModeGroup, mode); });
        connect(m_moleculeView, &MoleculeViewer::hydrogenDisplayChanged, this,
            [this, checkByData](int mode) { checkByData(m_hydrogenDisplayGroup, mode); });
    }

    viewMenu->addSeparator();

    // Camera.
    m_fitViewAction = viewMenu->addAction(QIcon::fromTheme("zoom-fit-best"), tr("&Fit in View"));
    m_fitViewAction->setShortcuts({ QKeySequence(Qt::CTRL | Qt::Key_0), QKeySequence(Qt::Key_Home) });
    connect(m_fitViewAction, &QAction::triggered, this, &MainWindow::fitMoleculeInView);
    m_centerSelectionAction = viewMenu->addAction(tr("&Center on Selection"));
    m_centerSelectionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    connect(m_centerSelectionAction, &QAction::triggered, this, &MainWindow::centerViewOnSelection);
    m_viewsMenu = viewMenu->addMenu(tr("&Views"));
    connect(m_viewsMenu, &QMenu::aboutToShow, this, &MainWindow::populateViewsMenu);
    populateViewsMenu();  // once now, so the palette finds the quick views

    viewMenu->addSeparator();

    // Claude Generated (2026-04) - Dock rewrite: toggle actions for the dock architecture.
    QMenu *docksMenu = viewMenu->addMenu(QIcon::fromTheme("view-split-left-right"), tr("&Panels"));

    // Claude Generated 2026 - Use each dock's official toggleViewAction() instead of
    // wiring setVisible() directly. Qt's toggle action knows about tabified groups
    // and keeps the shared tab bar stable when the user hides/showes a dock.
    auto addDockToggle = [&docksMenu, this](QDockWidget* dock, const QString& label, const QKeySequence& shortcut = QKeySequence()) {
        if (!dock) return;
        QAction* act = dock->toggleViewAction();
        act->setText(label);
        if (!shortcut.isEmpty()) act->setShortcut(shortcut);
        docksMenu->addAction(act);
    };

    addDockToggle(m_projectDock,          tr("&Project"),            QKeySequence(Qt::CTRL | Qt::Key_B));
    addDockToggle(m_structureDock,        tr("S&tructure"));
    addDockToggle(m_appearanceDock,       tr("&Appearance"));
    addDockToggle(m_simulationDock,       tr("&Simulation"));
    addDockToggle(m_outputViewDock,       tr("&Output"));
    addDockToggle(m_nciDock,              tr("&Interactions"));
    addDockToggle(m_imageGalleryDock,     tr("I&mages"));

    // Reset layout: restore the captured baseline, then lay out the current mode on it.
    QAction *resetLayoutAction = viewMenu->addAction(QIcon::fromTheme("view-restore"), tr("&Reset to Default Layout"));
    resetLayoutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_0));
    connect(resetLayoutAction, &QAction::triggered, this, [this]() {
        if (m_dockManager) {
            m_dockManager->resetToBaseline();
            setAppMode(m_appMode);
            statusBar()->showMessage(tr("Layout reset to default"), 2000);
        }
    });

    // Claude Generated 2026 - Menu path back in for a floated dock, independent of
    // dragging it (a fallback, e.g. on a Wayland compositor without
    // xdg_toplevel_drag_v1; see src/docks/CLAUDE.md "Wayland"). This calls
    // QMainWindow::addDockWidget() directly, so it works on every platform.
    QAction *redockAction = viewMenu->addAction(QIcon::fromTheme("view-restore"), tr("Re-&dock Floating Panels"));
    connect(redockAction, &QAction::triggered, this, [this]() {
        if (m_dockManager) {
            m_dockManager->redockFloating();
            statusBar()->showMessage(tr("Floating panels re-docked"), 2000);
        }
    });

    viewMenu->addSeparator();

    // Claude Generated 2026 - P4: Command palette (searches every menu-bar action).
    QAction* paletteAction = viewMenu->addAction(QIcon::fromTheme("edit-find"), tr("Comm&and Palette…"));
    paletteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    connect(paletteAction, &QAction::triggered, this, &MainWindow::showCommandPalette);

    // Claude Generated 2026 - Hand the shared menus/actions to the viewer bar here, once
    // they exist. (setupNciAnalysis runs from setupUI, before createMenus, so the NCI
    // source menu it passed used to be null and the NCI dropdown stayed empty.)
    if (m_moleculeView) {
        m_moleculeView->setNciQuickMenu(m_nciSourceMenu);
        m_moleculeView->setQuickAccess(m_hbondToggleAction, m_hydrogenMenu, m_moleculeKindsMenu,
            m_renderStyleMenu, m_lookMenu);
    }

    // ------------------------------------------------------------- Structure
    QMenu *structureMenu = menuBar->addMenu(tr("St&ructure"));

    // Tool: the viewer bar's View · Measure · Edit · Build selector as radio items,
    // mirrored from interactionModeChanged (Bond editing is a Build sub-tool).
    auto* toolGroup = new QActionGroup(this);
    const struct { MoleculeViewer::InteractionMode mode; QString label; QKeySequence key; QString tip; } tools[] = {
        { MoleculeViewer::InteractionMode::None, tr("&View"), QKeySequence(),
          tr("Plain viewing: drag rotates, click selects an atom. Esc steps back to it.") },
        { MoleculeViewer::InteractionMode::Measure, tr("&Measure"), QKeySequence(Qt::Key_M),
          tr("Click atoms to measure: 2 = distance, 3 = angle, 4 = dihedral.") },
        { MoleculeViewer::InteractionMode::Edit, tr("&Edit"), QKeySequence(Qt::CTRL | Qt::Key_E),
          tr("Select and move atoms and molecules, copy/paste, with clash feedback.") },
        { MoleculeViewer::InteractionMode::Build, tr("&Build"), QKeySequence(Qt::Key_B),
          tr("Molecule builder: place atoms, draw bonds, add hydrogens, insert fragments.") },
    };
    for (const auto& t : tools) {
        QAction* a = structureMenu->addAction(t.label);
        a->setCheckable(true);
        a->setShortcut(t.key);
        a->setToolTip(t.tip);
        a->setData(int(t.mode));
        a->setChecked(t.mode == MoleculeViewer::InteractionMode::None);
        toolGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, mode = t.mode]() {
            if (!m_moleculeView)
                return;
            switch (mode) {
            case MoleculeViewer::InteractionMode::Measure: m_moleculeView->setMeasurementMode(1); break;
            case MoleculeViewer::InteractionMode::Edit:    m_moleculeView->setEditMode(true); break;
            case MoleculeViewer::InteractionMode::Build:   m_moleculeView->setBuildMode(true); break;
            default: m_moleculeView->setInteractionMode(MoleculeViewer::InteractionMode::None); break;
            }
        });
    }
    if (m_moleculeView)
        connect(m_moleculeView, &MoleculeViewer::interactionModeChanged, this,
            [toolGroup, checkByData](MoleculeViewer::InteractionMode m) {
                const auto shown = (m == MoleculeViewer::InteractionMode::BondEdit)
                    ? MoleculeViewer::InteractionMode::Build : m;
                checkByData(toolGroup, int(shown));
            });

    structureMenu->addSeparator();

    QAction *addMoleculeAction = structureMenu->addAction(QIcon::fromTheme("list-add"), tr("&Add Molecule to Scene…"));
    addMoleculeAction->setToolTip(tr("Merge a molecule from a file into the current scene (single-frame structures)."));
    connect(addMoleculeAction, &QAction::triggered, this, &MainWindow::addMoleculeToScene);

    QAction* addHydrogensAction = structureMenu->addAction(tr("Add &Hydrogens"));
    addHydrogensAction->setToolTip(tr("Saturate every open valence with hydrogens "
                                      "(single-frame structures)."));
    connect(addHydrogensAction, &QAction::triggered, this, [this]() {
        if (m_moleculeView)
            m_moleculeView->addHydrogens();
    });

    QAction *centerOriginAction = structureMenu->addAction(
        QIcon::fromTheme("snap-orthogonal"), tr("M&ove to Origin"));
    centerOriginAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Backspace));
    centerOriginAction->setToolTip(
        tr("Translate all frames so the mass-weighted centre of mass is at the origin, "
           "then reset the camera."));
    connect(centerOriginAction, &QAction::triggered, this,
        &MainWindow::centerMoleculeAtOrigin);

    structureMenu->addSeparator();

    QAction *rmsdAction = structureMenu->addAction(QIcon::fromTheme("view-object-histogram-linear"),
        tr("&RMSD / Align Structures"));
    rmsdAction->setToolTip(
        tr("Open the RMSD / Align tab: overlay structures, align them and "
           "optionally reorder atoms (curcuma RMSDDriver)."));
    connect(rmsdAction, &QAction::triggered, this, [this]() { showRMSDTool(); });

    // ------------------------------------------------------------ Simulation
    QMenu *simulationMenu = menuBar->addMenu(tr("&Simulation"));

    // Claude Generated 2026 - UX stage 5: the entries start a run with the Simulation
    // dock's current parameters (the same path as its Start button and the CLI -md/-opt).
    auto startSimulation = [this](SimulationConfig::Mode mode) {
        if (!m_simulationControlWidget)
            return;
        if (m_simulationControlWidget->currentAtoms().isEmpty()) {
            statusBar()->showMessage(tr("No molecule loaded."), 3000);
            return;
        }
        m_simulationControlWidget->setMode(mode);
        m_simulationControlWidget->onStartClicked();
    };
    QAction *mdAction = simulationMenu->addAction(
        QIcon::fromTheme("media-playback-start"), tr("Start &MD"));
    mdAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    mdAction->setToolTip(tr("Start an interactive MD run with the parameters of the Simulation dock."));
    connect(mdAction, &QAction::triggered, this,
        [startSimulation]() { startSimulation(SimulationConfig::Mode::MolecularDynamics); });

    QAction *optAction = simulationMenu->addAction(
        QIcon::fromTheme("system-run"), tr("Start &Optimization"));
    optAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G));
    optAction->setToolTip(tr("Start a geometry optimization with the parameters of the Simulation dock."));
    connect(optAction, &QAction::triggered, this,
        [startSimulation]() { startSimulation(SimulationConfig::Mode::GeometryOptimization); });

    QAction *stopAction = simulationMenu->addAction(QIcon::fromTheme("media-playback-stop"), tr("S&top"));
    stopAction->setEnabled(false);
    connect(stopAction, &QAction::triggered, this, [this]() {
        if (m_simulationControlWidget)
            m_simulationControlWidget->onStopClicked();
    });
    // While a run is active, Start would replace it; only Stop is offered.
    if (m_simulationControlWidget)
        connect(m_simulationControlWidget, &SimulationControlWidget::simulationRunningChanged, this,
            [mdAction, optAction, stopAction](bool running) {
                mdAction->setEnabled(!running);
                optAction->setEnabled(!running);
                stopAction->setEnabled(running);
            });

    simulationMenu->addSeparator();

    // Bring one tab of the Simulation dock to the front.
    auto showSimulationTab = [this](int tab) {
        if (!m_simulationDock)
            return;
        m_simulationDock->show();
        m_simulationDock->raise();
        if (m_simulationTabs)
            m_simulationTabs->setCurrentIndex(tab);
    };
    QAction *parametersAction = simulationMenu->addAction(QIcon::fromTheme("configure"), tr("&Parameters…"));
    parametersAction->setToolTip(tr("Show the Simulation dock with the method, MD and optimization parameters."));
    connect(parametersAction, &QAction::triggered, this, [showSimulationTab]() { showSimulationTab(0); });
    QAction *snapshotsAction = simulationMenu->addAction(tr("S&napshots…"));
    snapshotsAction->setToolTip(tr("Show the Snapshots tab: take, restore and delete structure snapshots."));
    connect(snapshotsAction, &QAction::triggered, this, [showSimulationTab]() { showSimulationTab(1); });

    // Claude Generated 2026 - open the live temperature/energy charts (modeless dialog).
    QAction *chartsAction = simulationMenu->addAction(
        QIcon::fromTheme("office-chart-line"), tr("&Charts…"));
    chartsAction->setToolTip(tr("Open the live temperature/energy charts for the running simulation."));
    connect(chartsAction, &QAction::triggered, this, [this]() {
        if (!m_simulationChartDialog)
            return;
        m_simulationChartDialog->show();
        m_simulationChartDialog->raise();
        m_simulationChartDialog->activateWindow();
    });

    // ----------------------------------------------------------------- Tools
    QMenu *toolsMenu = menuBar->addMenu(tr("&Tools"));

    QAction *runCalculationAction = toolsMenu->addAction(QIcon::fromTheme("system-run"), tr("&Run Calculation"));
    runCalculationAction->setShortcuts({ QKeySequence(Qt::CTRL | Qt::Key_R), QKeySequence(Qt::Key_F5) });
    runCalculationAction->setToolTip(tr("Run the program and command set in the calculation "
                                        "toolbar (Compute mode)."));
    connect(runCalculationAction, &QAction::triggered, this, &MainWindow::runSimulation);

    QAction *newCalcDirAction = toolsMenu->addAction(QIcon::fromTheme("folder-new"), tr("New Calculation &Directory…"));
    newCalcDirAction->setShortcut(QKeySequence::New);
    newCalcDirAction->setToolTip(tr("Create a new calculation directory in the working directory."));
    connect(newCalcDirAction, &QAction::triggered, this, &MainWindow::createNewDirectory);

    QAction *clearOutputAction = toolsMenu->addAction(QIcon::fromTheme("edit-clear"), tr("C&lear Output"));
    clearOutputAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(clearOutputAction, &QAction::triggered, this, &MainWindow::clearOutputView);

    QAction *nmrAction = toolsMenu->addAction(tr("&NMR Spectra…"));
    nmrAction->setToolTip(tr("Open the NMR spectrum viewer."));
    connect(nmrAction, &QAction::triggered, this, [this]() {
        if (m_nmrDialog)
            m_nmrDialog->show();
    });

    toolsMenu->addSeparator();
    QAction *configAction = toolsMenu->addAction(QIcon::fromTheme("preferences-system"), tr("Configure &Programs..."));
    connect(configAction, &QAction::triggered, this, &MainWindow::configurePrograms);

    // ------------------------------------------------------------------ Help
    QMenu *helpMenu = menuBar->addMenu(tr("&Help"));
    QAction *shortcutsAction = helpMenu->addAction(QIcon::fromTheme("input-keyboard"), tr("&Keyboard Shortcuts"));
    connect(shortcutsAction, &QAction::triggered, this, &MainWindow::showKeyboardShortcuts);
    QAction *aboutAction = helpMenu->addAction(QIcon::fromTheme("help-about"), tr("&About Qurcuma"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);

    // Statusleiste
    setStatusBar(new QStatusBar);
    // Claude Generated 2026 - Permanent indicators: file · atoms · frame. Transient
    // showMessage() notices appear to their left and disappear; these stay.
    m_statusFileLabel = new QLabel(this);
    m_statusAtomsLabel = new QLabel(this);
    m_statusFrameLabel = new QLabel(this);
    m_statusFrameLabel->setVisible(false);
    statusBar()->addPermanentWidget(m_statusFileLabel);
    statusBar()->addPermanentWidget(m_statusAtomsLabel);
    statusBar()->addPermanentWidget(m_statusFrameLabel);
}

void MainWindow::setupConnections()
{
    // Claude Generated - Phase 2.3: Connect modification tracking for editors
    // Note: Tab index management will be handled in setupUI where tabs are created
    // For now, just connect to show modified state in status bar
    connect(m_structureView, &ModifiableTextEdit::modificationChanged, [this](bool modified) {
        if (modified) {
            statusBar()->showMessage(tr("Structure file modified"));
        }
    });

    connect(m_inputView, &ModifiableTextEdit::modificationChanged, [this](bool modified) {
        if (modified) {
            statusBar()->showMessage(tr("Input file modified"));
        }
    });

    // Kommandozeilen-Verbindung
    connect(m_commandInput, &QLineEdit::returnPressed,
        this, &MainWindow::runCommand);

    // Programmauswahl-Verbindung
    connect(m_programSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MainWindow::programSelected);

    // Projekt-Auswahl-Verbindung
    connect(m_projectListView, &QListView::clicked,
        this, &MainWindow::projectSelected);

    // Claude Generated 2026 - WP T3: route the calculation runner's output/completion
    // into the output dock + the finish handler (the runner owns the QProcess).
    connect(m_calculationRunner, &CalculationRunner::outputReceived,
        this, [this](const QString& text) { m_outputViewDock->appendOutput(text); });
    connect(m_calculationRunner, &CalculationRunner::errorReceived,
        this, [this](const QString& text) { m_outputViewDock->appendOutput("Error: " + text); });
    connect(m_calculationRunner, &CalculationRunner::finished,
        this, &MainWindow::onCalculationFinished);

    // Programmtyp-spezifische Aktionen
    connect(m_programSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [this](int index) {
            QString program = m_programSelector->itemText(index);
            if (m_simulationPrograms.contains(program)) {
                m_commandInput->setEnabled(true);
                m_commandInput->setPlaceholderText("Enter simulation command...");
            } else if (m_visualizerPrograms.contains(program)) {
                m_commandInput->setEnabled(false);
                m_commandInput->setPlaceholderText(tr("Visualization program - no command needed"));
            }
        });

    // Verbindung für den "Neue Rechnung" Button
    connect(m_newCalculationButton, &QPushButton::clicked,
        this, &MainWindow::createNewDirectory);

    // Verbindung für den "Neue Rechnung" Button
    connect(m_runCalculation, &QPushButton::clicked,
        this, &MainWindow::runSimulation);

    // Claude Generated - Fixed duplicate connection removed below, kept single handler
    connect(m_projectListView->selectionModel(),
        &QItemSelectionModel::currentChanged,
        [this](const QModelIndex& current, const QModelIndex&) {
            if (current.isValid()) {
                // Use projectSelected() slot for consistent handling
                projectSelected(current);
            }
        });

    connect(m_programSelector, &QComboBox::currentTextChanged,
        this, &MainWindow::updateCommandLineVisibility);

    // Verbinde Programmauswahl mit Completer-Aktualisierung
    connect(m_programSelector, &QComboBox::currentTextChanged,
        [this](const QString& program) {
            if (m_simulationPrograms.contains(program)) {
                m_commandCompleter->setModel(new QStringListModel(m_calculationRunner->commandsFor(program)));
            }
        });
    connect(m_directoryContentView, &QListView::clicked,
        [this](const QModelIndex& index) {
            // Claude Generated 2026 - In Lesson mode the view shows the in-memory
            // lesson model, not the filesystem; load that structure directly.
            if (m_lessonController->browseMode()) {
                m_lessonController->loadStructureFromIndex(index);
                return;
            }
            QString filePath = filePathFromContentIndex(index);
            QString suffix = QFileInfo(filePath).suffix().toLower();
            QString basename = QFileInfo(filePath).baseName();
            if (suffix == "xyz" || suffix == "vtf") {
                // Claude Generated 2026 - Route molecule files through the
                // central loadMoleculeFile() which handles snapshots, simulation
                // dock sync, save-path tracking, and modified-state flags.
                loadMoleculeFile(filePath);
            }
            else if (suffix == "log" || suffix == "out" || suffix == "txt") {
                // Log/Output-Dateien in Output View laden
                QFile file(filePath);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    m_outputViewDock->setText(QString::fromUtf8(file.readAll()));
                    file.close();
                }
            }
            else if (suffix == "inp" || basename == "input") {
                // Input-Dateien in Input View laden
                QFile file(filePath);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    m_inputView->setPlainText(QString::fromUtf8(file.readAll()));
                    m_inputFileEdit->setText(QFileInfo(filePath).fileName());
                    file.close();
                }
            }
        });

    // Claude Generated Phase 3.2 - Tree widget signals
    // Phase 6: BookmarkWidget is embedded in ProjectDock and forwards
    // bookmarkDirectorySelected through the ProjectDock. Keep workspace list signals.

    // Claude Generated Phase 4.3 - Workspace list signals
    if (m_workspaceListView) {
        connect(m_workspaceListView, &QListWidget::itemClicked,
            this, &MainWindow::onWorkspaceItemClicked);

        connect(m_workspaceListView, &QListWidget::customContextMenuRequested,
            this, &MainWindow::onWorkspaceContextMenu);
    }

#ifdef USE_SFTP
    if (m_remoteDirectoriesView) {
        connect(m_remoteDirectoriesView, &QTreeWidget::itemClicked,
                this, &MainWindow::onRemoteDirectoryClicked);
    }
#endif

    // Claude Generated - Removed duplicate click handler, using projectSelected() slot instead
    // Navigation is now handled in projectSelected() which calls updateDirectoryContent()

    connect(m_chooseDirectory, &QPushButton::clicked, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this,
            tr("Choose directory"),
            m_workingDirectory.isEmpty() ? QDir::homePath() : m_workingDirectory);
        if (!dir.isEmpty()) {
            switchWorkingDirectory(dir);
        }
    });

    // Claude Generated - Quick Win: Auto-save drafts timer
    m_autoSaveTimer = new QTimer(this);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::autoSaveDrafts);
    m_autoSaveTimer->start(30000);  // Auto-save every 30 seconds

    // Ctrl+V is handled by the Edit-menu Paste action (context-aware: selection in
    // viewer Edit mode, else structure text). A second QShortcut here caused an
    // "Ambiguous shortcut overload: Ctrl+V" so paste stopped working. Claude Generated 2026.

    // Claude Generated - Phase 2C: AtomListPanel Connections
    if (m_atomListPanel && m_moleculeView) {
        // When MoleculeViewer selection changes → Update AtomListPanel
        connect(m_moleculeView, &MoleculeViewer::selectionChanged,
                [this](const QVector<int>& selectedAtoms) {
                    if (m_atomListPanel) {
                        m_atomListPanel->setSelectedAtoms(selectedAtoms);
                    }
                });

        // When AtomListPanel selection changes → Update MoleculeViewer
        connect(m_atomListPanel, &AtomListPanel::atomSelectionChanged,
                [this](const QVector<int>& selectedAtoms) {
                    if (m_moleculeView) {
                        m_moleculeView->getSelectionManager()->clearSelection();
                        for (int idx : selectedAtoms) {
                            m_moleculeView->getSelectionManager()->selectAtom(idx, true);
                        }
                        m_moleculeView->update();
                    }
                });

        // When user double-clicks atom in table → Focus on it
        connect(m_atomListPanel, &AtomListPanel::focusAtom,
                [this](int atomIndex) {
                    if (m_moleculeView) {
                        m_moleculeView->centerOnAtom(atomIndex);
                    }
                });

        // Claude Generated 2026 - Keep the permanent status-bar indicators current.
        connect(m_moleculeView, &MoleculeViewer::trajectoryLoaded,
                this, &MainWindow::updateStatusIndicators);
        connect(m_moleculeView, &MoleculeViewer::frameChanged,
                this, &MainWindow::updateStatusIndicators);
        connect(m_moleculeView, &MoleculeViewer::moleculeUpdated,
                this, &MainWindow::updateStatusIndicators);

        // When MoleculeViewer loads molecule → Update AtomListPanel
        connect(m_moleculeView, &MoleculeViewer::trajectoryLoaded,
                [this]() {
                    if (m_atomListPanel && m_moleculeView) {
                        m_atomListPanel->updateAtomList(
                            m_moleculeView->getAtomPositions(),
                            m_moleculeView->getAtomElements(),
                            m_moleculeView->getAtomCharges()
                        );
                    }
                });

        // When frame changes → Update AtomListPanel with new positions
        connect(m_moleculeView, &MoleculeViewer::frameChanged,
                [this]() {
                    if (m_atomListPanel && m_moleculeView) {
                        m_atomListPanel->updateAtomList(
                            m_moleculeView->getAtomPositions(),
                            m_moleculeView->getAtomElements(),
                            m_moleculeView->getAtomCharges()
                        );
                    }
                });

        // Claude Generated 2026 - Bidirectional structure sync. The viewer is the
        // canonical store; on any geometry edit it re-emits moleculeUpdated, which
        // refreshes the atom table + structure text. m_structSyncing prevents the
        // edit we just applied (table/text source) from bouncing back; we also skip
        // the live-MD path (updateSimulationFrame emits moleculeUpdated every step).
        connect(m_moleculeView, &MoleculeViewer::moleculeUpdated, this,
                [this](const QVector<MoleculeViewer::Atom>&, const QVector<MoleculeViewer::Bond>&) {
                    if (m_structSyncing || m_moleculeView->simulationActive())
                        return;
                    updateAtomTableFromViewer();
                    updateStructureTextFromViewer();
                });

        // Table edit → viewer (then refresh the text mirror; the table keeps its edit).
        connect(m_atomListPanel, &AtomListPanel::atomEdited, this,
                [this](int row, const QString& element, const QVector3D& position) {
                    if (m_structSyncing || !m_moleculeView)
                        return;
                    m_structSyncing = true;
                    takeSnapshot(tr("Before table edit"));  // Claude Generated 2026
                    m_moleculeView->setAtomInCurrentFrame(row, element, position);
                    if (m_simulationControlWidget)
                        m_simulationControlWidget->setMolecule(
                            m_moleculeView->getCurrentFrameAtoms(),
                            m_moleculeView->getCurrentFrameBonds());
                    m_structureModified = true;
                    updateStructureTextFromViewer();
                    m_structSyncing = false;
                });
    }

    // After a simulation run ends, refresh the table + text once (they were skipped
    // live to avoid per-step churn). Claude Generated 2026.
    if (m_simulationControlWidget) {
        connect(m_simulationControlWidget, &SimulationControlWidget::simulationRunningChanged,
                this, [this](bool running) {
                    if (!running) {
                        updateAtomTableFromViewer();
                        updateStructureTextFromViewer();
                    }
                });
    }
}

void MainWindow::setupShortcuts()
{
    // Claude Generated - Phase 1.2: Keyboard shortcuts. Ctrl+N, Ctrl+R/F5 and Ctrl+L sit
    // on the Tools menu actions (createMenus).
    // Claude Generated 2026 - Ctrl+S is bound to the File>Save menu action
    // (m_saveAction) below, so we deliberately omit a second QShortcut here to
    // avoid double-firing. The editor save behaviour is still reachable via
    // saveCurrentStructureAs() / editor shortcuts.
    // Claude Generated 2026 - Escape is handled once in setupUI (handleEscape:
    // cancel a running calculation, else clear the selection); Ctrl+0/Home live
    // on View ▸ Fit in View. The doubled registrations were ambiguous.
    new QShortcut(QKeySequence::NextChild, this, this, &MainWindow::switchEditorTab);  // Ctrl+Tab
}

void MainWindow::setupProjectViewContextMenu()
{
    m_projectListView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_projectListView, &QListView::customContextMenuRequested,
        [this](const QPoint& pos) {
            QModelIndex index = m_projectListView->indexAt(pos);
            if (!index.isValid())
                return;

            QString path = m_projectModel->filePath(index);
            QMenu contextMenu(this);

            QAction* bookmarkAction = contextMenu.addAction(tr("Add to Bookmarks"));
            connect(bookmarkAction, &QAction::triggered, [this, path]() {
                Settings::BookmarkItem bm;
                bm.id = QUuid::createUuid().toString();
                bm.name = QDir(path).dirName();
                bm.path = path;
                bm.isFolder = false;
                bm.parentId = "";
                bm.created = QDateTime::currentDateTime();
                m_settings.addBookmark(bm);
                refreshBookmarkTree();
                statusBar()->showMessage(tr("Directory bookmarked: %1")
                                             .arg(QDir(path).dirName()),
                    3000);
            });

            QAction* setWorkDirAction = contextMenu.addAction(tr("Set as Working Directory"));
            connect(setWorkDirAction, &QAction::triggered, [this, path]() {
                switchWorkingDirectory(path);
                m_settings.addWorkingDirectory(path);
            });

            contextMenu.exec(m_projectListView->viewport()->mapToGlobal(pos));
        });
}

// Anpassung der Kommandozeilen-Logik
void MainWindow::updateCommandLineVisibility(const QString &program)
{
    if (program == "orca") {
        m_commandInput->setVisible(false);
        m_commandInput->setEnabled(false);
        m_inputFileEdit->setText("input");
        m_inputFileEdit->setReadOnly(true);
    } else {
        m_commandInput->setVisible(true);
        m_commandInput->setEnabled(true);
        m_inputFileEdit->setReadOnly(false);

        if (program == "xtb") {
            // Bei xtb wird der Strukturdateiname direkt nach dem Programmnamen verwendet
            connect(m_structureFileEdit, &QLineEdit::textChanged, this, [this]() {
                QString command = m_commandInput->text();
                // Entferne alten Dateinamen falls vorhanden
                command = command.split(" ").first();
                command += " " + m_structureFileEdit->text();
                m_commandInput->setText(command);
            });
        } else if (program == "curcuma") {
            // Bei curcuma folgt der Dateiname nach dem Befehl
            connect(m_commandCompleter, QOverload<const QString&>::of(&QCompleter::activated),
                this, [this](const QString& text) {
                    QString command = text + " " + m_structureFileEdit->text();
                    m_commandInput->setText(command);
                });
        }

        const QStringList programCommands = m_calculationRunner->commandsFor(program);
        if (!programCommands.isEmpty()) {
            m_commandInput->setPlaceholderText(tr("Enter command for %1...").arg(program));
            m_commandCompleter->setModel(new QStringListModel(programCommands));
        }
    }
}

void MainWindow::setupProgramSpecificDirectory(const QString &dirPath, const QString &program)
{
    // Struktur speichern wenn vorhanden
    if (!m_structureView->toPlainText().isEmpty()) {
        QString structureFileName = m_structureFileEdit->text();
        QFile structureFile(dirPath + "/" + structureFileName);
        if (structureFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            structureFile.write(m_structureView->toPlainText().toUtf8());
            structureFile.close();
        }
    }

    if (program == "orca") {
        // ORCA-spezifische Initialisierung
        QFile inputFile(dirPath + "/input");
        if (inputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            inputFile.write(m_inputView->toPlainText().toUtf8());
            inputFile.close();
        }
    }
    else if (program == "xtb") {
        // XTB-spezifische Initialisierung
        // Input speichern falls vorhanden
        if (!m_inputView->toPlainText().isEmpty()) {
            QString inputFileName = m_inputFileEdit->text();
            QFile inputFile(dirPath + "/" + inputFileName);
            if (inputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                inputFile.write(m_inputView->toPlainText().toUtf8());
                inputFile.close();
            }
        }
    } else if (program == "curcuma") {
        // Input speichern falls vorhanden
        if (!m_inputView->toPlainText().isEmpty()) {
            QString inputFileName = m_inputFileEdit->text();
            QFile inputFile(dirPath + "/" + inputFileName);
            if (inputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                inputFile.write(m_inputView->toPlainText().toUtf8());
                inputFile.close();
            }
        }
    }
}
// In mainwindow.cpp, die configurePrograms-Funktion anpassen:
void MainWindow::configurePrograms()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Configure Programs"));
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    // Spezielle Behandlung für ORCA
    {
        QHBoxLayout *hbox = new QHBoxLayout();
        QLineEdit* pathEdit = new QLineEdit(m_settings.orcaBinaryPath());
        QPushButton *browseBtn = new QPushButton(tr("..."));
        
        hbox->addWidget(new QLabel(tr("ORCA Binary Directory")));
        hbox->addWidget(pathEdit);
        hbox->addWidget(browseBtn);
        layout->addLayout(hbox);

        connect(browseBtn, &QPushButton::clicked, [=]() {
            QString path = QFileDialog::getExistingDirectory(this,
                tr("Select ORCA Binary Directory"),
                QDir::homePath());
            if (!path.isEmpty()) {
                pathEdit->setText(path);
            }
        });

        // Speichern des ORCA-Pfads
        connect(&dialog, &QDialog::accepted, [=]() {
            m_settings.setOrcaBinaryPath(pathEdit->text());
        });
    }

    // Andere Programme (außer orca)
    for (const QString& program : m_simulationPrograms + m_visualizerPrograms) {
        if (program != "orca") {  // ORCA überspringen, da bereits behandelt
            QHBoxLayout *hbox = new QHBoxLayout();
            QLineEdit* pathEdit = new QLineEdit(m_settings.getProgramPath(program));
            QPushButton *browseBtn = new QPushButton(tr("..."));
            
            hbox->addWidget(new QLabel(program));
            hbox->addWidget(pathEdit);
            hbox->addWidget(browseBtn);
            layout->addLayout(hbox);

            connect(browseBtn, &QPushButton::clicked, [=]() {
                QString path = QFileDialog::getOpenFileName(this,
                    tr("Path for ") + program,
                    QDir::homePath());
                if (!path.isEmpty()) {
                    pathEdit->setText(path);
                    m_settings.setProgramPath(program, path);
                }
            });
        }
    }

    // OK und Abbrechen Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.exec();
}

// Claude Generated 2026 - Operator metadata (name/ORCID/institution/license),
// stored once and reused as default authorship for image exports and lessons.
void MainWindow::configureOperatorMetadata()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Operator Metadata"));
    auto* form = new QFormLayout(&dialog);

    auto* nameEdit = new QLineEdit(m_settings.operatorName(), &dialog);
    nameEdit->setPlaceholderText(tr("Your name"));
    form->addRow(tr("Name:"), nameEdit);

    auto* orcidEdit = new QLineEdit(m_settings.operatorOrcid(), &dialog);
    orcidEdit->setPlaceholderText(tr("0000-0000-0000-0000"));
    form->addRow(tr("ORCID:"), orcidEdit);

    auto* institutionEdit = new QLineEdit(m_settings.operatorInstitution(), &dialog);
    institutionEdit->setPlaceholderText(tr("Institution / affiliation"));
    form->addRow(tr("Institution:"), institutionEdit);

    auto* licenseCombo = new QComboBox(&dialog);
    licenseCombo->setEditable(true);
    licenseCombo->addItems({ QStringLiteral("CC-BY-4.0"), QStringLiteral("CC0-1.0"),
                             QStringLiteral("CC-BY-SA-4.0"), QStringLiteral("CC-BY-ND-4.0"),
                             QStringLiteral("CC-BY-NC-4.0") });
    const QString currentLicense = m_settings.operatorLicense();
    int licIdx = licenseCombo->findText(currentLicense);
    if (licIdx >= 0)
        licenseCombo->setCurrentIndex(licIdx);
    else if (!currentLicense.isEmpty())
        licenseCombo->setEditText(currentLicense);
    licenseCombo->setToolTip(tr("Default license embedded into exported images."));
    form->addRow(tr("License:"), licenseCombo);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    m_settings.setOperatorName(nameEdit->text().trimmed());
    m_settings.setOperatorOrcid(orcidEdit->text().trimmed());
    m_settings.setOperatorInstitution(institutionEdit->text().trimmed());
    m_settings.setOperatorLicense(licenseCombo->currentText().trimmed());
}

// In mainwindow.cpp:
bool MainWindow::setupCalculationDirectory()
{
    bool ok;
    QString calcName = QInputDialog::getText(this, tr("New Calculation"),
        tr("Calculation name:"), QLineEdit::Normal, "", &ok);
    
    if (!ok || calcName.isEmpty()) {
        return false;
    }

    // Entferne ungültige Zeichen aus dem Namen
    QRegularExpression invalidChars("[^a-zA-Z0-9_-]");
    calcName.replace(invalidChars, "_");

    // Erstelle das Berechnungsverzeichnis
    QDir workDir(m_workingDirectory);
    if (!workDir.exists()) {
        QMessageBox::warning(this, tr("Error"),
            tr("Please select a valid working directory first."));
        return false;
    }

    // Erstelle das Unterverzeichnis
    m_currentCalculationDir = workDir.filePath(calcName);
    m_currentProjectLabel->setText(m_currentCalculationDir);
    QDir calcDir(currentCalculationDir());

    if (calcDir.exists()) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, tr("Directory Exists"),
            tr("Directory already exists. Do you want to overwrite it?"),
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply == QMessageBox::No) {
            return false;
        }
        // Lösche existierendes Verzeichnis
        calcDir.removeRecursively();
    }

    if (!workDir.mkdir(calcName)) {
        QMessageBox::warning(this, tr("Error"),
            tr("Could not create the calculation directory."));
        return false;
    }

    // Speichere Input-Daten
    if (!m_structureView->toPlainText().isEmpty()) {
        QFile structFile(calcDir.filePath("input.xyz"));
        if (structFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            structFile.write(m_structureView->toPlainText().toUtf8());
            structFile.close();
        }
    }

    if (!m_inputView->toPlainText().isEmpty()) {
        QFile inputFile(calcDir.filePath("input"));
        if (inputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            inputFile.write(m_inputView->toPlainText().toUtf8());
            inputFile.close();
        }
    }

    statusBar()->showMessage(tr("Calculation directory created: ") + calcName);
    return true;
}
void MainWindow::createNewDirectory()
{
    bool ok;
    QString suggestedName;
    
    // Wenn ein Verzeichnis ausgewählt ist, nutze dessen Namen als Basis
    if (!m_currentCalculationDir.isEmpty()) {
        QDir dir(currentCalculationDir());
        QString baseName = dir.dirName();
        // Füge _1, _2 etc. hinzu, falls das Verzeichnis bereits existiert
        int counter = 1;
        suggestedName = baseName + "_" + QString::number(counter);
        while (QDir(dir.absolutePath() + "/" + suggestedName).exists()) {
            counter++;
            suggestedName = baseName + "_" + QString::number(counter);
        }
    }

    QString dirName = QInputDialog::getText(this, tr("New Directory"),
        tr("Directory name:"), QLineEdit::Normal, suggestedName, &ok);
    
    if (!ok || dirName.isEmpty()) {
        return;
    }

    QRegularExpression invalidChars("[^a-zA-Z0-9_-]");
    dirName.replace(invalidChars, "_");

    // Bestimme das Elternverzeichnis
    QString parentDir = m_workingDirectory;
    
    QDir workDir(parentDir);
    QString newDirPath = workDir.filePath(dirName);
    QDir newDir(newDirPath);

    if (newDir.exists()) {
        QMessageBox::warning(this, tr("Error"),
            tr("A directory with this name already exists."));
        return;
    }

    if (!workDir.mkdir(dirName)) {
        QMessageBox::warning(this, tr("Error"),
            tr("Could not create directory."));
        return;
    }
    m_structureView->clear();
    m_inputFileEdit->clear();
    m_inputView->clear();

    // Programm-spezifische Initialisierung
    QString program = m_programSelector->currentText();
    setupProgramSpecificDirectory(newDirPath, program);

    // Claude Generated - Set current directory before updating view
    m_currentCalculationDir = dirName;
    m_currentProjectLabel->setText(m_currentCalculationDir);

    // Now update the view (which uses m_currentCalculationDir)
    updateDirectoryContent();

    statusBar()->showMessage(tr("Directory created: ") + dirName);
}

void MainWindow::updateOutputView(const QString& logFile, bool scrollToBottom)
{
    QFile file(logFile);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_outputViewDock->setText(QString::fromUtf8(file.readAll()), scrollToBottom);
        file.close();
    }
}

void MainWindow::runSimulation()
{
    if (m_currentCalculationDir.isEmpty() || m_currentCalculationDir == m_workingDirectory || m_currentCalculationDir == "/" || m_currentCalculationDir == ".") {
        setupCalculationDirectory();
    }

    QString program = m_programSelector->currentText();
    if (!m_simulationPrograms.contains(program)) {
        // Claude Generated - Phase 4.1: Enhanced error dialog
        showEnhancedError(tr("No Program Selected"),
            tr("Please select a simulation program."),
            tr("Choose one of the available programs (curcuma, orca, or xtb) from the dropdown."),
            nullptr);
        return;
    }

    // Claude Generated 2026 - WP T3: validate the editors up front (CalculationRunner
    // assumes a valid request), assemble the request from the widgets/settings, then
    // hand the QProcess lifecycle off to the runner.
    const bool structureEmpty = m_structureView->toPlainText().isEmpty();
    const bool inputEmpty = m_inputView->toPlainText().isEmpty();

    CalculationRequest req;
    req.program = program;
    req.command = m_commandInput->text().trimmed();
    req.structureText = m_structureView->toPlainText();
    req.inputText = m_inputView->toPlainText();
    req.structureBase = m_structureFileEdit->text();
    req.structureExt = m_structureFileEditExtension->text();
    req.inputBase = m_inputFileEdit->text();
    req.inputExt = m_inputFileEditExtension->text();
    req.threads = m_threads->value();
    req.uniqueFileNames = m_uniqueFileNames->isChecked();
    req.calcDir = currentCalculationDir();

    if (program == "orca") {
        req.orcaBinaryPath = m_settings.orcaBinaryPath();
        if (req.orcaBinaryPath.isEmpty()) {
            // Claude Generated - Phase 4.1: Enhanced error dialog
            showEnhancedError(tr("ORCA Configuration Error"),
                tr("ORCA binary path is not configured."),
                tr("Please configure the ORCA binary directory in the settings."),
                [this]() {
                    configurePrograms();
                });
            return;
        }
        if (inputEmpty) {
            // Claude Generated - Phase 4.1: Enhanced error dialog
            showEnhancedError(tr("Input File Empty"),
                tr("Input file is empty."),
                tr("Please fill in the input file with ORCA configuration parameters."),
                nullptr);
            return;
        }
    } else {
        if (structureEmpty) {
            // Claude Generated - Phase 4.1: Enhanced error dialog
            showEnhancedError(tr("Structure Data Missing"),
                tr("Structure data is empty."),
                tr("Please fill in the structure data in the Structure tab. This is required for curcuma and xtb calculations."),
                nullptr);
            return;
        }
        req.programPath = m_settings.getProgramPath(program);
    }

    const CalculationEntry entry = m_calculationRunner->start(req);
    CalculationHistory::add(currentCalculationDir(), entry, m_uniqueFileNames->isChecked());

    // Claude Generated - Phase 2.2: Update workflow state
    updateWorkflowState(WorkflowState::CalculationRunning);

    // Claude Generated - Quick Win: Start calculation timer
    m_elapsedSeconds = 0;
    m_timerLabel->setText("00:00:00");
    if (!m_calculationTimer) {
        m_calculationTimer = new QTimer(this);
        connect(m_calculationTimer, &QTimer::timeout, [this]() {
            m_elapsedSeconds++;
            int hours = m_elapsedSeconds / 3600;
            int minutes = (m_elapsedSeconds % 3600) / 60;
            int seconds = m_elapsedSeconds % 60;
            m_timerLabel->setText(QString("%1:%2:%3")
                .arg(hours, 2, 10, QChar('0'))
                .arg(minutes, 2, 10, QChar('0'))
                .arg(seconds, 2, 10, QChar('0')));
        });
    }
    m_calculationTimer->start(1000);  // Update every second

    // Claude Generated - Phase 1.3: Show progress dialog
    if (m_progressDialog) {
        delete m_progressDialog;
    }
    m_progressDialog = new QProgressDialog(tr("Running calculation..."), tr("Cancel"), 0, 0, this);
    m_progressDialog->setWindowModality(Qt::WindowModal);
    m_progressDialog->setWindowTitle(tr("Calculation Progress"));
    m_progressDialog->show();

    // Connect cancel button to stop the calculation
    connect(m_progressDialog, &QProgressDialog::canceled, [this]() {
        if (m_calculationRunner->isRunning()) {
            m_calculationRunner->cancel();
            statusBar()->showMessage(tr("Calculation canceled by user"));
        }
    });

    // Claude Generated 2026 - WP T3: live output tail. The process redirects
    // stdout/stderr into the log file, so re-read it periodically until finish
    // (stopped in onCalculationFinished()).
    m_currentOutputFile = entry.outputFile;
    if (!m_outputUpdateTimer) {
        m_outputUpdateTimer = new QTimer(this);
        connect(m_outputUpdateTimer, &QTimer::timeout, [this]() {
            updateOutputView(currentCalculationDir() + QDir::separator() + m_currentOutputFile, true);
        });
    }
    m_outputUpdateTimer->start(1000); // Aktualisiere alle 1000 ms (1 Sekunde)

    // Zeige eine Information und setze den Cursor auf "Warten"
    statusBar()->showMessage(tr("Calculation running..."));
    QApplication::setOverrideCursor(Qt::WaitCursor);
}

// Claude Generated 2026 - WP T3: react to CalculationRunner::finished (echoes the
// entry from runSimulation()). Persists the final status, refreshes the output view
// and workflow state, and tears down the progress dialog + timers.
void MainWindow::onCalculationFinished(const CalculationEntry& entry, int exitCode)
{
    // Claude Generated - Phase 1.3: Close progress dialog
    if (m_progressDialog) {
        m_progressDialog->close();
        m_progressDialog = nullptr;
    }

    // Claude Generated - Quick Win: Stop calculation + output timers
    if (m_calculationTimer) {
        m_calculationTimer->stop();
    }
    if (m_outputUpdateTimer) {
        m_outputUpdateTimer->stop();
    }

    // Aktualisiere Status in der Historie
    CalculationEntry updatedEntry = entry;
    updatedEntry.status = (exitCode == 0) ? "completed" : "error";
    CalculationHistory::add(currentCalculationDir(), updatedEntry, m_uniqueFileNames->isChecked());

    updateOutputView(currentCalculationDir() + QDir::separator() + entry.outputFile);
    statusBar()->showMessage(exitCode == 0 ?
        tr("Calculation completed successfully") :
        tr("Calculation failed with error (Code: %1)").arg(exitCode));

    // Claude Generated - Phase 2.2: Update workflow state based on exit code
    if (exitCode == 0) {
        updateWorkflowState(WorkflowState::CalculationComplete);
    } else {
        updateWorkflowState(WorkflowState::CalculationError);
    }

    QApplication::restoreOverrideCursor();
}

void MainWindow::orcaPlotVib(const QString &filename, int frequency)
{
    QString orcaPath = m_settings.orcaBinaryPath();

        QString orcaExe = orcaPath + "/orca_pltvib";
        QString cfilename = QFileInfo(filename).fileName();
        // Claude Generated 2026 - WP T3: local process (the runner owns the calc one).
        QProcess process;
        process.setWorkingDirectory(currentCalculationDir());
        process.setProgram(orcaExe);
        process.setArguments(QStringList() << cfilename << QString::number(frequency));
        process.start();
        process.waitForFinished();

        QString vXXX;
        if(frequency < 10)
            vXXX = cfilename + ".v00" + QString::number(frequency);
        else if(frequency < 100)
            vXXX = cfilename + ".v0" + QString::number(frequency);
        else
            vXXX = cfilename + ".v" + QString::number(frequency);
        openWithVisualizer(currentCalculationDir() + QDir::separator()+ vXXX + ".xyz", "avogadro");
}

void MainWindow::openWithVisualizer(const QString &filePath, const QString &visualizer)
{         
    QString programPath = m_settings.getProgramPath(visualizer);

    if (programPath.isEmpty()) {
        QMessageBox::warning(this, tr("Error"),
            tr("Path for %1 not configured.").arg(visualizer));
        return;
    }
    
    QStringList arguments;

    if(filePath.contains("loc") || filePath.contains("gbw") || filePath.contains("ges")) 
    {
        QString orcaPath = m_settings.orcaBinaryPath();

        QString orcaExe = orcaPath + "/orca_2mkl";
        QString filename = QFileInfo(filePath).fileName();
        QString fileDir = QFileInfo(filePath).absolutePath();
        QFile::copy(filePath, fileDir + "/tmp.gbw");
        QString nfilePath = fileDir + "/tmp";
        // Claude Generated 2026 - WP T3: local process (the runner owns the calc one).
        QProcess process;
        process.setWorkingDirectory(currentCalculationDir());
        process.setProgram(orcaExe);
        process.setArguments(QStringList() << nfilePath << "-molden");
        process.start();
        process.waitForFinished();
        arguments << nfilePath + ".molden.input";
    }else
        arguments << filePath;  // Übergebe den Dateipfad als Argument

    // Debug-Ausgabe

    if (!QProcess::startDetached(programPath, arguments)) {
        QMessageBox::warning(this, tr("Error"),
            tr("Could not start %1.").arg(visualizer));
    }
}


void MainWindow::runCommand()
{
    QString program = m_programSelector->currentText();
    if (m_simulationPrograms.contains(program)) {
        runSimulation();
    }
}

void MainWindow::programSelected(int index)
{
    QString program = m_programSelector->itemText(index);
    if (m_simulationPrograms.contains(program)) {
        m_commandInput->setEnabled(true);
        m_commandInput->setPlaceholderText("Enter simulation command...");
    } else if (m_visualizerPrograms.contains(program)) {
        m_commandInput->setEnabled(false);
        m_commandInput->setPlaceholderText(tr("Visualization program - no command needed"));
    }
}

void MainWindow::projectSelected(const QModelIndex &index)
{
    // Claude Generated - Phase 4.2: Add file loading feedback
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Claude Generated - Fixed to extract relative directory name
    if (!index.isValid()) {
        QApplication::restoreOverrideCursor();
        return;
    }

    QString fullPath = m_projectModel->filePath(index);
    QDir workDir(m_workingDirectory);
    QString relativeName = workDir.relativeFilePath(fullPath);

    m_currentCalculationDir = relativeName;
    updateDirectoryContent();
    syncRightView();

    statusBar()->showMessage(tr("Loaded calculation directory"), 2000);

    // Claude Generated - Phase 2.2: Update workflow state
    updateWorkflowState(WorkflowState::DirectoryReady);

    // Claude Generated - Quick Win: Add to recent files
    addToRecentFiles(currentCalculationDir());

    QApplication::restoreOverrideCursor();
}


void MainWindow::loadSettings()
{
    m_workingDirectory = m_settings.workingDirectory();
    if (!m_workingDirectory.isEmpty()) {
        m_projectModel->setRootPath(m_workingDirectory);
        m_projectListView->setRootIndex(m_projectModel->index(m_workingDirectory));
    }
    // Claude Generated - Quick Win: Load recent files (now V2 with timestamps)
    m_recentFiles = m_settings.recentFilesV2();
    updateRecentFilesMenu();

    // Claude Generated Phase 4.3 - Initialize workspace manager and load UI
    if (!m_workspaceManager) {
        m_workspaceManager = new WorkspaceManager(this);
    }

    // Load bookmarks and workspaces into UI
    refreshBookmarkTree();
    updateWorkspaceList();
}


void MainWindow::updateDirectoryContent()
{
    // Claude Generated - Handle empty calculation directory
    QString fullPath = m_workingDirectory;
    if (isValidCalculationDir())
        fullPath = currentCalculationDir();

    const QModelIndex sourceRoot = m_directoryContentModel->setRootPath(fullPath);
    const QModelIndex proxyRoot = m_directoryContentProxyModel
        ? m_directoryContentProxyModel->mapFromSource(sourceRoot)
        : sourceRoot;
    m_directoryContentView->setRootIndex(proxyRoot);
}

QString MainWindow::filePathFromContentIndex(const QModelIndex& viewIndex) const
{
    if (!viewIndex.isValid())
        return QString();
    if (m_directoryContentProxyModel && m_directoryContentView &&
        m_directoryContentView->model() == m_directoryContentProxyModel)
        return m_directoryContentModel->filePath(m_directoryContentProxyModel->mapToSource(viewIndex));
    return m_directoryContentModel->filePath(viewIndex);
}

void MainWindow::syncRightView()
{
    // Claude Generated - Handle empty calculation directory
    if (!isValidCalculationDir()) {
        return;  // Nothing to sync if no calculation dir selected
    }

    QDir dir(currentCalculationDir());
    const qint64 LARGE_FILE_THRESHOLD = 1024 * 1024; // 1MB threshold

    // Claude Generated - Phase 4.2: Check file size for progress dialog
    QProgressDialog* loadingProgress = nullptr;

    QFile defaultStructure(currentCalculationDir() + QDir::separator() + "input.xyz");
    if (defaultStructure.exists() && defaultStructure.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // Show progress for large files
        if (defaultStructure.size() > LARGE_FILE_THRESHOLD) {
            loadingProgress = new QProgressDialog(tr("Loading structure file..."), tr("Cancel"), 0, 0, this);
            loadingProgress->setWindowModality(Qt::WindowModal);
            loadingProgress->show();
            QApplication::processEvents();
        }
        m_structureView->setPlainText(QString::fromUtf8(defaultStructure.readAll()));
        m_structureFileEdit->setText("input.xyz");
        defaultStructure.close();
        } else {
            // Wenn nicht vorhanden, nach anderen xyz-Dateien suchen
            QStringList xyzFiles = dir.entryList(QStringList() << "*.xyz", QDir::Files);
            if (!xyzFiles.isEmpty()) {
                QFile structureFile(dir.filePath(xyzFiles.first()));
                if (structureFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    m_structureView->setPlainText(QString::fromUtf8(structureFile.readAll()));
                    m_structureFileEdit->setText(xyzFiles.first());
                    structureFile.close();
                }
            } else {
                m_structureView->clear();
                m_structureFileEdit->setText("input.xyz"); // Setze Standard-Namen
            }
        }

    // Output-Dateien suchen und laden (*.log oder *.out)
    QStringList outputFiles = dir.entryList(QStringList() << "*.log" << "*.out", QDir::Files);
    if (!outputFiles.isEmpty()) {
        QFile outputFile(dir.filePath(outputFiles.first()));
        if (outputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            // Show progress for large output files
            if (outputFile.size() > LARGE_FILE_THRESHOLD) {
                if (!loadingProgress) {
                    loadingProgress = new QProgressDialog(tr("Loading output file..."), tr("Cancel"), 0, 0, this);
                    loadingProgress->setWindowModality(Qt::WindowModal);
                }
                loadingProgress->setLabelText(tr("Loading output file..."));
                loadingProgress->show();
                QApplication::processEvents();
            }
            m_outputViewDock->setText(QString::fromUtf8(outputFile.readAll()));
            outputFile.close();
        }
    } else {
        m_outputViewDock->clearOutput();
    }

    // Input-Datei suchen und laden
    QStringList inputFiles = dir.entryList(QStringList() << "input", QDir::Files);
    if (!inputFiles.isEmpty()) {
        QFile inputFile(dir.filePath(inputFiles.first()));
        if (inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_inputView->setPlainText(QString::fromUtf8(inputFile.readAll()));
            m_inputFileEdit->setText(inputFiles.first());
            inputFile.close();
        }
    } else {
        m_inputView->clear();
        m_inputFileEdit->clear();
    }

    // Close progress dialog if it was shown
    if (loadingProgress) {
        loadingProgress->close();
        delete loadingProgress;
    }
}

// Claude Generated Phase 3.2 - Build tree from BookmarkItem hierarchy
// Claude Generated 2026 - Phase 3: BookmarkWidget owns the tree. MainWindow only
// refreshes it after external changes (e.g. context-menu "Add to Bookmarks").
void MainWindow::refreshBookmarkTree()
{
    if (m_projectDock && m_projectDock->bookmarkWidget())
        m_projectDock->bookmarkWidget()->refresh();
}

void MainWindow::updatePathLabel(const QString& path)
{
    // Claude Generated Phase 1 - Update breadcrumb bar with current path
    m_breadcrumbBar->setPath(path);
}

// Aktualisiere die switchWorkingDirectory Funktion
// Claude Generated 2026 - WP T4: MainWindow bookkeeping after LessonController loaded
// an in-memory lesson structure into the viewer (LessonController::loadStructureFromIndex).
void MainWindow::onLessonStructureLoaded(const QString& name)
{
    m_currentMoleculeFilePath.clear();  // in-memory: force Save-As on a later save
    m_structureModified = false;
    if (m_saveAction) m_saveAction->setEnabled(true);
    if (m_saveAsAction) m_saveAsAction->setEnabled(true);
    captureInitialSnapshot(name, m_moleculeView->getCurrentFrameAtoms(),
        m_moleculeView->getCurrentFrameBonds());
}

void MainWindow::switchWorkingDirectory(const QString& path)
{
    if (path.isEmpty() || !QDir(path).exists()) {
        QMessageBox::warning(this, tr("Error"),
            tr("Directory does not exist: %1").arg(path));
        return;
    }

    m_workingDirectory = path;
    if (m_lessonController)
        m_lessonController->setWorkingDirectory(path);  // keep the save-dialog default in sync
    m_settings.setLastUsedWorkingDirectory(path);
    m_projectModel->setRootPath(path);
    m_projectListView->setRootIndex(m_projectModel->index(path));
    if (m_imageGalleryDock)
        m_imageGalleryDock->setWorkingDirectory(path);  // keep "show all in folder" scoped

    // Claude Generated - Reset current calculation directory when switching working dir
    m_currentCalculationDir.clear();
    m_currentProjectLabel->setText("");

    // Update views
    updateDirectoryContent();
    updatePathLabel(path); // Aktualisiere das Pfad-Label
    statusBar()->showMessage(tr("Working directory changed to: %1").arg(path));

    // Claude Generated - Phase 2.2: Reset workflow state when switching directories
    updateWorkflowState(WorkflowState::NoDirectory);
}

// Claude Generated (2026-04) - Toggles Project dock (Ctrl+B). Navigation is tabbed
// with Project so hiding the group hides both. Phase 6: routed through DockManager.
void MainWindow::toggleLeftPanel()
{
    if (!m_dockManager)
        return;
    m_dockManager->toggleLeftPanel();
    const bool show = m_dockManager->dockVisible(m_dockManager->projectDock());
    statusBar()->showMessage(
        show ? tr("Project panel shown") : tr("Project panel hidden"),
        1500);
}

QPair<int, int> MainWindow::countImaginaryFrequencies(const QString& filename) {
    
    m_frequencies.clear();
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QPair<int, int>(0,0); // Error opening file
    }

    QTextStream in(&file);
    QString line;
    int imagCount = 0, freqCount = 0;
    bool inFreqSection = false;

    // Search for the frequency section
    while (!in.atEnd()) {
        line = in.readLine();
        if (line.contains("$ir_spectrum")) {
            inFreqSection = true;
            in.readLine(); // Skip the number line
            break;
        }
    }

    // Count negative frequencies
    if (inFreqSection) {
        while (!in.atEnd()) {
            line = in.readLine().trimmed();
            if (line.startsWith("$end")) break;
            QRegularExpression spaceSplit("\\s+");
            // Split the line and check first number
            QStringList parts = line.split(spaceSplit, Qt::SkipEmptyParts);
            if (!parts.isEmpty()) {
                bool ok;
                double freq = parts[0].toDouble(&ok);
                if (ok && freq < 0) {
                    imagCount++;
                    m_frequencies.append(QPair<int, double>(m_frequencies.size(), freq));
                }else if(ok && freq > 0){
                    m_frequencies.append(QPair<int, double>(m_frequencies.size(), freq));
                    freqCount++;
                }
            }
        }
    }

    file.close();
    return QPair<int, int>(imagCount, freqCount);
}

// Claude Generated - Quick Win: Recent files management
void MainWindow::addToRecentFiles(const QString& path)
{
    // Claude Generated Phase 2 - Now using V2 with timestamps
    m_settings.addRecentFileV2(path);
    m_recentFiles = m_settings.recentFilesV2();
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu()
{
    m_recentFilesMenu->clear();
    if (m_recentFiles.isEmpty()) {
        m_recentFilesMenu->setEnabled(false);
        return;
    }
    m_recentFilesMenu->setEnabled(true);

    // Claude Generated Phase 2 - Group by date: Today, Yesterday, This Week, Older
    QDateTime today = QDateTime::currentDateTime();
    today.setTime(QTime(0, 0, 0));

    QMap<QString, QVector<Settings::RecentFileEntry>> grouped;
    const QString TODAY = "Today";
    const QString YESTERDAY = "Yesterday";
    const QString THIS_WEEK = "This Week";
    const QString OLDER = "Older";

    for (const auto& entry : m_recentFiles) {
        QDateTime entryDateTime = entry.lastAccessed;
        entryDateTime.setTime(QTime(0, 0, 0));

        int daysAgo = entryDateTime.daysTo(today);

        QString group;
        if (daysAgo == 0) group = TODAY;
        else if (daysAgo == 1) group = YESTERDAY;
        else if (daysAgo < 7) group = THIS_WEEK;
        else group = OLDER;

        grouped[group].append(entry);
    }

    // Add items by group in order
    QStringList groupOrder = {TODAY, YESTERDAY, THIS_WEEK, OLDER};
    for (const QString& group : groupOrder) {
        if (!grouped[group].isEmpty()) {
            // Add group label
            QAction* groupAction = m_recentFilesMenu->addAction(group);
            groupAction->setEnabled(false);
            QFont f = groupAction->font();
            f.setBold(true);
            groupAction->setFont(f);

            // Add items in this group
            for (const auto& entry : grouped[group]) {
                QFileInfo info(entry.path);
                QString displayText = info.fileName() + " (" + QDir(info.absolutePath()).dirName() + ")";
                QAction* action = m_recentFilesMenu->addAction(displayText);
                action->setData(entry.path);
                connect(action, &QAction::triggered, [this, path = entry.path]() {
                    openRecentFile(path);
                });
            }

            m_recentFilesMenu->addSeparator();
        }
    }

    m_recentFilesMenu->addAction(tr("Clear Recent Files"), [this]() {
        m_recentFiles.clear();
        m_settings.clearRecentFilesV2();
        updateRecentFilesMenu();
    });
}

void MainWindow::openRecentFile(const QString& path)
{
    if (!QDir(path).exists()) {
        QMessageBox::warning(this, tr("Directory Not Found"),
            tr("The directory '%1' no longer exists.").arg(path));
        // Claude Generated Phase 2 - Remove from V2 recent files
        m_recentFiles.erase(std::remove_if(m_recentFiles.begin(), m_recentFiles.end(),
            [&path](const Settings::RecentFileEntry& e) { return e.path == path; }), m_recentFiles.end());
        m_settings.setRecentFilesV2(m_recentFiles);
        updateRecentFilesMenu();
        return;
    }
    switchWorkingDirectory(path);
    addToRecentFiles(path);
}

// Claude Generated - Phase 1.2: Keyboard shortcut handlers
// Claude Generated 2026 - Single Escape handler (the key was doubly bound to
// cancelCalculation and clearAtomSelection, which made it ambiguous): cancel a
// running calculation first, otherwise clear selection + measurement marks.
void MainWindow::handleEscape()
{
    if (m_calculationRunner && m_calculationRunner->isRunning()) {
        cancelCalculation();
        return;
    }
    // Claude Generated 2026 - Esc steps back one level per press: a carried fragment is
    // dropped first, then the selection / measurement marks are cleared, then the tool
    // (Measure, Edit, Bonds, Build) is left for plain viewing.
    if (m_moleculeView) {
        if (m_moleculeView->fragmentCarryActive()) {
            m_moleculeView->cancelFragmentCarry();
            return;
        }
        if (!m_moleculeView->getSelectedAtoms().isEmpty()) {
            clearAtomSelection();
            return;
        }
        if (m_moleculeView->interactionMode() != MoleculeViewer::InteractionMode::None) {
            m_moleculeView->setInteractionMode(MoleculeViewer::InteractionMode::None);
            return;
        }
    }
    clearAtomSelection();
}

// Claude Generated 2026 - The Look menu: built-in and user looks (checked = the look the
// scene currently shows), save/delete, the colour scheme and the detailed settings.
void MainWindow::populateLookMenu()
{
    if (!m_lookMenu)
        return;
    m_lookMenu->clear();
    // clear() keeps sub-menu objects alive; drop the previous "Delete Look" menu. The
    // colour scheme menu is owned by the window, so it is not a child of this one.
    qDeleteAll(m_lookMenu->findChildren<QMenu*>(Qt::FindDirectChildrenOnly));
    const Look current = m_moleculeView ? m_moleculeView->currentLook() : Look();
    auto addLook = [this, &current](const Look& look) {
        QAction* a = m_lookMenu->addAction(look.name);
        a->setCheckable(true);
        a->setChecked(look.sameAppearance(current));
        connect(a, &QAction::triggered, this, [this, look]() {
            if (!m_moleculeView)
                return;
            m_moleculeView->applyLook(look);
            statusBar()->showMessage(tr("Look: %1").arg(look.name), 2000);
        });
    };
    for (const Look& look : looks::builtIn())
        addLook(look);
    const QVector<Look> user = m_settings.userLooks();
    if (!user.isEmpty()) {
        m_lookMenu->addSeparator();
        for (const Look& look : user)
            addLook(look);
    }

    m_lookMenu->addSeparator();
    QAction* saveAct = m_lookMenu->addAction(tr("Save Current Look…"));
    connect(saveAct, &QAction::triggered, this, [this]() {
        if (!m_moleculeView)
            return;
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Save Look"),
            tr("Name (colours, material, lighting, effects, background):"),
            QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || name.isEmpty())
            return;
        if (looks::isBuiltInName(name)) {
            QMessageBox::information(this, tr("Save Look"),
                tr("'%1' is a built-in look; please choose another name.").arg(name));
            return;
        }
        for (const Look& l : m_settings.userLooks()) {
            if (l.name.compare(name, Qt::CaseInsensitive) == 0
                && QMessageBox::question(this, tr("Save Look"),
                       tr("A look named '%1' already exists. Replace it?").arg(name))
                    != QMessageBox::Yes)
                return;
        }
        Look look = m_moleculeView->currentLook();
        look.name = name;
        m_settings.saveUserLook(look);
        statusBar()->showMessage(tr("Look '%1' saved").arg(name), 2000);
    });
    QMenu* deleteMenu = m_lookMenu->addMenu(tr("Delete Look"));
    deleteMenu->setEnabled(!user.isEmpty());
    for (const Look& look : user) {
        QAction* a = deleteMenu->addAction(look.name);
        connect(a, &QAction::triggered, this, [this, name = look.name]() {
            if (QMessageBox::question(this, tr("Delete Look"), tr("Delete the look '%1'?").arg(name))
                == QMessageBox::Yes)
                m_settings.deleteUserLook(name);
        });
    }

    m_lookMenu->addSeparator();
    if (m_colorSchemeMenu)
        m_lookMenu->addMenu(m_colorSchemeMenu);
    QAction* details = m_lookMenu->addAction(tr("Details…"));
    details->setToolTip(tr("Open the Appearance dock (style, material, lighting, effects)"));
    connect(details, &QAction::triggered, this, &MainWindow::openVisualizationSettings);
}

// Claude Generated 2026 - One checkable entry per molecule kind of the loaded structure
// (checked = hidden), most numerous first, plus "Show All".
void MainWindow::populateMoleculeKindsMenu()
{
    if (!m_moleculeKindsMenu)
        return;
    m_moleculeKindsMenu->clear();
    if (!m_moleculeView)
        return;
    const auto kinds = m_moleculeView->moleculeKinds();
    const QSet<QString> hidden = m_moleculeView->hiddenMoleculeKinds();

    QAction* showAll = m_moleculeKindsMenu->addAction(tr("Show All"));
    showAll->setEnabled(!hidden.isEmpty());
    connect(showAll, &QAction::triggered, this, [this]() {
        if (m_moleculeView)
            m_moleculeView->setHiddenMoleculeKinds({});
    });
    m_moleculeKindsMenu->addSeparator();

    if (kinds.size() < 2) {
        QAction* note = m_moleculeKindsMenu->addAction(tr("Only one kind of molecule loaded"));
        note->setEnabled(false);
        return;
    }
    for (const auto& kind : kinds) {
        QAction* a = m_moleculeKindsMenu->addAction(
            tr("Hide %1  (×%2)").arg(kind.first).arg(kind.second));
        a->setCheckable(true);
        a->setChecked(hidden.contains(kind.first));
        connect(a, &QAction::toggled, this, [this, formula = kind.first](bool on) {
            if (!m_moleculeView)
                return;
            QSet<QString> set = m_moleculeView->hiddenMoleculeKinds();
            if (on)
                set.insert(formula);
            else
                set.remove(formula);
            m_moleculeView->setHiddenMoleculeKinds(set);
        });
    }
}

// Claude Generated 2026 - View ▸ Views: the quick camera orientations and the saved
// camera views of the Appearance dock, rebuilt on every opening.
void MainWindow::populateViewsMenu()
{
    if (!m_viewsMenu)
        return;
    m_viewsMenu->clear();
    const struct { int axis; QString label; } quick[] = {
        { 0, tr("Front") }, { 1, tr("Top") }, { 2, tr("Side") },
    };
    for (const auto& q : quick) {
        QAction* a = m_viewsMenu->addAction(q.label);
        connect(a, &QAction::triggered, this, [this, axis = q.axis]() {
            if (m_moleculeView)
                m_moleculeView->setCameraOrientation(quickViewOrientation(axis));
        });
    }
    const QVector<ViewPreset> saved = m_settings.viewPresets();
    if (!saved.isEmpty()) {
        m_viewsMenu->addSeparator();
        for (const ViewPreset& preset : saved) {
            QAction* a = m_viewsMenu->addAction(preset.name);
            connect(a, &QAction::triggered, this, [this, preset]() {
                if (m_moleculeView)
                    m_moleculeView->applyViewPreset(preset);
            });
        }
    }
    m_viewsMenu->addSeparator();
    QAction* manage = m_viewsMenu->addAction(tr("Manage Views…"));
    manage->setToolTip(tr("Open the Appearance dock to save and delete camera views."));
    connect(manage, &QAction::triggered, this, &MainWindow::openVisualizationSettings);
}

// Claude Generated 2026 - Viewport context menu (UX stage 5). On an atom: the atom
// actions. On empty space: camera, the quick toggles, Style, Look, deselect and the
// quick photo. Same QActions/QMenus as the menu bar, so checked states always match.
// (In Build mode a right-click on an atom deletes it instead, see MoleculeViewer.)
void MainWindow::showViewportContextMenu(const QPoint& globalPos, int atomIndex)
{
    QMenu menu(this);
    if (atomIndex >= 0 && m_moleculeView) {
        QAction* attach = menu.addAction(
            tr("Add Bonded Atom (%1)").arg(m_moleculeView->buildElement()));
        connect(attach, &QAction::triggered, this,
            [this, atomIndex]() { m_moleculeView->buildAttachAtom(atomIndex); });

        QAction* changeEl = menu.addAction(tr("Change Element…"));
        connect(changeEl, &QAction::triggered, this, [this, atomIndex]() {
            const auto atoms = m_moleculeView->getCurrentFrameAtoms();
            if (atomIndex >= atoms.size())
                return;
            bool ok = false;
            QString s = QInputDialog::getText(this, tr("Change Element"),
                tr("Element symbol:"), QLineEdit::Normal, atoms[atomIndex].element, &ok)
                            .trimmed();
            if (!ok || s.isEmpty())
                return;
            s = s.left(1).toUpper() + s.mid(1).toLower();
            if (!elem::isElementSymbol(s)) {
                statusBar()->showMessage(tr("Unknown element: %1").arg(s), 3000);
                return;
            }
            takeSnapshot(tr("Before element change"));  // Claude Generated 2026
            m_moleculeView->setAtomInCurrentFrame(atomIndex, s, atoms[atomIndex].position);
        });

        QAction* delAtom = menu.addAction(tr("Delete Atom"));
        connect(delAtom, &QAction::triggered, this, [this, atomIndex]() {
            m_moleculeView->selectAtoms({ atomIndex }, false);
            m_moleculeView->deleteSelection();
        });
        QAction* addH = menu.addAction(tr("Add Hydrogens Here"));
        connect(addH, &QAction::triggered, this,
            [this, atomIndex]() { m_moleculeView->addHydrogens({ atomIndex }); });
        QAction* addHAll = menu.addAction(tr("Add Hydrogens (All Atoms)"));
        connect(addHAll, &QAction::triggered, this,
            [this]() { m_moleculeView->addHydrogens(); });
        // Claude Generated 2026 - Dock a substituent fragment onto this atom.
        QMenu* fragMenu = menu.addMenu(tr("Attach Fragment"));
        const auto& library = build::fragmentLibrary();
        for (int i = 0; i < library.size(); ++i) {
            if (library[i].attachAtom < 0)
                continue;
            QAction* fa = fragMenu->addAction(library[i].name);
            connect(fa, &QAction::triggered, this, [this, i, atomIndex]() {
                m_moleculeView->attachFragment(build::fragmentLibrary()[i], atomIndex);
            });
        }
    } else {
        menu.addAction(m_fitViewAction);
        menu.addAction(m_centerSelectionAction);
        menu.addSeparator();
        menu.addAction(m_nciToggleAction);
        menu.addAction(m_hbondToggleAction);
        menu.addMenu(m_hydrogenMenu);
        menu.addMenu(m_moleculeKindsMenu);
        menu.addMenu(m_renderStyleMenu);
        menu.addMenu(m_lookMenu);
        menu.addSeparator();
        menu.addAction(m_deselectAction);  // also clears measurement marks
        menu.addAction(m_quickPhotoAction);
    }
    menu.exec(globalPos);
}

// Claude Generated 2026 - Refresh the permanent status-bar indicators from the
// viewer (atom count of the current frame, frame position for trajectories).
void MainWindow::updateStatusIndicators()
{
    if (!m_moleculeView || !m_statusAtomsLabel || !m_statusFrameLabel)
        return;
    const int atoms = m_moleculeView->getCurrentFrameAtoms().size();
    m_statusAtomsLabel->setText(atoms > 0 ? tr("%1 atoms").arg(atoms) : QString());
    const int frames = m_moleculeView->getFrameCount();
    if (frames > 1) {
        m_statusFrameLabel->setText(
            tr("Frame %1/%2").arg(m_moleculeView->getCurrentFrame() + 1).arg(frames));
        m_statusFrameLabel->setVisible(true);
    } else {
        m_statusFrameLabel->setVisible(false);
    }
}

// Claude Generated 2026 - Edit ▸ Undo (Ctrl+Z): restore the newest snapshot and
// consume it, so repeated Ctrl+Z walks back through the history. Snapshot 0
// (the original geometry) is restored but never removed.
void MainWindow::undoLastSnapshot()
{
    if (m_snapshots.isEmpty()) {
        statusBar()->showMessage(tr("Nothing to undo — no snapshots yet."), 2500);
        return;
    }
    const int last = m_snapshots.size() - 1;
    const MoleculeSnapshot snap = m_snapshots[last];
    restoreSnapshot(snap);
    if (last > 0) {
        m_snapshots.removeAt(last);
        if (m_snapshotsWidget)
            m_snapshotsWidget->removeSnapshotAt(last);
    }
    statusBar()->showMessage(tr("Undo: %1").arg(snap.name), 2500);
}

// Claude Generated 2026 - File ▸ New Scene: clear everything and enter Build
// mode. The previous structure is asked about when modified and always remains
// reachable through the Snapshots tab.
void MainWindow::newScene()
{
    if (!m_moleculeView)
        return;
    if (m_structureModified) {
        const auto answer = QMessageBox::question(this, tr("New Scene"),
            tr("Discard the current (modified) structure and start an empty scene?\n"
               "The current state stays available in the Snapshots tab."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }
    m_moleculeView->newScene();
    m_structureModified = false;
    if (m_statusFileLabel)
        m_statusFileLabel->clear();
    m_moleculeView->setBuildMode(true);
    statusBar()->showMessage(
        tr("New empty scene — click in the viewport to place the first atom."), 4000);
}

// Claude Generated 2026 - One-click PNG export into the working directory
// (viewer-bar Photo button, viewport context menu).
void MainWindow::quickExportPhoto()
{
    if (!m_moleculeView)
        return;
    const QString saved = m_moleculeView->quickExportImage(m_workingDirectory, &m_settings);
    if (!saved.isEmpty())
        statusBar()->showMessage(tr("Image exported: %1").arg(saved), 5000);
    else
        statusBar()->showMessage(tr("Quick export failed — is a molecule loaded?"), 5000);
}

void MainWindow::cancelCalculation()
{
    if (m_calculationRunner->isRunning()) {
        m_calculationRunner->cancel();
        statusBar()->showMessage(tr("Calculation canceled"));
    }
}

void MainWindow::switchEditorTab()
{
    // Find editor tabs widget and switch to next tab
    // This is a simple implementation - can be improved
    QTabWidget* tabWidget = findChild<QTabWidget*>();
    if (tabWidget) {
        int currentIndex = tabWidget->currentIndex();
        int nextIndex = (currentIndex + 1) % tabWidget->count();
        tabWidget->setCurrentIndex(nextIndex);
    }
}

// Claude Generated 2026 - Central molecule-save routine. Used by:
//   - File > Save / File > Save As (Ctrl+S / Ctrl+Shift+S)
//   - The Save button inside the simulation dock
//   - The Save/Discard/Cancel prompt when opening a new structure
//
// Strategy:
//   1. If `path` is empty AND the loaded source is .xyz → overwrite it.
//   2. Otherwise open a Save-As dialog with the .xyz default filter.
//   3. Build an XYZ frame from the current viewer geometry and write it.
bool MainWindow::saveStructure(const QString& path)
{
    if (!m_moleculeView) {
        statusBar()->showMessage(tr("No molecule viewer"), 3000);
        return false;
    }
    const QVector<MoleculeViewer::Atom> atoms = m_moleculeView->getCurrentFrameAtoms();
    if (atoms.isEmpty()) {
        statusBar()->showMessage(tr("No molecule to save"), 3000);
        return false;
    }

    // 1) Resolve target path
    QString targetPath = path;
    if (targetPath.isEmpty()) {
        const QString suffix = QFileInfo(m_currentMoleculeFilePath).suffix().toLower();
        if (suffix == QLatin1String("xyz") && QFile::exists(m_currentMoleculeFilePath)) {
            targetPath = m_currentMoleculeFilePath;  // silent overwrite of source
        } else {
            const QString startDir = m_workingDirectory.isEmpty()
                ? QDir::homePath() : m_workingDirectory;
            targetPath = QFileDialog::getSaveFileName(this,
                tr("Save Molecule File As"),
                QDir(startDir).filePath(
                    QFileInfo(m_currentMoleculeFilePath).completeBaseName().isEmpty()
                        ? QStringLiteral("structure.xyz")
                        : QFileInfo(m_currentMoleculeFilePath).completeBaseName() + QStringLiteral(".xyz")),
                tr("XYZ Files (*.xyz);;All Files (*)"));
            if (targetPath.isEmpty())  // user cancelled
                return false;
            // Force .xyz suffix if the user typed something else — XYZParser::writeFile
            // writes raw text regardless, but the dialog filter and recent-files menu
            // behave more predictably with a .xyz extension.
            if (QFileInfo(targetPath).suffix().isEmpty())
                targetPath += QStringLiteral(".xyz");
        }
    }

    // 2) Build the frame
    XYZParser::XYZFrame frame;
    const QString comment = tr("Saved from Qurcuma after MD/Optimization");
    XYZParser::convertFromMoleculeViewer(atoms, comment, frame);

    // 3) Write
    if (!XYZParser::writeFile(targetPath, frame)) {
        QMessageBox::critical(this, tr("Save failed"),
            tr("Could not write %1").arg(targetPath));
        return false;
    }

    // 4) Update application state
    m_currentMoleculeFilePath = targetPath;
    m_structureModified = false;
    if (m_simulationControlWidget)
        m_simulationControlWidget->setStructureModified(false);
    if (m_structureFileEdit)
        m_structureFileEdit->setText(QFileInfo(targetPath).fileName());
    statusBar()->showMessage(tr("Saved: %1").arg(targetPath), 3000);
    addToRecentFiles(targetPath);
    return true;
}

bool MainWindow::saveCurrentStructure()
{
    return saveStructure(QString());
}

void MainWindow::saveCurrentStructureAs()
{
    // Pass an explicitly-empty path AND clear the cached source path so the
    // saveStructure() resolution always falls through to the Save-As dialog,
    // even when the source was a .xyz file.
    const QString oldPath = m_currentMoleculeFilePath;
    m_currentMoleculeFilePath.clear();
    const bool ok = saveStructure(QString());
    if (!ok)
        m_currentMoleculeFilePath = oldPath;  // user cancelled; keep old target
}


// ============================================================================
// Bidirectional structure sync helpers (viewer <-> atom table <-> text editor).
// Claude Generated 2026. The viewer is the canonical store; callers manage the
// m_structSyncing re-entrancy guard.
// ============================================================================

// Push the viewer's current-frame geometry into the atom table.
void MainWindow::updateAtomTableFromViewer()
{
    if (!m_atomListPanel || !m_moleculeView)
        return;
    m_atomListPanel->updateAtomList(
        m_moleculeView->getAtomPositions(),
        m_moleculeView->getAtomElements(),
        m_moleculeView->getAtomCharges());
}

// Mirror the viewer's current-frame geometry into the structure text editor as XYZ.
// Single-frame structures only (a trajectory's text stays the loaded file), and
// never while the user is typing in the editor (focus) — that would fight them.
void MainWindow::updateStructureTextFromViewer()
{
    if (!m_structureView || !m_moleculeView)
        return;
    if (!m_moleculeView->canEditStructure() || m_structureView->hasFocus())
        return;
    const QVector<MoleculeViewer::Atom> atoms = m_moleculeView->getCurrentFrameAtoms();
    if (atoms.isEmpty())
        return;
    const QString comment = QFileInfo(m_currentMoleculeFilePath).fileName();
    QSignalBlocker block(m_structureView);  // don't trip modificationChanged
    m_structureView->setPlainText(atomsToXyz(atoms, comment));
}

// "Apply → Viewer": parse the editor text as XYZ and replace the current structure.
void MainWindow::applyStructureTextToViewer()
{
    if (!m_structureView || !m_moleculeView)
        return;
    if (!m_moleculeView->canEditStructure()) {
        statusBar()->showMessage(tr("Apply works on single-frame structures only"), 3000);
        return;
    }
    QVector<MoleculeViewer::Atom> atoms;
    if (!xyzToAtoms(m_structureView->toPlainText(), atoms)) {
        statusBar()->showMessage(tr("Could not parse the editor text as XYZ"), 3000);
        return;
    }
    m_structSyncing = true;
    const bool ok = m_moleculeView->applyStructureFromAtoms(atoms);
    if (ok) {
        if (m_simulationControlWidget)
            m_simulationControlWidget->setMolecule(m_moleculeView->getCurrentFrameAtoms(),
                m_moleculeView->getCurrentFrameBonds());
        updateAtomTableFromViewer();  // refresh table (text is the source, leave it)
        m_structureModified = true;
        statusBar()->showMessage(tr("Structure updated from editor (%1 atoms)").arg(atoms.size()), 3000);
    }
    m_structSyncing = false;
}

// Claude Generated 2026 - Restore the first snapshot (index 0), which is always
// the geometry as it was when the molecule was loaded. If no snapshot exists,
// fall back to reloading the source file.
void MainWindow::resetToOriginalSnapshot()
{
    if (m_snapshots.isEmpty()) {
        // Fall back to reloading the source file if no snapshot exists.
        if (m_currentMoleculeFilePath.isEmpty() || !QFile::exists(m_currentMoleculeFilePath)) {
            statusBar()->showMessage(tr("No original structure to reset to"), 2000);
            return;
        }
        if (m_simulationControlWidget)
            m_simulationControlWidget->onStopClicked();
        m_structureModified = false;
        if (m_simulationControlWidget)
            m_simulationControlWidget->setStructureModified(false);
        loadMoleculeFile(m_currentMoleculeFilePath);
        statusBar()->showMessage(tr("Reloaded original structure"), 2000);
        return;
    }

    restoreSnapshot(m_snapshots[0]);

    // Reset means "back to the loaded original", so the modified flag must be
    // cleared afterwards. restoreSnapshot() sets it to true because restoring an
    // arbitrary snapshot is a user edit; for the initial snapshot it is not.
    m_structureModified = false;
    if (m_simulationControlWidget)
        m_simulationControlWidget->setStructureModified(false);

    statusBar()->showMessage(tr("Reset to original structure"), 2000);
}

// Claude Generated 2026 - Store the initial snapshot (index 0) from the loaded geometry.
// Called once per loadMoleculeFile(); clears any previous snapshots, creates snapshot 0
// from the first frame, and enables the Reset button.
void MainWindow::captureInitialSnapshot(const QString& filePath,
    const QVector<MoleculeViewer::Atom>& atoms,
    const QVector<MoleculeViewer::Bond>& bonds)
{
    m_snapshots.clear();
    if (m_snapshotsWidget)
        m_snapshotsWidget->clearSnapshots();
    if (m_simulationControlWidget)
        m_simulationControlWidget->setResetEnabled(false);

    MoleculeSnapshot snap;
    snap.name = QFileInfo(filePath).fileName();
    snap.timestamp = QDateTime::currentDateTime();
    snap.atoms = atoms;
    snap.bonds = bonds;
    m_snapshots.append(snap);

    if (m_snapshotsWidget)
        m_snapshotsWidget->addSnapshot(snap);
    if (m_simulationControlWidget)
        m_simulationControlWidget->setResetEnabled(true);
}

// Claude Generated 2026 - Capture the current viewer geometry as a named snapshot.
void MainWindow::takeSnapshot(const QString& name)
{
    if (!m_moleculeView)
        return;
    const QVector<MoleculeViewer::Atom> atoms = m_moleculeView->getCurrentFrameAtoms();
    const QVector<MoleculeViewer::Bond> bonds = m_moleculeView->getCurrentFrameBonds();
    if (atoms.isEmpty())
        return;

    MoleculeSnapshot snap;
    snap.name = name.isEmpty()
        ? tr("Snapshot %1").arg(m_snapshots.size() + 1)
        : name;
    snap.timestamp = QDateTime::currentDateTime();
    snap.atoms = atoms;
    snap.bonds = bonds;
    m_snapshots.append(snap);

    if (m_snapshotsWidget)
        m_snapshotsWidget->addSnapshot(snap);
}

// Claude Generated 2026 - Restore any snapshot to the viewer and simulation dock.
void MainWindow::restoreSnapshot(const MoleculeSnapshot& snapshot)
{
    if (!m_moleculeView)
        return;

    if (m_simulationControlWidget)
        m_simulationControlWidget->onStopClicked();

    m_moleculeView->setTrajectoryData(
        QVector<QVector<MoleculeViewer::Atom>>{ snapshot.atoms },
        QVector<QVector<MoleculeViewer::Bond>>{ snapshot.bonds });
    if (m_simulationControlWidget)
        m_simulationControlWidget->setMolecule(snapshot.atoms, snapshot.bonds);

    m_structureModified = true;
    if (m_simulationControlWidget)
        m_simulationControlWidget->setStructureModified(true);

    statusBar()->showMessage(tr("Restored snapshot: %1").arg(snapshot.name), 2000);
}

// Claude Generated - Quick Win: Auto-save drafts
void MainWindow::autoSaveDrafts()
{
    // Create drafts directory if needed
    QString draftsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/drafts";
    QDir().mkpath(draftsDir);

    // Save structure editor
    if (m_structureView->document()->isModified()) {
        QFile structDraft(draftsDir + "/structure.draft");
        if (structDraft.open(QIODevice::WriteOnly | QIODevice::Text)) {
            structDraft.write(m_structureView->toPlainText().toUtf8());
            structDraft.close();
        }
    }

    // Save input editor
    if (m_inputView->document()->isModified()) {
        QFile inputDraft(draftsDir + "/input.draft");
        if (inputDraft.open(QIODevice::WriteOnly | QIODevice::Text)) {
            inputDraft.write(m_inputView->toPlainText().toUtf8());
            inputDraft.close();
        }
    }
}

void MainWindow::loadDrafts()
{
    QString draftsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/drafts";

    // Load structure draft
    QFile structDraft(draftsDir + "/structure.draft");
    if (structDraft.exists() && structDraft.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_structureView->setPlainText(QString::fromUtf8(structDraft.readAll()));
        structDraft.close();
    }

    // Load input draft
    QFile inputDraft(draftsDir + "/input.draft");
    if (inputDraft.exists() && inputDraft.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_inputView->setPlainText(QString::fromUtf8(inputDraft.readAll()));
        inputDraft.close();
    }
}

// Claude Generated - Quick Win: Copy/Paste structures
// Claude Generated 2026 - Parse the first frame of a structure file into viewer
// atoms/bonds via the shared MoleculeFileLoader (xyz/vtf/pdb/mol2).
bool MainWindow::parseFirstFrame(const QString& filePath, QVector<MoleculeViewer::Atom>& atoms,
    QVector<MoleculeViewer::Bond>& bonds)
{
    atoms.clear();
    bonds.clear();
    const MoleculeFileLoader::Result r = MoleculeFileLoader::load(filePath);
    if (!r.frames.isEmpty()) {
        atoms = r.frames.first();
        bonds = r.frameBonds.first();
    }
    return !atoms.isEmpty();
}

// Claude Generated 2026 - Merge a molecule from a file into the current scene (single
// frame only). The added atoms arrive selected and in Edit/placement mode with live
// clash feedback; drag to place and use Resolve clashes if they overlap.
void MainWindow::addMoleculeToScene()
{
    if (!m_moleculeView)
        return;
    if (!m_moleculeView->canEditStructure()) {
        QMessageBox::information(this, tr("Add Molecule to Scene"),
            tr("Merging is only available for single-frame structures, not trajectories."));
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, tr("Add Molecule to Scene"), QString(),
        tr("Molecule files (*.xyz *.vtf *.pdb *.mol2);;All files (*)"));
    if (path.isEmpty())
        return;
    mergeFileIntoScene(path);
}

// Claude Generated 2026 - Parse a file's first frame and append it to the current scene.
// Shared by the Edit menu and the file-browser right-click "Add to current scene".
void MainWindow::mergeFileIntoScene(const QString& filePath)
{
    if (!m_moleculeView || filePath.isEmpty())
        return;
    if (!m_moleculeView->canEditStructure()) {
        QMessageBox::information(this, tr("Add Molecule to Scene"),
            tr("Merging is only available for single-frame structures, not trajectories."));
        return;
    }
    QVector<MoleculeViewer::Atom> atoms;
    QVector<MoleculeViewer::Bond> bonds;
    if (!parseFirstFrame(filePath, atoms, bonds)) {
        QMessageBox::warning(this, tr("Add Molecule to Scene"),
            tr("Could not read a structure from:\n%1").arg(filePath));
        return;
    }
    m_moleculeView->appendMolecule(atoms, bonds);  // selects new atoms + enters placement
    statusBar()->showMessage(
        tr("Added %1 atoms — drag to place, then Resolve clashes if needed").arg(atoms.size()), 3000);
}

void MainWindow::copyStructureToClipboard()
{
    QString structureText = m_structureView->toPlainText();

    if (structureText.isEmpty()) {
        statusBar()->showMessage(tr("No structure to copy"), 2000);
        return;
    }

    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(structureText);
    statusBar()->showMessage(tr("Structure copied to clipboard"), 2000);
}

void MainWindow::pasteStructureFromClipboard()
{
    QClipboard* clipboard = QApplication::clipboard();
    QString clipboardText = clipboard->text();

    if (clipboardText.isEmpty()) {
        statusBar()->showMessage(tr("Clipboard is empty"), 2000);
        return;
    }

    // Check if clipboard contains structure data (basic heuristic)
    if (clipboardText.contains(QRegularExpression("^\\s*\\d+\\s*$", QRegularExpression::MultilineOption)) ||
        clipboardText.contains("xyz") || clipboardText.contains("atom") || clipboardText.contains("C H O N")) {
        m_structureView->setPlainText(clipboardText);
        statusBar()->showMessage(tr("Structure pasted from clipboard"), 2000);
    } else {
        statusBar()->showMessage(tr("Clipboard content doesn't look like a structure file"), 2000);
    }
}

// Claude Generated - Quick Fix: Clear output view
void MainWindow::clearOutputView()
{
    m_outputViewDock->clearOutput();
    statusBar()->showMessage(tr("Output cleared"), 1500);
}

// Claude Generated - Quick Fix: Copy current path to clipboard
void MainWindow::copyCurrentPath()
{
    QString fullPath = m_workingDirectory + QDir::separator() + m_currentCalculationDir;
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(fullPath);
    statusBar()->showMessage(tr("Path copied to clipboard: %1").arg(fullPath), 3000);
}

// Claude Generated - Visualization Settings Dialog
// Claude Generated 2026 - Display options now live in the docked DisplayPanel
// (the former modal dialog was retired). This just surfaces the dock.
// Claude Generated 2026 - Opens the Appearance dock (UX stage 4; it starts closed).
void MainWindow::openVisualizationSettings()
{
    if (!m_appearanceDock)
        return;
    if (m_displayPanel)
        m_displayPanel->syncFromViewer();
    m_appearanceDock->show();
    m_appearanceDock->raise();
}

// Claude Generated 2026 - RMSD / align / reorder tool (curcuma RMSDDriver),
// embedded as a tab in the Editors dock. Raises the Editors dock + RMSD tab and
// re-seeds the reference from the current viewer frame; optional targetFile
// preloads the comparison structure (used by the file-manager context menu).
void MainWindow::showRMSDTool(const QString& targetFile)
{
    if (!m_moleculeView) {
        QMessageBox::warning(this, tr("No Viewer"), tr("Molecule viewer is not available."));
        return;
    }

    if (!m_rmsdWidget)
        return;  // tab not built yet (should not happen post-construction)

    // Auto-seed the reference from the currently displayed structure, but only if the
    // workspace has no reference yet (don't clobber a user-chosen reference on re-open).
    if (!m_rmsdWidget->hasReference())
        seedRMSDReference();

    // Focus the Simulation dock and switch to the RMSD / Align tab first, so the new
    // overlay row + status feedback are visible when the alignment runs.
    if (m_simulationDock) {
        m_simulationDock->show();
        m_simulationDock->raise();
        m_simulationDock->activateWindow();
    }
    if (m_simulationTabs)
        m_simulationTabs->setCurrentIndex(2);  // Simulation=0, Snapshots=1, RMSD=2, Input=3

    // From the file-manager context menu: load the structure, align it against the
    // current reference and add it to the workspace in one step (structureAligned()
    // drives the status-bar feedback). The user can still re-align with other options.
    if (!targetFile.isEmpty())
        m_rmsdWidget->addStructureFromFile(targetFile);
}

// Claude Generated 2026 - Re-seed the RMSD reference from the current viewer
// frame. Called on Analysis-menu/context-menu invocation and by the widget's
// "Use current as reference" button (seedReferenceRequested).
void MainWindow::seedRMSDReference()
{
    if (!m_moleculeView || !m_rmsdWidget)
        return;

    const QVector<MoleculeViewer::Atom> refAtoms = m_moleculeView->getCurrentFrameAtoms();
    if (refAtoms.isEmpty()) {
        QMessageBox::information(this, tr("RMSD"),
            tr("Load a structure first — it becomes the alignment reference."));
        return;
    }
    const QString refName = m_currentMoleculeFilePath.isEmpty()
        ? tr("current structure")
        : QFileInfo(m_currentMoleculeFilePath).fileName();
    // Carry the canonical path so the workspace's duplicate check catches a target
    // file that is the very same file as the reference (e.g. "Overlay onto current"
    // invoked on the loaded molecule's own file).
    QString refPath = m_currentMoleculeFilePath;
    if (!refPath.isEmpty()) {
        const QString canonical = QFileInfo(refPath).canonicalFilePath();
        if (!canonical.isEmpty())
            refPath = canonical;
    }
    m_rmsdWidget->setReferenceStructure(refAtoms, m_moleculeView->getCurrentFrameBonds(), refName, refPath);
}

// Claude Generated - Quick Fix: Show about dialog
void MainWindow::showAboutDialog()
{
    QMessageBox aboutBox(this);
    aboutBox.setWindowTitle(tr("About Qurcuma"));
    aboutBox.setIcon(QMessageBox::Information);
    aboutBox.setText(tr("Qurcuma %1").arg(QCoreApplication::applicationVersion()));
    aboutBox.setInformativeText(
        tr("Molecular viewer, builder and interactive simulation front end for curcuma.\n\n"
           "Reads XYZ, VTF, PDB and MOL2 files, runs molecular dynamics and geometry "
           "optimizations through curcuma, and launches curcuma, ORCA and xtb calculations."));
    aboutBox.setDetailedText(
        tr("Built with Qt %1\n"
           "Copyright (C) 2015 - 2026 Conrad Hübler").arg(QT_VERSION_STR));
    aboutBox.exec();
}

// Claude Generated - Phase 4.1: Enhanced error dialog with optional fix action
void MainWindow::showEnhancedError(const QString& title, const QString& problem,
                                   const QString& solution, std::function<void()> actionCallback)
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(title);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText(problem);
    msgBox.setInformativeText(solution);
    msgBox.setStandardButtons(QMessageBox::Ok);

    // Add optional "Fix Now" button if callback provided
    if (actionCallback) {
        QPushButton* fixButton = msgBox.addButton(tr("Fix Now"), QMessageBox::ActionRole);
        msgBox.exec();
        if (msgBox.clickedButton() == fixButton) {
            actionCallback();
        }
    } else {
        msgBox.exec();
    }
}

// Claude Generated - Phase 2.2: Workflow state management
void MainWindow::updateWorkflowState(WorkflowState state)
{
    m_workflowState = state;

    // Update button states based on workflow state
    bool canCreateDir = (state == WorkflowState::NoDirectory || state == WorkflowState::DirectoryReady);
    bool canRunCalc = (state == WorkflowState::DirectoryReady);
    bool canEditFiles = (state == WorkflowState::DirectoryReady || state == WorkflowState::NoDirectory);

    m_newCalculationButton->setEnabled(canCreateDir);
    m_runCalculation->setEnabled(canRunCalc);
    m_structureView->setReadOnly(!canEditFiles);
    m_inputView->setReadOnly(!canEditFiles);

    // Update status bar and visual indicators with state information
    QString stateMessage;
    QString stateColor;
    switch (state) {
        case WorkflowState::NoDirectory:
            stateMessage = tr("No calculation directory selected");
            stateColor = "grey";
            break;
        case WorkflowState::DirectoryReady:
            stateMessage = tr("Ready to run calculation");
            stateColor = "blue";
            break;
        case WorkflowState::CalculationRunning:
            stateMessage = tr("Calculation running...");
            stateColor = "orange";
            break;
        case WorkflowState::CalculationComplete:
            stateMessage = tr("Calculation completed successfully");
            stateColor = "green";
            break;
        case WorkflowState::CalculationError:
            stateMessage = tr("Calculation failed with error");
            stateColor = "red";
            break;
    }
    statusBar()->showMessage(stateMessage, 5000);

    // Claude Generated - Phase 3.3: Update visual state indicators
    if (m_stateIcon && m_stateIndicator) {
        m_stateIcon->setStyleSheet(QString("color: %1; font-size: 14px;").arg(stateColor));
        m_stateIndicator->setText(stateMessage);
    }
}

// Claude Generated - Visual Polish: Apply stylesheet (dark/light mode)
// Claude Generated 2026 - The .qss files in stylesheets/ are intentionally NOT
// applied any more. They were a stop-gap "Material-style" theme that
// overrode Qt's native look-and-feel. The setting/state plumbing stays
// (so the dark-mode menu action remains a working toggle), but the actual
// qApp->setStyleSheet() call is now a no-op. Restore by reverting this
// function and uncommenting the file load below.
void MainWindow::applyStylesheet(bool darkMode)
{
    // 2026: no custom .qss is loaded — the app uses the native palette; dark mode
    // just records the preference and updates the status bar.
    m_darkModeEnabled = darkMode;
    m_settings.setDarkMode(darkMode);
    statusBar()->showMessage(
        darkMode ? tr("Dark Mode (native)") : tr("Light Mode (native)"),
        2000);
}

void MainWindow::toggleDarkMode()
{
    bool newMode = !m_darkModeEnabled;
    applyStylesheet(newMode);
    // Update checkbox state after toggle
    if (m_darkModeAction) {
        m_darkModeAction->setChecked(newMode);
    }
}

// Claude Generated 2026 - "Use Invocation Directory" preference
// Centralized logic used by the menu toggle, the dialog checkbox signal,
// and the constructor. Keeps a single source of truth for the
// "use the directory qurcuma was launched from" feature.
void MainWindow::applyUseInvocationDirectoryState(bool enabled)
{
    m_useInvocationDirectoryEnabled = enabled;
    m_settings.setUseInvocationDirectoryEnabled(enabled);

    if (m_useInvocationDirAction) {
        m_useInvocationDirAction->setChecked(enabled);
    }

    if (enabled) {
        // If we somehow lack a captured dir (e.g. toggled via dialog before
        // main.cpp populated it, or the dir was removed), fall back to the
        // process CWD right now. Without this branch the feature would
        // silently do nothing.
        if (m_invocationDir.isEmpty() || !QDir(m_invocationDir).exists()) {
            m_invocationDir = QDir::currentPath();
        }
        if (!m_invocationDir.isEmpty() && QDir(m_invocationDir).exists()) {
            switchWorkingDirectory(m_invocationDir);
        } else {
            QMessageBox::warning(this, tr("Invocation Directory"),
                tr("No valid invocation directory is available."));
            m_useInvocationDirectoryEnabled = false;
            m_settings.setUseInvocationDirectoryEnabled(false);
            if (m_useInvocationDirAction) {
                m_useInvocationDirAction->setChecked(false);
            }
        }
    } else {
        // Toggling OFF: restore the last-used working dir (or the settings
        // default if no last-used dir is recorded). User expects their
        // previous working state back when they uncheck.
        const QString lastDir = m_settings.lastUsedWorkingDirectory();
        if (!lastDir.isEmpty() && QDir(lastDir).exists()) {
            switchWorkingDirectory(lastDir);
        } else {
            const QString fallback = m_settings.workingDirectory();
            if (!fallback.isEmpty() && QDir(fallback).exists()) {
                switchWorkingDirectory(fallback);
            }
        }
    }
}

// Claude Generated 2026 - "Use Invocation Directory" preference
// Reads the desired new state from the menu action (or falls back to the
// in-memory mirror) and delegates to applyUseInvocationDirectoryState.
void MainWindow::toggleUseInvocationDirectory()
{
    const bool newState = !(m_useInvocationDirAction
                            ? m_useInvocationDirAction->isChecked()
                            : m_useInvocationDirectoryEnabled);
    applyUseInvocationDirectoryState(newState);
}

// Claude Generated - Quick Win: Drag & Drop support
void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();

    // Handle dropped files (directories or structure files)
    if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        for (const QUrl &url : urls) {
            QString path = url.toLocalFile();
            QFileInfo info(path);

            // If it's a directory, switch to it
            if (info.isDir()) {
                switchWorkingDirectory(path);
                addToRecentFiles(path);
                event->acceptProposedAction();
                return;
            }
            // If it's a structure file, load it
            else if (info.suffix() == "xyz" || info.suffix() == "vtf" || info.suffix() == "mol" || info.suffix() == "pdb") {
                QFile file(path);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    m_structureView->setPlainText(QString::fromUtf8(file.readAll()));
                    file.close();
                    statusBar()->showMessage(tr("Structure loaded from: %1").arg(info.fileName()), 3000);
                    event->acceptProposedAction();
                    return;
                }
            }
        }
    }
    // Handle dropped text (structure data)
    else if (mimeData->hasText()) {
        QString text = mimeData->text();
        if (!text.isEmpty()) {
            m_structureView->setPlainText(text);
            statusBar()->showMessage(tr("Structure pasted from drop"), 2000);
            event->acceptProposedAction();
        }
    }
}

// Claude Generated - Rendering mode shortcuts implementation
void MainWindow::setRenderingModeBallAndStick()
{
    if (!m_moleculeView) return;
    m_moleculeView->setRenderingMode(MoleculeViewer::RenderingMode::BallAndStick);
    statusBar()->showMessage(tr("Rendering Mode: Ball and Stick"), 1500);
    syncVisualizationDialog();  // Claude Generated - Fix: Sync dialog
}

void MainWindow::setRenderingModeSpaceFilling()
{
    if (!m_moleculeView) return;
    m_moleculeView->setRenderingMode(MoleculeViewer::RenderingMode::SpaceFilling);
    statusBar()->showMessage(tr("Rendering Mode: Space Filling"), 1500);
    syncVisualizationDialog();  // Claude Generated - Fix: Sync dialog
}

void MainWindow::setRenderingModeWireframe()
{
    if (!m_moleculeView) return;
    m_moleculeView->setRenderingMode(MoleculeViewer::RenderingMode::Wireframe);
    statusBar()->showMessage(tr("Rendering Mode: Wireframe"), 1500);
    syncVisualizationDialog();  // Claude Generated - Fix: Sync dialog
}

void MainWindow::setRenderingModeSticks()
{
    if (!m_moleculeView) return;
    m_moleculeView->setRenderingMode(MoleculeViewer::RenderingMode::SticksOnly);
    statusBar()->showMessage(tr("Rendering Mode: Sticks Only"), 1500);
    syncVisualizationDialog();  // Claude Generated - Fix: Sync dialog
}

// Claude Generated - Atom size adjustment shortcuts
void MainWindow::increaseAtomSize()
{
    if (!m_moleculeView) return;
    float currentScale = m_moleculeView->getAtomScaleFactor();
    float newScale = std::min(3.0f, currentScale + 0.1f);
    m_moleculeView->setAtomScaleFactor(newScale);
    statusBar()->showMessage(QString(tr("Atom Size: %1x")).arg(newScale, 0, 'f', 1), 1500);
    syncVisualizationDialog();  // Claude Generated - Fix: Sync dialog
}

void MainWindow::decreaseAtomSize()
{
    if (!m_moleculeView) return;
    float currentScale = m_moleculeView->getAtomScaleFactor();
    float newScale = std::max(0.1f, currentScale - 0.1f);
    m_moleculeView->setAtomScaleFactor(newScale);
    statusBar()->showMessage(QString(tr("Atom Size: %1x")).arg(newScale, 0, 'f', 1), 1500);
    syncVisualizationDialog();  // Claude Generated - Fix: Sync dialog
}

// Claude Generated - Bond thickness adjustment shortcuts
void MainWindow::increaseBondThickness()
{
    if (!m_moleculeView) return;
    float currentThickness = m_moleculeView->getBondThickness();
    float newThickness = std::min(0.5f, currentThickness + 0.02f);
    m_moleculeView->setBondThickness(newThickness);
    statusBar()->showMessage(QString(tr("Bond Thickness: %1")).arg(newThickness, 0, 'f', 2), 1500);
    syncVisualizationDialog();  // Claude Generated - Fix: Sync dialog
}

void MainWindow::decreaseBondThickness()
{
    if (!m_moleculeView) return;
    float currentThickness = m_moleculeView->getBondThickness();
    float newThickness = std::max(0.05f, currentThickness - 0.02f);
    m_moleculeView->setBondThickness(newThickness);
    statusBar()->showMessage(QString(tr("Bond Thickness: %1")).arg(newThickness, 0, 'f', 2), 1500);
    syncVisualizationDialog();  // Claude Generated - Fix: Sync dialog
}

// Claude Generated - Helper: Update visualization dialog when settings change via shortcuts
void MainWindow::syncVisualizationDialog()
{
    // Keep the Display dock in sync when settings change via shortcuts.
    if (m_displayPanel)
        m_displayPanel->syncFromViewer();
}

// Claude Generated - Focus & Centering Commands
void MainWindow::fitMoleculeInView()
{
    if (!m_moleculeView) return;
    m_moleculeView->fitAllInView();
    statusBar()->showMessage(tr("Fitted molecule in view"), 1500);
    // Claude Generated - Sync dialog if open
    syncVisualizationDialog();
}

void MainWindow::centerViewOnSelection()
{
    if (!m_moleculeView) return;
    auto selected = m_moleculeView->getSelectedAtoms();
    if (!selected.isEmpty()) {
        m_moleculeView->zoomToSelection(selected);
        statusBar()->showMessage(tr("Centered on selection"), 1500);
    }
    // Claude Generated - Sync dialog if open
    syncVisualizationDialog();
}

// Claude Generated 2026 - Move all frames so the mass-weighted COM = origin, reset camera.
void MainWindow::centerMoleculeAtOrigin()
{
    if (!m_moleculeView) return;
    m_moleculeView->centerAtOrigin();
    statusBar()->showMessage(tr("Molecule centered at origin"), 2000);
}

// Claude Generated - Phase 2A: Selection management commands
void MainWindow::selectAllAtoms()
{
    if (!m_moleculeView) return;
    // Claude Generated 2026 - Select every atom of the current frame. The old version only
    // filled the range 0..max(already selected), so Ctrl+A did nothing on an empty selection.
    const int count = m_moleculeView->getCurrentFrameAtoms().size();
    if (count == 0) return;
    QVector<int> all(count);
    for (int i = 0; i < count; ++i)
        all[i] = i;
    m_moleculeView->selectAtoms(all);
    statusBar()->showMessage(tr("Selected all %1 atoms").arg(count), 1500);
}

void MainWindow::clearAtomSelection()
{
    if (!m_moleculeView) return;
    m_moleculeView->clearSelection();
    statusBar()->showMessage(tr("Selection cleared"), 1500);
}

// Claude Generated Phase 4.3-4.5 - Workspace management stubs (to be implemented)

void MainWindow::updateWorkspaceList()
{
    // Populate workspace list from WorkspaceManager
    if (!m_workspaceListView) return;
    if (!m_workspaceManager) return;

    m_workspaceListView->clear();
    auto workspaces = m_workspaceManager->listWorkspaces();

    for (const auto& ws : workspaces) {
        QListWidgetItem* item = new QListWidgetItem(ws.name);
        item->setData(Qt::UserRole, ws.id);
        m_workspaceListView->addItem(item);
    }
}

void MainWindow::onWorkspaceItemClicked(QListWidgetItem* item)
{
    if (!item || !m_workspaceManager) return;
    QString id = item->data(Qt::UserRole).toString();
    Settings::Workspace ws = m_workspaceManager->getWorkspace(id);
    if (ws.isValid()) {
        restoreWorkspaceState(ws);
    }
}

void MainWindow::onWorkspaceContextMenu(const QPoint& pos)
{
    // Claude Generated Phase 4.5 - Workspace context menu
    QListWidgetItem* item = m_workspaceListView->itemAt(pos);
    if (!item || !m_workspaceManager) return;

    QString wsId = item->data(Qt::UserRole).toString();
    Settings::Workspace ws = m_workspaceManager->getWorkspace(wsId);
    if (!ws.isValid()) return;

    QMenu contextMenu(this);

    // Load Workspace
    QAction* loadAction = contextMenu.addAction(tr("Load Workspace"));
    connect(loadAction, &QAction::triggered, [this, ws]() {
        restoreWorkspaceState(ws);
    });

    contextMenu.addSeparator();

    // Rename Workspace
    QAction* renameAction = contextMenu.addAction(tr("Rename..."));
    connect(renameAction, &QAction::triggered, [this, wsId, item]() {
        QString newName = QInputDialog::getText(this,
            tr("Rename Workspace"),
            tr("New name:"),
            QLineEdit::Normal,
            item->text());

        if (!newName.isEmpty() && newName != item->text()) {
            m_workspaceManager->renameWorkspace(wsId, newName);
            updateWorkspaceList();
            statusBar()->showMessage(tr("Workspace renamed to '%1'").arg(newName), 2000);
        }
    });

    // Edit Description
    QAction* descAction = contextMenu.addAction(tr("Edit Description..."));
    connect(descAction, &QAction::triggered, [this, wsId]() {
        auto workspaces = m_workspaceManager->listWorkspaces();
        Settings::Workspace targetWs;
        for (const auto& w : workspaces) {
            if (w.id == wsId) {
                targetWs = w;
                break;
            }
        }

        QString newDesc = QInputDialog::getText(this,
            tr("Edit Workspace Description"),
            tr("Description:"),
            QLineEdit::Normal,
            targetWs.description);

        if (!newDesc.isEmpty() || !targetWs.description.isEmpty()) {
            targetWs.description = newDesc;
            m_workspaceManager->saveWorkspace(targetWs);
            statusBar()->showMessage(tr("Workspace description updated"), 2000);
        }
    });

    contextMenu.addSeparator();

    // Delete Workspace
    QAction* deleteAction = contextMenu.addAction(tr("Delete..."));
    deleteAction->setIcon(QIcon::fromTheme("edit-delete"));
    connect(deleteAction, &QAction::triggered, [this, wsId, item]() {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            tr("Delete Workspace"),
            tr("Are you sure you want to delete workspace '%1'?").arg(item->text()),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            m_workspaceManager->deleteWorkspace(wsId);
            updateWorkspaceList();
            statusBar()->showMessage(tr("Workspace deleted"), 2000);
        }
    });

    contextMenu.exec(m_workspaceListView->viewport()->mapToGlobal(pos));
}

void MainWindow::saveCurrentWorkspace()
{
    // Claude Generated Phase 4.4 - Capture and save workspace
    QString name = QInputDialog::getText(this, tr("Save Workspace"), tr("Workspace name:"));
    if (name.isEmpty() || !m_workspaceManager) return;

    QString desc = QInputDialog::getText(this, tr("Workspace Description"), tr("Description (optional):"));

    // Create workspace with current state
    Settings::Workspace ws;
    ws.id = QUuid::createUuid().toString();
    ws.name = name;
    ws.description = desc;
    ws.workingDirectory = m_workingDirectory;
    ws.openCalculations = m_currentCalculationDir.isEmpty() ? QStringList() : QStringList() << m_currentCalculationDir;
    ws.windowGeometry = saveGeometry();
    ws.dockState = saveState();  // Claude Generated - UI Restructuring: Save dock widget layout
    ws.created = QDateTime::currentDateTime();
    ws.lastUsed = QDateTime::currentDateTime();

    m_workspaceManager->saveWorkspace(ws);
    updateWorkspaceList();
    statusBar()->showMessage(tr("Workspace '%1' saved").arg(name), 3000);
}

void MainWindow::restoreWorkspaceState(const Settings::Workspace& ws)
{
    if (!ws.isValid()) return;

    if (!ws.workingDirectory.isEmpty()) {
        switchWorkingDirectory(ws.workingDirectory);
        if (m_projectDock)
            m_projectDock->setCurrentSegment(ProjectDock::ProjectSegment::Files);
    }

    if (!ws.windowGeometry.isEmpty()) {
        restoreGeometry(ws.windowGeometry);
    }

    // Claude Generated - UI Restructuring: Restore dock widget layout
    if (!ws.dockState.isEmpty()) {
        restoreState(ws.dockState);
    } else {
        // No layout stored with the workspace: lay out the current mode.
        setAppMode(m_appMode);
    }

    if (m_workspaceManager) {
        m_workspaceManager->updateWorkspaceLastUsed(ws.id);
    }

    statusBar()->showMessage(tr("Workspace '%1' restored").arg(ws.name), 3000);
}

// Claude Generated - SFTP: Load molecule file from local or remote path
void MainWindow::loadMoleculeFile(const QString& filePath)
{
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        qWarning() << "File does not exist:" << filePath;
        return;
    }

    // Claude Generated 2026 - Suppress the structure-sync text mirror during a file
    // load: loadMoleculeFile sets m_structureView to the file's own text (which may
    // be VTF, not XYZ), and the setTrajectoryData below emits moleculeUpdated. The
    // guard keeps the loaded file text intact; edits after load start the mirroring.
    m_structSyncing = true;
    auto syncGuard = qScopeGuard([this]() { m_structSyncing = false; });

    // Claude Generated 2026 - If the user has modified the structure (e.g. by
    // running an MD/Opt), prompt before discarding those changes. Save runs
    // through the central saveStructure() helper which itself may show a
    // Save-As dialog; if the user cancels that, the load is aborted.
    if (m_structureModified) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(tr("Unsaved changes"));
        box.setText(tr("The current structure has unsaved changes from a "
                       "previous MD/optimization run."));
        box.setInformativeText(tr("Save them before opening a new file?"));
        QPushButton* saveBtn    = box.addButton(tr("Save..."),  QMessageBox::AcceptRole);
        QPushButton* discardBtn = box.addButton(tr("Discard"),  QMessageBox::DestructiveRole);
        QPushButton* cancelBtn  = box.addButton(tr("Cancel"),   QMessageBox::RejectRole);
        box.setDefaultButton(saveBtn);
        box.exec();
        QAbstractButton* clicked = box.clickedButton();
        if (clicked == cancelBtn)
            return;
        if (clicked == saveBtn && !saveCurrentStructure())
            return;  // user cancelled Save-As — keep the unsaved structure
        // Discard falls through silently.
    }

    QString suffix = QFileInfo(filePath).suffix().toLower();
    QString basename = QFileInfo(filePath).baseName();

    // Claude Generated 2026 - tracks whether at least one parser successfully
    // loaded the file. Only used at the end to decide whether to auto-switch
    // the working directory to the file's parent directory.
    bool fileLoaded = false;

    {
        // Claude Generated 2026 - Every format the shared MoleculeFileLoader parses
        // (xyz, vtf, pdb, mol2) loads through this one path, so snapshots, save path,
        // recent files and the simulation dock are synced for all of them. Save on a
        // non-xyz source asks for a new .xyz file (saveStructure) instead of
        // overwriting the original.
        const MoleculeFileLoader::Result r = MoleculeFileLoader::load(filePath);
        if (!r.supported) {
            statusBar()->showMessage(tr("Unsupported file format: %1").arg(suffix), 2000);
        } else if (r.ok) {
            // Load the raw file contents into the structure editor.
            QFile file(filePath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                m_structureView->setPlainText(QString::fromUtf8(file.readAll()));
                m_structureFileEdit->setText(QFileInfo(filePath).fileName());
                file.close();
            }
            m_moleculeView->setFrameCount(r.frameCount());
            m_moleculeView->clearScenePublic();
            m_moleculeView->setTrajectoryData(r.frames, r.frameBonds);
            if (m_centerOnLoad) m_moleculeView->centerAtOrigin();

            // Feed the first frame into the inline simulation widget so the user
            // can start a run without re-selecting the molecule.
            if (m_simulationControlWidget)
                m_simulationControlWidget->setMolecule(m_moleculeView->getCurrentFrameAtoms(),
                    m_moleculeView->getCurrentFrameBonds());
            // Fresh load: clear the modified flag, cache the source path, enable Save.
            m_currentMoleculeFilePath = filePath;
            m_structureModified = false;
            if (m_simulationControlWidget)
                m_simulationControlWidget->setStructureModified(false);
            if (m_saveAction) m_saveAction->setEnabled(true);
            if (m_saveAsAction) m_saveAsAction->setEnabled(true);
            // Snapshot 0 = original geometry (seeds in-dock Reset + history list).
            captureInitialSnapshot(filePath, m_moleculeView->getCurrentFrameAtoms(),
                m_moleculeView->getCurrentFrameBonds());
            fileLoaded = true;
        } else {
            m_moleculeView->clearScenePublic();
            qWarning() << "Failed to parse file:" << filePath;
        }
    }

    // Claude Generated 2026 - Loading a structure is a pure viewer operation and
    // must NOT change the Working Directory. The Working Directory is a stable
    // anchor the user sets deliberately (Choose Directory, "Set as Working
    // Directory", breadcrumb, recent dirs, workspace, CLI <dir>); clicking around
    // and opening structures should never move it.
    if (fileLoaded) {
        if (m_statusFileLabel)
            m_statusFileLabel->setText(QFileInfo(filePath).fileName());
        // A fresh molecule resets the scene (clears any RMSD overlays); reset the RMSD
        // workspace too so it does not keep stale aligned structures.
        if (m_rmsdWidget)
            m_rmsdWidget->clearWorkspace();
        // Claude Generated 2026 - If this file belongs to an unpacked lesson (a
        // lesson.json sidecar in its directory references it), restore the stored
        // simulation conditions into the dock. No-op for ordinary files.
        m_lessonController->applyConditions(filePath);
    }
}

#ifdef USE_SFTP
// Claude Generated - Phase SFTP Integration: Recent remote connections menu management
void MainWindow::updateRecentConnectionsMenu()
{
    m_recentConnectionsMenu->clear();

    Settings settings;
    QVector<Settings::SftpConnectionProfile> recentConnections = settings.getRecentSftpConnections(5);

    if (recentConnections.isEmpty()) {
        m_recentConnectionsMenu->setEnabled(false);
        return;
    }

    m_recentConnectionsMenu->setEnabled(true);

    for (const auto& connection : recentConnections) {
        QString displayText = QString("%1 (%2@%3)")
            .arg(connection.name)
            .arg(connection.username)
            .arg(connection.host);

        // Show last used time
        QString timeAgo;
        qint64 secondsAgo = connection.lastUsed.secsTo(QDateTime::currentDateTime());
        if (secondsAgo < 3600) {
            timeAgo = tr("%1 minutes ago").arg(secondsAgo / 60);
        } else if (secondsAgo < 86400) {
            timeAgo = tr("%1 hours ago").arg(secondsAgo / 3600);
        } else {
            timeAgo = tr("%1 days ago").arg(secondsAgo / 86400);
        }

        QAction* action = m_recentConnectionsMenu->addAction(
            QIcon::fromTheme("network-server"),
            QString("%1 - %2").arg(displayText, timeAgo)
        );

        action->setData(connection.id);
        connect(action, &QAction::triggered, this, [this, connection]() {
            openRecentConnection(connection.id);
        });
    }

    m_recentConnectionsMenu->addSeparator();
    QAction* clearAction = m_recentConnectionsMenu->addAction(tr("Clear Recent Connections"));
    connect(clearAction, &QAction::triggered, this, [this]() {
        Settings settings;
        settings.setSftpProfiles({});  // Clear all profiles
        updateRecentConnectionsMenu();
    });
}

void MainWindow::openRecentConnection(const QString& profileId)
{
    // This would be better with profile pre-selection in SftpDialog
    // For now, just open the dialog
    SftpDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString localPath = dialog.getLocalPath();
        if (!localPath.isEmpty()) {
            loadMoleculeFile(localPath);
            statusBar()->showMessage(tr("Loaded remote file: %1").arg(QFileInfo(localPath).fileName()), 3000);
            updateRecentConnectionsMenu();
        }
    }
}

// Claude Generated - Remote Directory Mounting
void MainWindow::onAddRemoteDirectoryClicked()
{
    SftpDialog dialog(this);
    dialog.setMode(SftpDialog::Mode::SelectDirectory);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString remotePath = dialog.getSelectedDirectory();
    QString profileId = dialog.getSelectedProfileId();

    if (remotePath.isEmpty() || profileId.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Selection"), tr("Could not get directory path or profile."));
        return;
    }

    // Ask for name
    bool ok;
    QString name = QInputDialog::getText(this, tr("Remote Directory Name"),
        tr("Name for this remote directory:"), QLineEdit::Normal,
        remotePath, &ok);
    if (!ok || name.isEmpty()) return;

    // Save mount
    Settings::RemoteMountPoint mount;
    mount.id = QUuid::createUuid().toString();
    mount.name = name;
    mount.profileId = profileId;
    mount.remotePath = remotePath;
    mount.mounted = QDateTime::currentDateTime();
    mount.lastAccessed = QDateTime::currentDateTime();

    m_settings.addRemoteMount(mount);
    updateRemoteDirectoriesView();
    statusBar()->showMessage(tr("Remote directory added: %1").arg(name), 3000);
}

void MainWindow::updateRemoteDirectoriesView()
{
    m_remoteDirectoriesView->clear();
    Settings settings;
    auto mounts = settings.remoteMounts();

    for (const auto& mount : mounts) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_remoteDirectoriesView);
        item->setText(0, QString("📡 %1").arg(mount.name));
        item->setData(0, Qt::UserRole, mount.id);
        item->setToolTip(0, mount.remotePath);
    }
}

void MainWindow::onRemoteDirectoryClicked(QTreeWidgetItem* item, int column)
{
    if (!item) return;
    QString mountId = item->data(0, Qt::UserRole).toString();
    if (mountId.isEmpty()) return;

    Settings settings;
    auto mounts = settings.remoteMounts();
    Settings::RemoteMountPoint mount;
    for (const auto& m : mounts) {
        if (m.id == mountId) {
            mount = m;
            break;
        }
    }
    if (!mount.isValid()) return;

    auto profiles = settings.sftpProfiles();
    Settings::SftpConnectionProfile profile;
    for (const auto& p : profiles) {
        if (p.id == mount.profileId) {
            profile = p;
            break;
        }
    }
    if (!profile.isValid()) {
        QMessageBox::warning(this, tr("Error"), tr("Connection profile not found."));
        return;
    }

    QString password = QInputDialog::getText(this, tr("Password"),
        tr("Password for %1@%2:").arg(profile.username, profile.host),
        QLineEdit::Password);
    if (password.isEmpty()) return;

    // Create/get SFTP model
    if (!m_remoteSftpModels.contains(mountId)) {
        m_remoteSftpModels[mountId] = new SftpItemModel(profile.host, profile.username, password, profile.port, this);
        if (profile.useKeyAuth) {
            m_remoteSftpModels[mountId]->setUseKeyAuth(true);
        }
    }

    SftpItemModel* model = m_remoteSftpModels[mountId];
    if (!model->isConnected()) {
        QMessageBox::critical(this, tr("Connection Failed"), tr("Could not connect to server."));
        return;
    }

    m_directoryContentView->setModel(model);
    m_currentRemoteMountId = mountId;
    settings.updateRemoteMountLastAccessed(mountId);
    updateRemoteDirectoriesView();
    statusBar()->showMessage(tr("Browsing: %1 (%2)").arg(mount.name, mount.remotePath));
}

// Claude Generated - Remote File Double-Click Handler
void MainWindow::onRemoteFileDoubleClicked(const QModelIndex& index)
{
    // Check if we have an active SFTP model
    if (m_currentRemoteMountId.isEmpty()) return;
    if (!m_remoteSftpModels.contains(m_currentRemoteMountId)) return;

    SftpItemModel* model = m_remoteSftpModels[m_currentRemoteMountId];
    if (!model || !model->isConnected()) return;

    // Get the file path from the SFTP model
    QString filePath = model->getItemPath(index);
    if (filePath.isEmpty()) return;

    // Check if it's a directory
    if (model->isDirectory(index)) {
        // For directories, we would need to navigate, but SftpItemModel
        // doesn't support cd() yet. This is for file downloads.
        statusBar()->showMessage(tr("Directory navigation not yet supported for SFTP."));
        return;
    }

    // Download and load the file
    downloadAndLoadRemoteFile(filePath);
}

// Claude Generated - Download Remote File and Load into Viewer
void MainWindow::downloadAndLoadRemoteFile(const QString& filePath)
{
    if (m_currentRemoteMountId.isEmpty()) return;
    if (!m_remoteSftpModels.contains(m_currentRemoteMountId)) return;

    SftpItemModel* model = m_remoteSftpModels[m_currentRemoteMountId];
    if (!model) return;

    // Extract filename from path
    QString fileName = filePath.split("/").last();
    if (fileName.isEmpty()) return;

    // Create cache directory in system temp
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/qurcuma_remote/";
    QDir().mkpath(cacheDir);

    // Download file to cache
    QString localPath = cacheDir + fileName;
    statusBar()->showMessage(tr("Downloading: %1...").arg(fileName));

    if (!model->downloadFile(filePath, localPath)) {
        QMessageBox::critical(this, tr("Download Failed"),
            tr("Failed to download file: %1").arg(fileName));
        return;
    }

    // Load the downloaded file (first frame) into the viewer via the shared loader.
    const MoleculeFileLoader::Result r = MoleculeFileLoader::load(localPath);
    if (!r.supported) {
        QMessageBox::warning(this, tr("Unsupported Format"),
            tr("File format not supported: %1").arg(filePath));
        return;
    }
    if (r.ok) {
        m_moleculeView->setFrameCount(r.frameCount());
        m_moleculeView->addMolecule(r.frames.first(), r.frameBonds.first());
    } else if (!r.error.isEmpty()) {
        // pdb/mol2 surface a parse error; xyz/vtf fail silently (as before).
        QMessageBox::warning(this, tr("Error"), tr("Failed to parse file: %1").arg(r.error));
    }

    statusBar()->showMessage(tr("Loaded: %1 (from %2)").arg(fileName, filePath));
}
#endif // USE_SFTP

// Claude Generated (2026-04) - Dock architecture rewrite. Five focused docks rahmen
// the MoleculeViewer (CentralWidget). Internal QTabWidgets replace fragile Qt
// tabifyDockWidget chains for editors and atoms/simulation, eliminating the
// drift bug where preset switches stacked redundant tab groupings.
void MainWindow::createDockWidgets()
{
    // Claude Generated 2026 - Dock system restructuring: Phase 4. Create all
    // wrapped docks via the DockManager and pull their internal widgets into
    // MainWindow members so existing logic keeps working during the migration.
    m_dockManager->initialize(m_moleculeView, &m_settings);
    m_outputViewDock = m_dockManager->outputDockImpl();
    m_structureDock = m_dockManager->structureDockImpl();
    m_simulationDock = m_dockManager->simulationDockImpl();
    // Pull the wrapped internal widgets into MainWindow members so the rest of the
    // code can keep using them during the migration.
    if (m_structureDock) {
        m_structureView = m_structureDock->structureView();
        m_structureFileEdit = m_structureDock->structureFileEdit();
        m_structureFileEditExtension = m_structureDock->structureFileEditExtension();
        m_atomListPanel = m_structureDock->atomListPanel();
    }
    m_appearanceDock = m_dockManager->appearanceDockImpl();  // Claude Generated 2026 - UX stage 4
    if (m_appearanceDock)
        m_displayPanel = m_appearanceDock->displayPanel();
    if (m_simulationDock) {
        m_simulationTabs = m_simulationDock->tabs();
        // Claude Generated 2026 - UX stage 4: walls, wall potential, grab force vectors and
        // dynamic bonds are shown/hidden from the Simulation dock ("Show in viewer").
        m_simulationDock->setViewOptions(new SimulationViewOptions(m_moleculeView));
        m_inputView = m_simulationDock->inputView();
        m_inputFileEdit = m_simulationDock->inputFileEdit();
        m_inputFileEditExtension = m_simulationDock->inputFileEditExtension();
        m_simulationControlWidget = m_simulationDock->simulationControlWidget();
        m_snapshotsWidget = m_simulationDock->snapshotsWidget();
        m_rmsdWidget = m_simulationDock->rmsdWidget();
    }

    // Image-gallery dock (bottom, hidden until the first export): collect exported
    // images and batch-trim their identical whitespace border. Claude Generated 2026.
    m_imageGalleryDock = m_dockManager->imageGalleryDockImpl();
    if (m_imageGalleryDock) {
        m_imageGalleryDock->setWorkingDirectory(m_workingDirectory);
        if (m_moleculeView)
            connect(m_moleculeView, &MoleculeViewer::imageExported,
                m_imageGalleryDock, &ImageGalleryDock::addExportedImage);
    }

    // Claude Generated 2026 - Non-covalent interaction dock (right, tabified with
    // Display, hidden until the overlay is switched on). The table lists the
    // contacts the 3D overlay draws; selecting a row highlights its atoms.
    m_nciDock = m_dockManager->nciDockImpl();
    setupNciAnalysis();

    // Viewer-bar "Photo" button → dialog-free quick export into the working folder.
    if (m_moleculeView) {
        connect(m_moleculeView, &MoleculeViewer::quickExportRequested,
            this, &MainWindow::quickExportPhoto);
        // Claude Generated 2026 - Right-click (no drag) on the 3D view.
        connect(m_moleculeView, &MoleculeViewer::contextMenuRequested,
            this, &MainWindow::showViewportContextMenu);
        // Claude Generated 2026 - Builder "Clean up": snapshot, then a bounded
        // optimization through the existing simulation worker lifecycle.
        connect(m_moleculeView, &MoleculeViewer::newSceneRequested,
            this, &MainWindow::newScene);
        connect(m_moleculeView, &MoleculeViewer::cleanupRequested, this, [this]() {
            if (!m_simulationControlWidget)
                return;
            takeSnapshot(tr("Before cleanup"));
            m_simulationControlWidget->startQuickOptimization(50);
        });
    }

    // ==================== PROJECT DOCK (left) ====================
    // Phase 6 redesign: ProjectDock owns a segmented upper panel
    // (Files / Bookmarks / Workspaces / Remote) plus a lower file browser.
    // NavigationDock no longer exists; MainWindow wires the panel widgets directly.
    m_projectDock = m_dockManager->projectDockImpl();

    if (m_projectDock) {
        m_chooseDirectory = m_projectDock->chooseDirectoryButton();
        m_breadcrumbBar = m_projectDock->breadcrumbBar();
        m_projectListView = m_projectDock->projectListView();
        m_projectModel = m_projectDock->projectModel();
        m_newCalculationButton = m_projectDock->newCalculationButton();
        m_currentProjectLabel = m_projectDock->currentProjectLabel();
        m_stateIcon = m_projectDock->stateIcon();
        m_stateIndicator = m_projectDock->stateIndicator();
        m_filesModeBtn = m_projectDock->filesModeButton();
        m_lessonModeBtn = m_projectDock->lessonModeButton();
        m_directoryContentView = m_projectDock->directoryContentView();
        m_directoryContentModel = m_projectDock->directoryContentModel();
        m_directoryContentProxyModel = m_projectDock->directoryContentProxyModel();
        // Claude Generated 2026 - WP T4: the lesson feature is owned by LessonController.
        // Inject the collaborators harvested from the docks (m_simulationControlWidget is
        // already set by the simulation-dock block above), then let the controller wire its
        // own metadata/detail editors + build the in-memory structure model.
        m_lessonController = new LessonController(this, this);
        m_lessonController->setViewer(m_moleculeView);
        m_lessonController->setSimulationWidget(m_simulationControlWidget);
        m_lessonController->setDockManager(m_dockManager);
        m_lessonController->setContentView(m_directoryContentView,
            m_directoryContentProxyModel
                ? static_cast<QAbstractItemModel*>(m_directoryContentProxyModel)
                : static_cast<QAbstractItemModel*>(m_directoryContentModel));
        m_lessonController->setModeButtons(m_filesModeBtn, m_lessonModeBtn);
        m_lessonController->setMetaWidgets(m_projectDock->lessonMetaWidget(),
            m_projectDock->lessonTitleEdit(), m_projectDock->lessonDescEdit(),
            m_projectDock->lessonAuthorsLabel());
        m_lessonController->setStructWidgets(m_projectDock->lessonStructWidget(),
            m_projectDock->structNameEdit(), m_projectDock->structDescEdit(),
            m_projectDock->structRoleCombo());
        m_lessonController->setWorkingDirectory(m_workingDirectory);
        m_lessonController->setCenterOnLoad(m_centerOnLoad);
        m_lessonController->wireWidgetConnections();
        connect(m_lessonController, &LessonController::workingDirectoryChangeRequested,
                this, &MainWindow::switchWorkingDirectory);
        connect(m_lessonController, &LessonController::windowTitleChangeRequested,
                this, &QWidget::setWindowTitle);
        connect(m_lessonController, &LessonController::directoryContentRefreshRequested,
                this, &MainWindow::updateDirectoryContent);
        connect(m_lessonController, &LessonController::statusMessage, this,
                [this](const QString& msg, int t) { statusBar()->showMessage(msg, t); });
        connect(m_lessonController, &LessonController::inMemoryStructureLoaded,
                this, &MainWindow::onLessonStructureLoaded);

        if (auto* bw = m_projectDock->bookmarkWidget())
            m_bookmarkTreeView = bw->treeView();
        if (auto* wp = m_projectDock->workspacePanel())
            m_workspaceListView = wp->listView();
#ifdef USE_SFTP
        if (auto* rp = m_projectDock->remotePanel())
            m_remoteDirectoriesView = rp->treeView();
#endif

        // Breadcrumb navigation
        connect(m_breadcrumbBar, &BreadcrumbBar::pathSelected,
                this, &MainWindow::switchWorkingDirectory);

        // Copy current calculation path
        connect(m_projectDock->copyPathButton(), &QPushButton::clicked,
                this, &MainWindow::copyCurrentPath);

        // Files / Lesson browser toggle
        connect(m_filesModeBtn, &QToolButton::clicked,
                this, [this]() { m_lessonController->setBrowserMode(false); });
        connect(m_lessonModeBtn, &QToolButton::clicked,
                this, [this]() { m_lessonController->setBrowserMode(true); });

        // Content view remote-file handling
#ifdef USE_SFTP
        connect(m_directoryContentView, &QListView::doubleClicked,
                this, &MainWindow::onRemoteFileDoubleClicked);
#endif

        // Lesson: the "Authors/License…" button opens the metadata dialog. The inline
        // title/desc/detail editors + the in-memory structure model are wired inside the
        // controller (wireWidgetConnections above).
        connect(m_projectDock->editAuthorsButton(), &QToolButton::clicked,
                this, [this]() { m_lessonController->editMetadata(); });

        // Bookmark / workspace / remote panel signals
        connect(m_projectDock, &ProjectDock::bookmarkDirectorySelected,
                this, &MainWindow::switchWorkingDirectory);
        connect(m_projectDock, &ProjectDock::saveWorkspaceRequested,
                this, &MainWindow::saveCurrentWorkspace);
#ifdef USE_SFTP
        connect(m_projectDock, &ProjectDock::addRemoteRequested,
                this, &MainWindow::onAddRemoteDirectoryClicked);
#endif
    }

    // ==================== STRUCTURE & DISPLAY DOCK (right) ====================
    // Phase 8: dock with [Structure | Atoms] segment toggle on top and Display panel below.
    if (m_structureDock) {
        // "Apply → Viewer" button lives inside the wrapper now.
        connect(m_structureDock, &StructureDock::structureApplyRequested,
                this, &MainWindow::applyStructureTextToViewer);
    }

    // ==================== SIMULATION DOCK (right) ====================
    // Phase 8: dock with Simulation/Snapshots/RMSD/Input tabs, tabified with Structure.
    if (m_simulationDock) {
        // RMSD / align workspace signals. The widget owns a table of structures (one is
        // the reference, the rest are aligned overlays) and drives the viewer through a
        // full-rebuild signal + cheap per-overlay live edits.
        if (m_rmsdWidget) {
            connect(m_rmsdWidget, &RMSDWidget::overlayWorkspaceChanged, this,
                [this](const QVector<MoleculeViewer::Atom>& refAtoms,
                    const QVector<MoleculeViewer::Bond>& refBonds, bool refVisible,
                    const QColor& refTint, const QVector<MoleculeViewer::OverlaySpec>& overlays,
                    bool resetView) {
                    if (m_moleculeView)
                        m_moleculeView->setOverlayWorkspace(refAtoms, refBonds, refVisible,
                            refTint, overlays, resetView);
                });
            connect(m_rmsdWidget, &RMSDWidget::overlayTintChanged, this,
                [this](int i, const QColor& c) {
                    if (m_moleculeView)
                        m_moleculeView->setOverlayTint(i, c);
                });
            connect(m_rmsdWidget, &RMSDWidget::overlaySizeChanged, this,
                [this](int i, float s) {
                    if (m_moleculeView)
                        m_moleculeView->setOverlaySize(i, s);
                });
            connect(m_rmsdWidget, &RMSDWidget::overlayVisibilityChanged, this,
                [this](int i, bool v) {
                    if (m_moleculeView)
                        m_moleculeView->setOverlayVisible(i, v);
                });
            connect(m_rmsdWidget, &RMSDWidget::referenceVisibilityChanged, this,
                [this](bool v) {
                    if (m_moleculeView)
                        m_moleculeView->setPrimaryVisible(v);
                });
            connect(m_rmsdWidget, &RMSDWidget::referenceTintChanged, this,
                [this](const QColor& c) {
                    if (m_moleculeView)
                        m_moleculeView->setPrimaryTint(c);
                });
            // Direct feedback when a structure is aligned + added to the workspace.
            connect(m_rmsdWidget, &RMSDWidget::structureAligned, this,
                [this](const QString& name, double rmsd) {
                    statusBar()->showMessage(
                        tr("Added '%1' to the RMSD workspace — RMSD %2 Å")
                            .arg(name).arg(rmsd, 0, 'f', 3),
                        5000);
                });
            connect(m_rmsdWidget, &RMSDWidget::seedReferenceRequested,
                    this, &MainWindow::seedRMSDReference);
        }
    }

    // ==================== DISPLAY PANEL (inside the Appearance dock) ====================
    // DisplayPanel is owned by the Appearance dock and harvested above; MainWindow only
    // wires its signals here.
    // The viewer's slim "Display ⚙" bar button surfaces this dock.
    if (m_moleculeView)
        connect(m_moleculeView, &MoleculeViewer::displayOptionsRequested,
            this, &MainWindow::openVisualizationSettings);

    // Claude Generated - Worker is wired to view + status slot directly (skips widget mid-hop).
    // Claude Generated 2026 - Phase 6: every molecule load path emits MoleculeViewer::moleculeUpdated;
    // centralising the sim-dock sync here replaces a dozen ad-hoc setMolecule callsites.
    // Claude Generated 2026 - The sim path (MoleculeViewer::updateSimulationFrame)
    // also emits moleculeUpdated (throttled to once per worker run) so the sim
    // dock's m_atoms cache stays in lockstep with the viewer. We mark the
    // structure as modified here too, which surfaces the "● Modified" hint and
    // enables the save button / File>Save action.
    if (m_moleculeView) {
        connect(m_moleculeView, &MoleculeViewer::moleculeUpdated,
            m_simulationControlWidget,
            [this](const QVector<MoleculeViewer::Atom>& atoms,
                const QVector<MoleculeViewer::Bond>& bonds) {
                if (m_simulationControlWidget)
                    m_simulationControlWidget->setMolecule(atoms, bonds);
                m_structureModified = true;
                if (m_simulationControlWidget)
                    m_simulationControlWidget->setStructureModified(true);
            });
    }
    // Claude Generated 2026 - Structure editing: snapshot the pre-edit geometry before a
    // move/paste/merge/delete so the Snapshots tab doubles as undo for those edits.
    if (m_moleculeView)
        connect(m_moleculeView, &MoleculeViewer::editSnapshotRequested, this,
            [this](const QString& label) { takeSnapshot(label); });
    connect(m_simulationControlWidget, &SimulationControlWidget::workerStarted,
        this, &MainWindow::wireSimulationWorker);
    connect(m_simulationControlWidget, &SimulationControlWidget::configChanged,
        this, &MainWindow::onSimulationConfigChanged);
    // Claude Generated 2026 - In-dock "Save" button routes to the central save.
    connect(m_simulationControlWidget, &SimulationControlWidget::saveStructureRequested,
        this, [this]() { saveCurrentStructure(); });
    // Claude Generated 2026 - In-dock "Reset" button restores the original geometry.
    // Always calls resetToOriginalSnapshot() which handles the empty-snapshots
    // fallback and correctly clears the modified flag (unlike restoreSnapshot).
    connect(m_simulationControlWidget, &SimulationControlWidget::resetStructureRequested,
        this, [this](int /*index*/) {
            resetToOriginalSnapshot();
        });

    // Claude Generated 2026 - Live wall-boundary feedback: viewer counts atoms
    // outside the configured wall region (per frame / live MD) and the dock shows
    // a "N atoms outside" status; the 3D wireframe also turns red.
    connect(m_moleculeView, &MoleculeViewer::wallViolationChanged,
        m_simulationControlWidget, &SimulationControlWidget::setWallViolationCount);

    // Claude Generated 2026 - Snapshot widget controls.
    connect(m_simulationDock, &SimulationDock::takeSnapshotRequested,
        this, [this]() {
            takeSnapshot();
        });
    connect(m_simulationDock, &SimulationDock::restoreSnapshotRequested,
        this, [this](int index) {
            if (index >= 0 && index < m_snapshots.size())
                restoreSnapshot(m_snapshots[index]);
        });
    // Claude Generated 2026 - Protect snapshot 0 (original geometry) from deletion.
    // Deleting it would break the Reset-to-original invariant.
    connect(m_simulationDock, &SimulationDock::deleteSnapshotRequested,
        this, [this](int index) {
            if (index == 0)
                return;  // Original snapshot must not be deleted
            if (index > 0 && index < m_snapshots.size()) {
                m_snapshots.removeAt(index);
                if (m_simulationControlWidget)
                    m_simulationControlWidget->setResetEnabled(!m_snapshots.isEmpty());
            }
        });
    // Claude Generated 2026 - Phase 6: keep pickers ON during sim so click+drag
    // on an atom triggers the grab path (QObjectPicker::pressed). Without this
    // the camera controller would eat the press and rotate instead.
    connect(m_simulationControlWidget, &SimulationControlWidget::simulationRunningChanged,
        this, [this](bool running) {
            if (!m_moleculeView) return;
            // Ensure pickers exist for grab (creates them if instancing skipped them)
            if (running)
                m_moleculeView->ensurePickersForGrab();
            m_moleculeView->setPickingActive(true);
            m_moleculeView->setSimulationActive(running);
            if (running && m_simulationControlWidget) {
                m_moleculeView->setGrabStrength(m_simulationControlWidget->grabStrength());
                m_moleculeView->setGrabAlpha(m_simulationControlWidget->grabAlpha());
                m_moleculeView->setGrabMaxShells(m_simulationControlWidget->grabMaxShells());
            }
        });
    // Claude Generated 2026 - Phase 6: push live grab-slider changes into the viewer.
    connect(m_simulationControlWidget, &SimulationControlWidget::grabSettingsChanged,
        this, [this](double strength, double alpha, int maxShells) {
            if (!m_moleculeView) return;
            m_moleculeView->setGrabStrength(strength);
            m_moleculeView->setGrabAlpha(alpha);
            m_moleculeView->setGrabMaxShells(maxShells);
        });

    // ==================== OUTPUT DOCK (bottom) ====================
    // The dock owns the log view; MainWindow drives it through the dock's
    // appendOutput()/setText()/clearOutput() slots (no harvested pointer).
    connect(m_outputViewDock, &OutputDock::clearRequested,
            this, &MainWindow::clearOutputView);

    // ==================== SIMULATION CHARTS DIALOG (modeless) ====================
    // Claude Generated 2026 - live temperature + energy time series for the running MD/Opt,
    // fed from SimulationWorker::frameReady (wired in wireSimulationWorker()). Hosted in a
    // modeless dialog (opened from Molecule -> Simulation Charts) instead of a dock so it does
    // not consume layout space. Modeless (show(), not exec()) keeps the simulation controls
    // usable while the charts update live.
    m_simulationChartDialog = new QDialog(this);
    m_simulationChartDialog->setObjectName("SimulationChartDialog");
    m_simulationChartDialog->setWindowTitle(tr("Simulation Charts"));
    m_simulationChartDialog->setModal(false);
    m_simulationChartDialog->resize(640, 560);
    m_simulationChartWidget = new SimulationChartWidget(m_simulationChartDialog);
    auto* chartDialogLayout = new QVBoxLayout(m_simulationChartDialog);
    chartDialogLayout->setContentsMargins(4, 4, 4, 4);
    chartDialogLayout->addWidget(m_simulationChartWidget);

    // ==================== INITIAL PLACEMENT ====================
    // Phase 4: all docks are now owned by DockManager. Ask it to place them in the
    // default areas and tabify/split as configured.
    m_dockManager->placeDocks();
}

// Claude Generated (2026-04) - Save global dock/geometry on close so next start
// restores the user's last arrangement. Per-workspace save is orthogonal.
void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_dockManager)
        m_dockManager->saveLayout();
    // Claude Generated 2026 - The display state of this session is restored at the next
    // start (setupUI). Read-modify-write: centerOnLoad and friends keep their values.
    if (m_moleculeView) {
        Settings::VisualizationSettings c = m_settings.getVisualizationSettings();
        static_cast<DisplaySettings&>(c) = m_moleculeView->currentDisplaySettings();
        c.instancingThreshold = m_moleculeView->getInstancingThreshold();
        m_settings.setVisualizationSettings(c);
    }
    QMainWindow::closeEvent(event);
}

// Claude Generated 2026 - True if a text-entry widget currently has focus, so the
// WASD/QE rotation keys must pass through (don't steal letters from typing).
static bool isTextInputFocused()
{
    QWidget* w = QApplication::focusWidget();
    if (!w)
        return false;
    return qobject_cast<QLineEdit*>(w) || qobject_cast<QAbstractSpinBox*>(w)
        || qobject_cast<QPlainTextEdit*>(w) || qobject_cast<QTextEdit*>(w)
        || qobject_cast<QComboBox*>(w);
}

// Claude Generated 2026 - Wayland dock re-docking. Under Wayland Qt drags a dock panel as a
// platform drag-and-drop (QMainWindowLayout::performPlatformWidgetDrag; the compositor moves
// the window along via xdg_toplevel_drag_v1) and places the drop gap from the DragMove
// events that reach QMainWindow::event(). Qt delivers drag events only to the innermost
// widget under the cursor that accepts drops and never propagates them further, so the 3D
// viewer's QQuickWidget (acceptDrops by default, accepts every DragEnter) and the line/text
// edits inside the docks swallow them: no drop indicator, no re-dock. This redirects them to
// the main window. The MIME type is set by Qt for dock drags only; file drops don't carry it.
// On xcb Qt tracks the dock drag with mouse events instead, so nothing here fires.
bool MainWindow::forwardDockDragEvent(QObject* obj, QEvent* event)
{
    static const QString dockDragMime = QStringLiteral("application/x-qt-mainwindowdrag-window");

    const QEvent::Type type = event->type();
    if (type != QEvent::DragEnter && type != QEvent::DragMove && type != QEvent::Drop
        && type != QEvent::DragLeave)
        return false;

    auto* w = qobject_cast<QWidget*>(obj);
    if (!w || w->window() != this)
        return false;

    if (type == QEvent::DragLeave) {
        if (w == this) {  // handled by QMainWindow::event itself
            m_dockDragActive = false;
            return false;
        }
        if (!m_dockDragActive)
            return false;
        // Crossing from one widget to the next, Qt sends Leave(old), Enter(new) and Move in
        // one go. Forwarding the Leave at once would close and reopen the gap on every
        // crossing (QMainWindowLayout::hover -> restore), a visible flicker with
        // AnimatedDocks. Deferred, it only takes effect when no Enter/Move followed, i.e.
        // the cursor really left the window, so a drop over the desktop can't dock into a
        // stale gap. The queued call runs inside QDrag::exec()'s nested event loop.
        m_dockDragLeavePending = true;
        QMetaObject::invokeMethod(this, [this]() {
            if (!m_dockDragLeavePending)
                return;
            m_dockDragLeavePending = false;
            m_dockDragActive = false;
            QDragLeaveEvent leave;
            QMainWindow::event(&leave);  // not sendEvent(), see below
        }, Qt::QueuedConnection);
        return true;
    }

    auto* drop = static_cast<QDropEvent*>(event);
    if (!drop->mimeData() || !drop->mimeData()->hasFormat(dockDragMime)) {
        m_dockDragActive = false;  // some other drag (e.g. files): leave it alone
        return false;
    }

    m_dockDragLeavePending = false;
    m_dockDragActive = (type != QEvent::Drop);
    if (w == this)
        return false;  // QMainWindow::event handles it

    // Call QMainWindow::event() directly instead of sendEvent(this, ...): QApplication::notify
    // delivers DragMove/Drop/DragLeave to QDragManager's current target (the child that took
    // the DragEnter), whatever receiver is passed, so a sendEvent would come straight back to
    // this child and recurse until the stack overflows.
    const QPointF pos = w->mapTo(this, drop->position());
    auto forward = [this, drop](QDropEvent& fwd) {
        QMainWindow::event(&fwd);
        drop->setDropAction(fwd.dropAction());
        drop->setAccepted(fwd.isAccepted());
    };
    if (type == QEvent::DragEnter) {
        QDragEnterEvent fwd(pos.toPoint(), drop->possibleActions(), drop->mimeData(),
                            drop->buttons(), drop->modifiers());
        forward(fwd);
    } else if (type == QEvent::DragMove) {
        QDragMoveEvent fwd(pos.toPoint(), drop->possibleActions(), drop->mimeData(),
                           drop->buttons(), drop->modifiers());
        forward(fwd);
    } else {
        QDropEvent fwd(pos, drop->possibleActions(), drop->mimeData(), drop->buttons(),
                       drop->modifiers());
        forward(fwd);
    }
    return true;
}

// Claude Generated 2026 - Application-level key filter: WASD = pitch/yaw, QE = roll
// rotate the 3D scene from anywhere (the file browser used to eat the arrow keys), and
// Shift+WASDQE nudges the selection in Edit mode. Skipped while a text widget has focus
// and when Ctrl/Alt/Meta are held (so Ctrl+A etc. keep working).
bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (forwardDockDragEvent(obj, event))
        return true;

    // Claude Generated 2026 - Drag molecule files from the browser onto the Lesson
    // toggle to add them to the lesson (this filter is installed on qApp, so it sees
    // the button's drag events once the button has setAcceptDrops(true)).
    if (obj == m_lessonModeBtn && m_lessonModeBtn) {
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto* de = static_cast<QDragMoveEvent*>(event);
            if (de->mimeData()->hasUrls()) {
                de->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::Drop) {
            auto* de = static_cast<QDropEvent*>(event);
            if (de->mimeData()->hasUrls()) {
                QStringList paths;
                for (const QUrl& url : de->mimeData()->urls())
                    paths << url.toLocalFile();
                m_lessonController->addFiles(paths);  // filters + parses + status
                de->acceptProposedAction();
                return true;
            }
        }
    }

    // Claude Generated 2026 - Builder hotkeys, only while Build mode is on and the
    // viewport has focus. ShortcutOverride must be accepted first: some of these
    // letters (N = NCI overlay) and Del are registered QAction shortcuts, which
    // would otherwise swallow the key before it reaches this filter.
    if ((event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)
        && m_moleculeView
        && m_moleculeView->interactionMode() == MoleculeViewer::InteractionMode::Build
        && m_moleculeView->viewportHasFocus() && !isTextInputFocused()) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (!(ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
            // Claude Generated 2026 - While a bond preview is on screen, 1/2/3
            // force the bond order (override the distance rule). Without a
            // preview the keys keep their render-style shortcuts.
            if (ke->key() >= Qt::Key_1 && ke->key() <= Qt::Key_3
                && m_moleculeView->bondPreviewActive()) {
                if (event->type() == QEvent::ShortcutOverride) {
                    event->accept();
                    return true;
                }
                m_moleculeView->setForcedBondOrder(ke->key() - Qt::Key_0);
                return true;
            }
            QString element;
            switch (ke->key()) {
            case Qt::Key_H: element = QStringLiteral("H"); break;
            case Qt::Key_C: element = QStringLiteral("C"); break;
            case Qt::Key_N: element = QStringLiteral("N"); break;
            case Qt::Key_O: element = QStringLiteral("O"); break;
            case Qt::Key_S: element = QStringLiteral("S"); break;
            case Qt::Key_P: element = QStringLiteral("P"); break;
            case Qt::Key_F: element = QStringLiteral("F"); break;
            case Qt::Key_L: element = QStringLiteral("Cl"); break;
            case Qt::Key_R: element = QStringLiteral("Br"); break;
            case Qt::Key_X:
            case Qt::Key_Delete:
                if (event->type() == QEvent::ShortcutOverride) {
                    event->accept();
                    return true;
                }
                m_moleculeView->deleteSelection();
                return true;
            default:
                break;
            }
            if (!element.isEmpty()) {
                if (event->type() == QEvent::ShortcutOverride) {
                    event->accept();
                    return true;
                }
                m_moleculeView->setBuildElement(element);
                statusBar()->showMessage(tr("Build element: %1").arg(element), 1500);
                return true;
            }
        }
    }

    if (event->type() == QEvent::KeyPress && m_moleculeView) {
        auto* ke = static_cast<QKeyEvent*>(event);
        // Claude Generated 2026 - Trajectory playback keys, only when a multi-frame
        // file is loaded AND the 3D viewport has focus (click it first), so lists,
        // tables and buttons keep their arrow/space behaviour: Space = play/pause,
        // Left/Right = prev/next frame, Ctrl+Left/Right = first/last.
        if (m_moleculeView->getFrameCount() > 1 && m_moleculeView->viewportHasFocus()
            && !(ke->modifiers() & (Qt::AltModifier | Qt::MetaModifier | Qt::ShiftModifier))) {
            const bool ctrl = ke->modifiers() & Qt::ControlModifier;
            switch (ke->key()) {
            case Qt::Key_Space:
                if (!ctrl) {
                    m_moleculeView->toggleAnimation();
                    return true;
                }
                break;
            case Qt::Key_Left:
                ctrl ? m_moleculeView->firstFrame() : m_moleculeView->previousFrame();
                return true;
            case Qt::Key_Right:
                ctrl ? m_moleculeView->lastFrame() : m_moleculeView->nextFrame();
                return true;
            default:
                break;
            }
        }
        // Auto-repeat allowed: holding a key keeps rotating.
        if (!(ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
            const int key = ke->key();
            const bool isRotKey = key == Qt::Key_W || key == Qt::Key_A || key == Qt::Key_S
                || key == Qt::Key_D || key == Qt::Key_Q || key == Qt::Key_E;
            // Intercept WASD/QE in Edit mode AND in the interactive MD/Opt grab mode
            // (rotation is purely visual there); free everywhere else.
            if (isRotKey && (m_moleculeView->editMode() || m_moleculeView->simulationActive())
                && !isTextInputFocused()) {
                m_moleculeView->rotateSceneByKey(key, ke->modifiers() & Qt::ShiftModifier);
                return true;  // consume
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// Claude Generated - Interactive Simulation Integration

// Claude Generated - Keep shared config in sync when dock widget controls change
void MainWindow::onSimulationConfigChanged(SimulationConfig cfg)
{
    m_simulationConfig = cfg;

    // Claude Generated 2026 - Forward the confinement-wall config to the viewer
    // so the box wireframe is drawn live (auto-show when enabled) as the operator
    // types the bounds — preview before the MD run even starts. notifyConfig
    // emits on every field edit, so this updates in real time.
    if (m_moleculeView) {
        m_moleculeView->setConfinementBox(
            cfg.wallEnabled, cfg.wallType,
            QVector3D(cfg.wallXmin, cfg.wallYmin, cfg.wallZmin),
            QVector3D(cfg.wallXmax, cfg.wallYmax, cfg.wallZmax),
            float(cfg.wallRadius));
        // Keep iso-potential shell params in sync with the current wall config.
        m_moleculeView->setWallPotentialParams(
            cfg.wallHarmonic, cfg.wallTemp, float(cfg.wallBeta));
    }
}

// Claude Generated - Connect a freshly-created worker to the viewer + status bar directly.
// Avoids widget/dialog acting as an atom-data forwarder; eliminates two queued signal hops
// per frame. Connections auto-clean when the worker is deleteLater'd at simulation end.
// Claude Generated 2026 - Non-covalent interaction analysis.
//
// The worker gets its own thread for the whole session: the simulation thread is
// occupied by the MD timer, so a parameter generation posted there would stall a
// running simulation - and the analysis is meant to be usable while one runs.
void MainWindow::setupNciAnalysis()
{
    if (m_nciWorker || !m_moleculeView)
        return;

    m_nciThread = new QThread(this);
    m_nciWorker = new NciAnalysisWorker;
    m_nciWorker->moveToThread(m_nciThread);
    connect(m_nciThread, &QThread::finished, m_nciWorker, &QObject::deleteLater);
    m_nciThread->start();

    connect(m_nciWorker, &NciAnalysisWorker::resultReady, this,
        [this](quint64 requestId, const nci::Result& result) {
            if (requestId != m_nciRequestId)
                return;  // a newer request is already on its way
            if (m_nciDock)
                m_nciDock->setBusy(false);
            if (m_moleculeView)
                m_moleculeView->setNciResult(result);
        },
        Qt::QueuedConnection);

    connect(m_nciWorker, &NciAnalysisWorker::chargesReady, this,
        [this](quint64 requestId, int frame, const QVector<float>& charges) {
            if (requestId != m_nciRequestId || !m_moleculeView)
                return;
            if (frame != m_moleculeView->getCurrentFrame())
                return;
            m_moleculeView->setAtomCharges(charges);
            statusBar()->showMessage(
                tr("Atomic charges available - select \"By Charge\" in the Display panel "
                   "to colour the structure by them."),
                6000);
        },
        Qt::QueuedConnection);

    connect(m_nciWorker, &NciAnalysisWorker::errorOccurred, this,
        [this](const QString& message) {
            if (m_nciDock) {
                m_nciDock->setBusy(false);
                m_nciDock->setStatus(message);
            }
            statusBar()->showMessage(message, 8000);
        },
        Qt::QueuedConnection);

    // Viewer -> table. The geometric source recomputes on every frame, so this is
    // the single path that keeps the contact table in step with the overlay.
    connect(m_moleculeView, &MoleculeViewer::nciResultChanged, this,
        [this](const nci::Result& result) {
            if (m_nciDock)
                m_nciDock->setResult(result, m_moleculeView->getCurrentFrameAtoms());
        });

    // Claude Generated 2026 - Single mirror for every NCI entry point: whenever
    // the viewer's source changes (panel combo, dock, menu, bar button, shortcut,
    // analysis result), all UI representations follow from here.
    connect(m_moleculeView, &MoleculeViewer::nciSourceChanged, this, [this](int source) {
        if (source != 0)
            m_lastNciSource = source;
        if (m_nciToggleAction)
            m_nciToggleAction->setChecked(source != 0);
        if (m_nciSourceGroup)
            for (QAction* a : m_nciSourceGroup->actions())
                if (a->data().toInt() == source)
                    a->setChecked(true);
        if (m_nciDock) {
            m_nciDock->setSource(source);
            if (source != 0)
                m_nciDock->show();
        }
        if (m_displayPanel)
            m_displayPanel->syncFromViewer();
    });
    // The bar button's click reuses the shared toggle; its dropdown menu is attached in
    // createMenus(), which runs after this.
    connect(m_moleculeView, &MoleculeViewer::nciToggleRequested,
        this, &MainWindow::toggleNciOverlay);

    // The contact table uses the same interaction colours as the 3D overlay.
    connect(m_moleculeView, &MoleculeViewer::nciPaletteChanged, this, [this]() {
        if (m_nciDock)
            m_nciDock->setKindPalette(m_moleculeView->getNciPalette());
    });
    if (m_nciDock)
        m_nciDock->setKindPalette(m_moleculeView->getNciPalette());

    if (!m_nciDock)
        return;

    connect(m_nciDock, &NciDock::sourceChanged, this, [this](int source) {
        if (m_moleculeView && source <= 1)
            m_moleculeView->setNciSource(source);
        if (m_displayPanel)
            m_displayPanel->syncFromViewer();
        if (source >= 2)
            startNciAnalysis(source);
    });
    connect(m_nciDock, &NciDock::analysisRequested, this,
        [this](int source) { startNciAnalysis(source); });

    // Row click -> highlight the contact's atoms in the 3D view. The guard stops
    // the viewer's own selectionChanged from bouncing back into the table.
    connect(m_nciDock, &NciDock::contactSelected, this, [this](const QVector<int>& atoms) {
        if (m_nciSelectionSyncing || !m_moleculeView || atoms.isEmpty())
            return;
        m_nciSelectionSyncing = true;
        m_moleculeView->selectAtoms(atoms, false);
        m_nciSelectionSyncing = false;
    });
    connect(m_nciDock, &NciDock::contactFocused, this, [this](const QVector<int>& atoms) {
        if (m_nciSelectionSyncing || !m_moleculeView || atoms.isEmpty())
            return;
        m_nciSelectionSyncing = true;
        m_moleculeView->selectAtoms(atoms, false);
        m_moleculeView->zoomToSelection(atoms);
        m_nciSelectionSyncing = false;
    });

    // Claude Generated 2026 - UX stage 4: the NCI options live in the Interactions dock.
    auto* nciOptions = new NciOptionsWidget(m_moleculeView, &m_settings);
    connect(nciOptions, &NciOptionsWidget::liveMdChanged, this, [this](bool on) { m_nciLiveMd = on; });
    m_nciDock->setOptionsWidget(nciOptions);
}

// Claude Generated 2026 - NCI quick access: shortcut N, View menu, bar button.
// Toggle-on restores the last-used source (default: geometry, instant); calculated
// sources go through the analysis worker.
void MainWindow::toggleNciOverlay()
{
    if (!m_moleculeView)
        return;
    if (m_moleculeView->getNciSource() != 0)
        m_moleculeView->setNciSource(0);
    else if (m_lastNciSource >= 2)
        startNciAnalysis(m_lastNciSource);
    else
        m_moleculeView->setNciSource(1);
}

void MainWindow::setNciSourceFromUi(int source)
{
    if (!m_moleculeView)
        return;
    if (source <= 1)
        m_moleculeView->setNciSource(source);
    else
        startNciAnalysis(source);
}

void MainWindow::startNciAnalysis(int source)
{
    if (source < 2 || !m_nciWorker || !m_moleculeView)
        return;

    NciAnalysisWorker::Request request;
    request.atoms = m_moleculeView->getCurrentFrameAtoms();
    request.bonds = m_moleculeView->getCurrentFrameBonds();
    if (request.atoms.isEmpty()) {
        statusBar()->showMessage(tr("No structure loaded."), 4000);
        return;
    }
    request.method = (source == 2) ? QStringLiteral("gfnff") : QStringLiteral("gfn2");
    request.frame = m_moleculeView->getCurrentFrame();
    request.options = m_moleculeView->getNciOptions();
    request.requestId = ++m_nciRequestId;

    if (m_nciDock) {
        m_nciDock->setBusy(true);
        m_nciDock->setStatus(tr("Calculating (%1)...").arg(request.method));
        m_nciDock->show();
    }

    QMetaObject::invokeMethod(m_nciWorker, "analyse", Qt::QueuedConnection,
        Q_ARG(NciAnalysisWorker::Request, request));
}

void MainWindow::wireSimulationWorker(SimulationWorker* worker)
{
    if (!worker)
        return;

    // Claude Generated 2026 - Live interaction overlay. Only worth the cost when
    // the GFN-FF source is the one on screen: it makes the force field rebuild its
    // hydrogen- and halogen-bond lists on every gradient step. Must be set before
    // the worker's thread starts, which workerStarted guarantees.
    worker->setLiveNci(m_nciLiveMd && m_moleculeView
        && m_moleculeView->getNciSource() == int(nci::Source::GfnffParameters));

    if (m_moleculeView) {
        // Claude Generated 2026 - Critical: a new worker run must re-arm the
        // viewer's throttled moleculeUpdated emit. Otherwise the *first* frame
        // of e.g. an MD-after-Opt run is dropped (m_moleculeDirty was still
        // true from the previous run's first frame), the sim dock's m_atoms
        // cache never sees the new geometry, and the new run starts from the
        // stale cache instead of the previous run's final coordinates.
        m_moleculeView->resetSimDirty();

        // Claude Generated 2026 - P0/P3 (docs/WP-performance.md): mirror the
        // worker's own "Performance" checkbox onto the viewer's GUI-side timing,
        // and route frames through the coalescing entry point instead of the
        // heavy path directly, so a GUI-bound burst drops stale frames instead of
        // working through a growing backlog.
        m_moleculeView->setPerformanceAnalysis(
            m_simulationConfig.performanceAnalysis, m_simulationConfig.performanceInterval);

        connect(worker, &SimulationWorker::frameReady,
            m_moleculeView, &MoleculeViewer::onWorkerFrameReady,
            Qt::QueuedConnection);
        // Claude Generated 2026 - Phase 6: viewer drag → worker force injection.
        // QueuedConnection marshals the force matrix to the worker thread safely.
        connect(m_moleculeView, &MoleculeViewer::atomForceRequested,
            worker, &SimulationWorker::injectForce,
            Qt::QueuedConnection);
        connect(m_moleculeView, &MoleculeViewer::atomGrabReleased,
            worker, &SimulationWorker::clearInjectedForce,
            Qt::QueuedConnection);
    }

    // Claude Generated 2026 - Live temperature: dock slider drag → worker (worker thread).
    // QueuedConnection marshals the value across threads; the worker pushes it into the
    // running SimpleMD before the next step (and cancels any active global ramp).
    if (m_simulationControlWidget) {
        connect(m_simulationControlWidget, &SimulationControlWidget::temperatureChanged,
            worker, &SimulationWorker::setTargetTemperature,
            Qt::QueuedConnection);
        connect(m_simulationControlWidget, &SimulationControlWidget::wallTempChanged,
            worker, &SimulationWorker::setWallTemp,
            Qt::QueuedConnection);
        connect(m_simulationControlWidget, &SimulationControlWidget::wallBetaChanged,
            worker, &SimulationWorker::setWallBeta,
            Qt::QueuedConnection);
        // Keep iso-potential shell params in sync with live slider changes.
        if (m_moleculeView) {
            connect(m_simulationControlWidget, &SimulationControlWidget::wallTempChanged,
                m_moleculeView, [this](double T) {
                    m_moleculeView->setWallPotentialParams(
                        m_simulationConfig.wallHarmonic, T, float(m_simulationConfig.wallBeta));
                });
            connect(m_simulationControlWidget, &SimulationControlWidget::wallBetaChanged,
                m_moleculeView, [this](double beta) {
                    m_moleculeView->setWallPotentialParams(
                        m_simulationConfig.wallHarmonic, m_simulationConfig.wallTemp, float(beta));
                });
        }
    }

    // Claude Generated 2026 - Live charts: clear for the new run, then append every frame
    // (temperature + energies). The widget throttles its own axis rescaling.
    if (m_simulationChartWidget) {
        m_simulationChartWidget->reset();
        connect(worker, &SimulationWorker::frameReady,
            m_simulationChartWidget, &SimulationChartWidget::appendFrame,
            Qt::QueuedConnection);
    }

    // Claude Generated 2026 - Re-sync the sim-dock m_atoms cache with the
    // viewer's *current* geometry before the new run starts. The
    // moleculeUpdated signal from the previous run's first frame only ever
    // captured frame-1 coordinates; every frame after that mutated
    // m_trajectoryAtoms[0] in place but the dock was never told. So the
    // cache is always one run behind — unless we refresh it here.
    if (m_simulationControlWidget && m_moleculeView) {
        const QVector<MoleculeViewer::Atom> liveAtoms = m_moleculeView->getCurrentFrameAtoms();
        if (!liveAtoms.isEmpty())
            m_simulationControlWidget->setMolecule(liveAtoms,
                m_moleculeView->getCurrentFrameBonds());
    }

    // Status-bar slot is a lambda so we don't need a Qt slot declaration.
    // Throttled to ~5 Hz to reduce per-frame GUI overhead.
    connect(worker, &SimulationWorker::frameReady,
        this, [this](SimulationFramePtr frame) {
            if (!frame) return;
            if (m_simStatusBarTimer.isValid() && m_simStatusBarTimer.elapsed() < 200)
                return;
            m_simStatusBarTimer.restart();
            statusBar()->showMessage(
                tr("Simulation step %1 | E = %2 Eh | Ekin = %3 Eh")
                    .arg(frame->step)
                    .arg(frame->energy, 0, 'f', 8)
                    .arg(frame->ekin, 0, 'f', 6),
                0);
        },
        Qt::QueuedConnection);

    // Claude Generated 2026 - Auto-snapshot stride: if the user sets N > 0 in the
    // Snapshots tab, capture a snapshot every N-th simulation step/iteration.
    connect(worker, &SimulationWorker::frameReady,
        this, [this](SimulationFramePtr frame) {
            if (!frame || frame->step <= 0)
                return;
            const int stride = m_simulationControlWidget
                ? m_simulationControlWidget->autoStride() : 0;
            if (stride <= 0)
                return;
            if (frame->step % stride != 0)
                return;
            takeSnapshot(tr("Auto step %1").arg(frame->step));
        },
        Qt::QueuedConnection);
}

// Claude Generated (Apr 2026): Entry point for command-line file argument loading.
// Called via QTimer::singleShot(200) from main.cpp so Qt3D is fully initialized.
void MainWindow::loadFileFromArg(const QString& path)
{
    if (path.isEmpty()) return;
    QString absPath = QFileInfo(path).absoluteFilePath();
    if (!QFile::exists(absPath)) {
        qWarning() << "Command-line file not found:" << absPath;
        return;
    }
    loadMoleculeFile(absPath);
    // Note: loading a file does not change the Working Directory; it stays at
    // the startup default (last-used dir, or the invocation dir if enabled).
}

// Claude Generated 2026 - Auto-start the interactive simulation from the CLI
// (-md / -opt). Called after loadFileFromArg() so the dock already holds the
// loaded molecule. currentAtoms().isEmpty() mirrors onStartClicked()'s own
// guard, so a failed load (or a dir positional) is a clean no-op. This is a
// diagnostic lever: a direct onStartClicked() call is byte-for-byte identical
// to a button click, so the release/AVX-512 crash reproduces faithfully.
void MainWindow::autoStartSimulation(SimulationConfig::Mode mode)
{
    if (!m_simulationControlWidget)
        return;
    if (m_simulationControlWidget->currentAtoms().isEmpty())
        return;  // load produced no molecule
    qDebug() << "autoStartSimulation: mode=" << static_cast<int>(mode)
             << "atoms=" << m_simulationControlWidget->currentAtoms().size();
    m_simulationControlWidget->setMode(mode);
    m_simulationControlWidget->onStartClicked();
}

// Claude Generated 2026: CLI entry point for `qurcuma <directory>`.
// Switches the working directory without loading any file. Lets the user
// launch qurcuma pointing at a project directory (e.g. `qurcuma .`).
void MainWindow::loadDirFromArg(const QString& dir)
{
    if (dir.isEmpty()) return;
    QString absDir = QFileInfo(dir).absoluteFilePath();
    if (!QDir(absDir).exists()) {
        qWarning() << "Command-line directory not found:" << absDir;
        return;
    }
    if (absDir != m_workingDirectory) {
        switchWorkingDirectory(absDir);
    }
}
