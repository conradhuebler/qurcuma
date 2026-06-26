#include "settings.h"
#include "selectionmanager.h"  // Claude Generated - Phase 2A
#include "atomlistpanel.h"  // Claude Generated - Phase 2C
#ifdef USE_SFTP
#include "sftpmodel.hpp"
#include "dialogs/sftpdialog.h"
#endif
#include "simulationcontrolwidget.h"  // Claude Generated - Interactive Simulation Integration
#include "snapshotswidget.h"  // Claude Generated 2026 - Snapshot history foundation
#include "dialogs/lessonmetadatadialog.h"  // Claude Generated 2026 - lesson metadata editor
#include "lessonstructuremodel.h"  // Claude Generated 2026 - in-memory lesson structure list
// Claude Generated 2026 - Phase 6: SimulationDialog removed; the dock widget is the sole sim UI.
#include <algorithm>  // Claude Generated - for std::min/std::max
#include <QAbstractSpinBox>
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QCompleter>
#include <QDialog>
#include <QDir>
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
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringListModel>
#include <QSysInfo>
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
#include "docks/displaydock.h"  // Claude Generated 2026 - Dock system restructuring
#include "docks/editorsdock.h"  // Claude Generated 2026 - Dock system restructuring
#include "docks/outputdock.h"  // Claude Generated 2026 - Dock system restructuring
#include "docks/atomssimulationdock.h"  // Claude Generated 2026 - Dock system restructuring
#include "docks/bookmarkwidget.h"  // Claude Generated 2026 - Dock system restructuring
#include "docks/workspacepanel.h"  // Claude Generated 2026 - Dock system restructuring
#include "docks/remotedirectoriespanel.h"  // Claude Generated 2026 - Dock system restructuring
#include "docks/projectdock.h"  // Claude Generated 2026 - Dock system restructuring
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
    // Claude Generated - Initialize m_currentProcess first (needed by setupConnections)
    m_currentProcess = new QProcess(this);

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
    m_vtfParser = new VTFParser();
    m_xyzParser = new XYZParser();

    // Claude Generated - Visual Polish: Load dark mode setting and update checkbox
    m_darkModeEnabled = m_settings.darkModeEnabled();
    if (m_darkModeAction) {
        m_darkModeAction->setChecked(m_darkModeEnabled);
    }
    applyStylesheet(m_darkModeEnabled);
}

MainWindow::~MainWindow()
{
    delete m_vtfParser;
    delete m_xyzParser;
}

void MainWindow::setupUI()
{
    // Claude Generated - Quick Win: Enable drag and drop
    setAcceptDrops(true);

    // Claude Generated (2026-04) - Dock rewrite: MoleculeViewer is the real central widget.
    // Replaces the old 1x1 dummy — fixes dock resize math and eliminates the tab-support hack.
    m_moleculeView = new MoleculeViewer;
    Settings::VisualizationSettings vizSettings = m_settings.getVisualizationSettings();
    m_moleculeView->setRenderingMode(static_cast<MoleculeViewer::RenderingMode>(vizSettings.renderingMode));
    m_moleculeView->setColorScheme(static_cast<MoleculeViewer::ColorScheme>(vizSettings.colorScheme));
    m_moleculeView->setAtomTransparency(vizSettings.atomTransparency);
    m_moleculeView->setAtomShininess(vizSettings.atomShininess);
    m_moleculeView->setAtomScaleFactor(vizSettings.atomScaleFactor);
    m_moleculeView->setBondThickness(vizSettings.bondThickness);
    m_moleculeView->setFogEnabled(vizSettings.fogEnabled);
    m_moleculeView->setFogIntensity(vizSettings.fogIntensity);
    // Claude Generated 2026 - Interaction & Performance persisted values
    m_moleculeView->setRotationMode(vizSettings.rotationMode);
    m_moleculeView->setInstancingThreshold(vizSettings.instancingThreshold);
    m_centerOnLoad = vizSettings.centerOnLoad;
    setCentralWidget(m_moleculeView);

    // Initialize available program commands before creating docks
    initializeProgramCommands();

    // Claude Generated 2026 - Dock refactor: set dock options and tab positions
    // BEFORE creating/placing docks so tabify/split calls inherit the right config.
    setDockOptions(QMainWindow::AllowTabbedDocks |
                   QMainWindow::AnimatedDocks |
                   QMainWindow::AllowNestedDocks |
                   QMainWindow::GroupedDragging);

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

    // Claude Generated - Rendering mode shortcuts (Keys 1-4)
    new QShortcut(Qt::Key_1, this, SLOT(setRenderingModeBallAndStick()));
    new QShortcut(Qt::Key_2, this, SLOT(setRenderingModeSpaceFilling()));
    new QShortcut(Qt::Key_3, this, SLOT(setRenderingModeWireframe()));
    new QShortcut(Qt::Key_4, this, SLOT(setRenderingModeSticks()));

    // Claude Generated - Atom size shortcuts (Plus/Minus)
    new QShortcut(Qt::Key_Plus, this, SLOT(increaseAtomSize()));
    new QShortcut(Qt::Key_Equal, this, SLOT(increaseAtomSize()));  // Plus key often requires Shift on some keyboards
    new QShortcut(Qt::Key_Minus, this, SLOT(decreaseAtomSize()));

    // Claude Generated - Bond thickness shortcuts (< / >)
    new QShortcut(Qt::SHIFT | Qt::Key_Less, this, SLOT(decreaseBondThickness()));
    new QShortcut(Qt::SHIFT | Qt::Key_Greater, this, SLOT(increaseBondThickness()));
    new QShortcut(Qt::Key_Comma, this, SLOT(decreaseBondThickness()));      // Fallback for < key
    new QShortcut(Qt::Key_Period, this, SLOT(increaseBondThickness()));     // Fallback for > key

    // Claude Generated - Focus & view shortcuts
    new QShortcut(Qt::CTRL | Qt::Key_0, this, SLOT(fitMoleculeInView()));   // Ctrl+0 for fit all
    new QShortcut(Qt::Key_Home, this, SLOT(fitMoleculeInView()));            // Home key also fits
    new QShortcut(Qt::CTRL | Qt::Key_F, this, SLOT(centerViewOnSelection())); // Ctrl+F for focus
    new QShortcut(Qt::CTRL | Qt::Key_Backspace, this, SLOT(centerMoleculeAtOrigin())); // Ctrl+Backspace for center at origin

    // Claude Generated - Phase 2A: Selection shortcuts
    new QShortcut(Qt::CTRL | Qt::Key_A, this, SLOT(selectAllAtoms()));       // Ctrl+A for select all
    new QShortcut(Qt::Key_Escape, this, SLOT(clearAtomSelection()));          // Escape for clear selection

    // Claude Generated 2026 - P3 command palette: Ctrl+K is carried by the View ▸ Command
    // Palette menu action (P4); no standalone QShortcut here to avoid an ambiguous overload.

    // Initial updates
    updatePathLabel(m_workingDirectory);
    refreshBookmarkTree();

    // Window settings
    resize(1400, 900);  // Larger default size for flexible docking
    setWindowTitle("Qurcuma");

    // Claude Generated (2026-04) - Dock rewrite: capture baseline after Qt finished
    // placement, then prefer the globally persisted layout from QSettings. Falls
    // back to Analysis layout only on first run. Phase 5: state capture/restore is
    // owned by DockManager; geometry stays with MainWindow.
    QTimer::singleShot(0, this, [this]() {
        if (m_dockManager)
            m_dockManager->captureBaselineState();
        QSettings uiSettings;
        const QByteArray savedGeometry = uiSettings.value("ui/geometry").toByteArray();
        if (!savedGeometry.isEmpty())
            restoreGeometry(savedGeometry);
        if (m_dockManager)
            m_dockManager->restoreSavedLayout();
        // Claude Generated 2026 - P2: enforce the saved Explore/Compute mode last so the
        // calculation toolbar + dock visibility match the mode (default Explore on first run).
        const auto savedMode = static_cast<DockConfig::AppMode>(
            uiSettings.value("ui/appMode", static_cast<int>(DockConfig::AppMode::Explore)).toInt());
        setAppMode(savedMode, /*reflow=*/false);
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

    QWidget* spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);

    QAction* toggleNMR = toolbar->addAction(tr("NMR Spektren"));
    connect(toggleNMR, &QAction::triggered, [this]() { m_nmrDialog->show(); });
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
            if (m_lessonBrowseMode) {
                QMenu menu(this);
                QAction* loadAct = menu.addAction(tr("Load Structure"));
                QAction* removeAct = menu.addAction(tr("Remove from Lesson"));
                QAction* chosen = menu.exec(m_directoryContentView->viewport()->mapToGlobal(pos));
                if (chosen == loadAct) {
                    loadLessonStructureFromIndex(index);
                } else if (chosen == removeAct && index.row() < m_lesson.structures.size()) {
                    const QString name = m_lesson.structures.at(index.row()).name;
                    m_lesson.structures.remove(index.row());
                    refreshLessonStructureView();
                    showLessonStructureDetails(-1);  // rows shifted; reset the editor
                    statusBar()->showMessage(tr("Removed '%1' from lesson").arg(name), 3000);
                }
                return;
            }

            QString filePath = m_directoryContentModel->filePath(index);
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
                QAction *rmsdAction = contextMenu.addAction(tr("Overlay onto current (RMSD/Align)…"));
                connect(rmsdAction, &QAction::triggered,
                    [this, filePath]() { showRMSDTool(filePath); });

                // Claude Generated 2026 - merge this file into the current scene (editing).
                QAction *mergeAction = contextMenu.addAction(tr("Add to current scene"));
                connect(mergeAction, &QAction::triggered, this,
                    [this, filePath]() { mergeFileIntoScene(filePath); });

                // Claude Generated 2026 - Add this file straight into the lesson.
                QAction *lessonAction = contextMenu.addAction(tr("Add to Lesson"));
                connect(lessonAction, &QAction::triggered, this,
                    [this, filePath]() { addFileToLesson(filePath); });

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
                QAction *rmsdAction = contextMenu.addAction(tr("Overlay onto current (RMSD/Align)…"));
                connect(rmsdAction, &QAction::triggered,
                    [this, filePath]() { showRMSDTool(filePath); });

                // Claude Generated 2026 - merge this file into the current scene (editing).
                QAction *mergeAction = contextMenu.addAction(tr("Add to current scene"));
                connect(mergeAction, &QAction::triggered, this,
                    [this, filePath]() { mergeFileIntoScene(filePath); });

                // Claude Generated 2026 - Add this file straight into the lesson.
                QAction *lessonAction = contextMenu.addAction(tr("Add to Lesson"));
                connect(lessonAction, &QAction::triggered, this,
                    [this, filePath]() { addFileToLesson(filePath); });

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
                    [this, filePath]() {
                        PDBParser pdbParser;
                        PDBParser::PDBFrame frame;
                        if (pdbParser.parseFile(filePath, frame)) {
                            QVector<MoleculeViewer::Atom> atoms;
                            QVector<MoleculeViewer::Bond> bonds;
                            PDBParser::convertToMoleculeViewer(frame, atoms, bonds, pdbParser.getBonds());
                            m_moleculeView->addMolecule(atoms, bonds);
                            if (m_simulationControlWidget) m_simulationControlWidget->setMolecule(atoms, bonds);
                        } else {
                            QMessageBox::warning(this, tr("Error"), tr("Failed to parse PDB file: %1").arg(pdbParser.getLastError()));
                        }
                    });

                // Claude Generated 2026 - Overlay this file onto the current structure (RMSD/Align).
                contextMenu.addSeparator();
                QAction *rmsdAction = contextMenu.addAction(tr("Overlay onto current (RMSD/Align)…"));
                connect(rmsdAction, &QAction::triggered,
                    [this, filePath]() { showRMSDTool(filePath); });

                // Claude Generated 2026 - merge this file into the current scene (editing).
                QAction *mergeAction = contextMenu.addAction(tr("Add to current scene"));
                connect(mergeAction, &QAction::triggered, this,
                    [this, filePath]() { mergeFileIntoScene(filePath); });

                // Claude Generated 2026 - Add this file straight into the lesson.
                QAction *lessonAction = contextMenu.addAction(tr("Add to Lesson"));
                connect(lessonAction, &QAction::triggered, this,
                    [this, filePath]() { addFileToLesson(filePath); });

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
                    [this, filePath]() {
                        MOL2Parser mol2Parser;
                        MOL2Parser::MOL2Molecule molecule;
                        if (mol2Parser.parseFile(filePath, molecule)) {
                            QVector<MoleculeViewer::Atom> atoms;
                            QVector<MoleculeViewer::Bond> bonds;
                            MOL2Parser::convertToMoleculeViewer(molecule, atoms, bonds);
                            m_moleculeView->addMolecule(atoms, bonds);
                            if (m_simulationControlWidget) m_simulationControlWidget->setMolecule(atoms, bonds);
                        } else {
                            QMessageBox::warning(this, tr("Error"), tr("Failed to parse MOL2 file: %1").arg(mol2Parser.getLastError()));
                        }
                    });

                // Claude Generated 2026 - Overlay this file onto the current structure (RMSD/Align).
                contextMenu.addSeparator();
                QAction *rmsdAction = contextMenu.addAction(tr("Overlay onto current (RMSD/Align)…"));
                connect(rmsdAction, &QAction::triggered,
                    [this, filePath]() { showRMSDTool(filePath); });

                // Claude Generated 2026 - merge this file into the current scene (editing).
                QAction *mergeAction = contextMenu.addAction(tr("Add to current scene"));
                connect(mergeAction, &QAction::triggered, this,
                    [this, filePath]() { mergeFileIntoScene(filePath); });

                // Claude Generated 2026 - Add this file straight into the lesson.
                QAction *lessonAction = contextMenu.addAction(tr("Add to Lesson"));
                connect(lessonAction, &QAction::triggered, this,
                    [this, filePath]() { addFileToLesson(filePath); });

                contextMenu.exec(m_directoryContentView->viewport()->mapToGlobal(pos));
            }else if(filePath.endsWith(".gbw", Qt::CaseInsensitive) || filePath.endsWith(".loc", Qt::CaseInsensitive) || filePath.endsWith(".ges", Qt::CaseInsensitive))
            {
                QMenu contextMenu(this);
                QAction *fileNameAction = contextMenu.addAction(tr("Open with IboView"));
                connect(fileNameAction, &QAction::triggered, [this, filePath]() { openWithVisualizer(filePath, "iboview"); });
                contextMenu.exec(m_directoryContentView->viewport()->mapToGlobal(pos));

            }else if(filePath.contains("molden"))
            {
                QMenu contextMenu(this);
                QAction *fileNameAction = contextMenu.addAction(tr("Open with IboView"));
                connect(fileNameAction, &QAction::triggered, [this, filePath]() { openWithVisualizer(filePath, "iboview"); });
                contextMenu.exec(m_directoryContentView->viewport()->mapToGlobal(pos));
            }else if(filePath.contains("hess"))
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

            } else if (filePath.contains("out")) {
                QMenu contextMenu(this);
                QAction* nmrstruktur = new QAction(tr("Add to NMR Spectrum"), this);
                connect(nmrstruktur, &QAction::triggered, [this, filePath]() {
                    if (QMessageBox::question(this, tr("NMR Spektren"), tr("Do you want to load all files with the name filename from each directory?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
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

                contextMenu.addAction(nmrstruktur);
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

void MainWindow::initializeProgramCommands()
{
    // Curcuma Befehle
    m_programCommands["curcuma"] = QStringList{
        "--align",
        "--rmsd",
        "--cluster",
        "--compare",
        "--convert",
        "--distance",
        "--docking",
        "--energy",
        "--geometry",
        "--md",
        "--md-analysis",
        "--reactive",
        "--traj-rmsd",
        "--opt",
        // Weitere Befehle hier ergänzen
    };

    // XTB Befehle
    m_programCommands["xtb"] = QStringList{
        "--opt",
        "--md",
        "--hess",
        "--ohess",
        "--bhess",
        "--grad",
        "--ograd",
        "--scc",
        "--vip",
        "--vipea",
        "--sp",
        "--gfn0",
        "--gfn1",
        "--gfn2",
        "--gfnff",
        "--alpb",
        "--gbsa",
        "--cosmo",
        "--wbo",
        "--pop",
        "--molden",
        "--dipole",
        "--chrg",
        "--uhf"
        // Weitere Befehle hier ergänzen
    };
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
        tr("Molecule viewing & interactive simulation (hides the calculation toolbar)"));
    m_computeButton = makeBtn(tr("⚙ Compute"),
        tr("Run calculations: program / command / threads, with project & output panels"));

    modeWidget->setStyleSheet(QStringLiteral(
        "QToolButton { padding: 3px 14px; border: 1px solid palette(mid); }"
        "QToolButton:checked { background: palette(highlight); color: palette(highlighted-text);"
        " font-weight: bold; }"));

    connect(m_exploreButton, &QToolButton::clicked, this, [this]() { setAppMode(DockConfig::AppMode::Explore); });
    connect(m_computeButton, &QToolButton::clicked, this, [this]() { setAppMode(DockConfig::AppMode::Compute); });

    if (menuBar())
        menuBar()->setCornerWidget(modeWidget, Qt::TopRightCorner);
}

// Claude Generated 2026 - P2: apply a top-level mode. Sets the calculation toolbar +
// dock visibility explicitly (deterministic); reflow=false keeps restored sizes on startup.
void MainWindow::setAppMode(DockConfig::AppMode mode, bool reflow)
{
    m_appMode = mode;
    const bool explore = (mode == DockConfig::AppMode::Explore);

    for (QToolButton* b : { m_exploreButton, m_computeButton }) {
        if (!b)
            continue;
        b->blockSignals(true);
        b->setChecked((b == m_exploreButton) == explore);
        b->blockSignals(false);
    }
    QSettings().setValue("ui/appMode", static_cast<int>(mode));

    if (m_calculationToolbar)
        m_calculationToolbar->setVisible(!explore);

    // Phase 5: dock visibility and reflow are owned by DockManager.
    if (m_dockManager)
        m_dockManager->setAppMode(mode, reflow);

    statusBar()->showMessage(explore ? tr("Mode: Explore") : tr("Mode: Compute"), 2000);
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
            c.shortcut = a->shortcut().toString(QKeySequence::NativeText);
            c.enabled = a->isEnabled();
            QPointer<QAction> ap(a);
            c.run = [ap]() { if (ap) ap->trigger(); };
            out.append(c);
        }
    }
}

void MainWindow::showCommandPalette()
{
    if (!m_commandPalette)
        m_commandPalette = new CommandPalette(this);

    QVector<CommandPalette::Command> cmds;
    if (menuBar()) {
        for (QAction* topAct : menuBar()->actions()) {
            if (!topAct->menu())
                continue;
            QString top = topAct->text();
            top.remove('&');
            collectMenuCommands(topAct->menu(), top, cmds);
        }
    }
    // Curated viewer/mode commands that are shortcut-only (not in any menu).
    auto add = [&](const QString& title, const QString& ctx, std::function<void()> run) {
        CommandPalette::Command c;
        c.title = title;
        c.context = ctx;
        c.run = std::move(run);
        cmds.append(c);
    };
    add(tr("Explore Mode"), tr("Mode"), [this]() { setAppMode(DockConfig::AppMode::Explore); });
    add(tr("Compute Mode"), tr("Mode"), [this]() { setAppMode(DockConfig::AppMode::Compute); });
    add(tr("Ball and Stick"), tr("Render"), [this]() { setRenderingModeBallAndStick(); });
    add(tr("Space Filling"), tr("Render"), [this]() { setRenderingModeSpaceFilling(); });
    add(tr("Wireframe"), tr("Render"), [this]() { setRenderingModeWireframe(); });
    add(tr("Sticks"), tr("Render"), [this]() { setRenderingModeSticks(); });
    add(tr("Fit Molecule in View"), tr("View"), [this]() { fitMoleculeInView(); });
    add(tr("Center Molecule at Origin"), tr("View"), [this]() { centerMoleculeAtOrigin(); });
    add(tr("Select All Atoms"), tr("Selection"), [this]() { selectAllAtoms(); });
    add(tr("Clear Selection"), tr("Selection"), [this]() { clearAtomSelection(); });

    m_commandPalette->setCommands(cmds);
    m_commandPalette->popUp();
}

void MainWindow::createMenus()
{
    QMenuBar *menuBar = new QMenuBar;
    setMenuBar(menuBar);

    // File Menu
    QMenu *fileMenu = menuBar->addMenu(tr("&File"));

    // Claude Generated 2026 - Local file open. The previous File menu only
    // exposed "Open Remote File..."; the standard "Open File..." action was
    // missing. The action uses the current Working Directory as the dialog's
    // start path so the user lands where they expect, and the underlying
    // loadMoleculeFile() will auto-switch the working directory to the
    // file's parent directory on a successful load.
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

    // Claude Generated - Quick Win: Recent files menu
    m_recentFilesMenu = fileMenu->addMenu(QIcon::fromTheme("document-open-recent"), tr("&Recent Files"));
    m_recentFilesMenu->setEnabled(false);

    // Claude Generated Phase 4.5 - Workspace menu
    m_workspaceMenu = fileMenu->addMenu(QIcon::fromTheme("window-duplicate"), tr("&Workspaces"));

    QAction *saveWorkspaceAction = m_workspaceMenu->addAction(QIcon::fromTheme("document-save"), tr("&Save Current Workspace..."));
    saveWorkspaceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(saveWorkspaceAction, &QAction::triggered, this, &MainWindow::saveCurrentWorkspace);

    QAction *loadWorkspaceAction = m_workspaceMenu->addAction(QIcon::fromTheme("document-open"), tr("&Load Workspace..."));
    loadWorkspaceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(loadWorkspaceAction, &QAction::triggered, [this]() {
        // For now, load workspace can be done via the sidebar list
        statusBar()->showMessage(tr("Use workspace list in sidebar to load"), 2000);
    });

    m_workspaceMenu->addSeparator();

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
        if (!path.isEmpty()) openLesson(path);
    });
    QAction* addStructAction = lessonMenu->addAction(tr("&Add Current Structure to Lesson..."));
    connect(addStructAction, &QAction::triggered, this, &MainWindow::addCurrentStructureToLesson);
    QAction* metaAction = lessonMenu->addAction(tr("Lesson &Metadata..."));
    connect(metaAction, &QAction::triggered, this, &MainWindow::editLessonMetadata);
    lessonMenu->addSeparator();
    // Save: overwrite the currently open lesson file directly; Save As: always
    // prompt. Both route through saveLessonInteractive() (Claude Generated 2026).
    QAction* saveLessonAction = lessonMenu->addAction(tr("&Save Lesson"));
    connect(saveLessonAction, &QAction::triggered, this,
        [this]() { saveLessonInteractive(/*forceDialog=*/false); });
    QAction* saveLessonAsAction = lessonMenu->addAction(tr("Save Lesson &As..."));
    connect(saveLessonAsAction, &QAction::triggered, this,
        [this]() { saveLessonInteractive(/*forceDialog=*/true); });

    fileMenu->addSeparator();
    // Claude Generated - Visual Polish: Menu icons
    QAction *quitAction = fileMenu->addAction(QIcon::fromTheme("application-exit"), tr("&Quit"));
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    // Claude Generated - Quick Win: Edit Menu with Copy/Paste
    QMenu *editMenu = menuBar->addMenu(tr("&Edit"));

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

    // Claude Generated 2026 - Structure editing actions (active in viewer Edit mode).
    editMenu->addSeparator();

    QAction *editModeAction = editMenu->addAction(QIcon::fromTheme("transform-move"), tr("Structure &Edit Mode"));
    editModeAction->setCheckable(true);
    editModeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    editModeAction->setToolTip(tr("Select/move atoms & molecules, copy/paste, merge files, with clash feedback."));
    connect(editModeAction, &QAction::toggled, this, [this](bool on) {
        if (m_moleculeView) m_moleculeView->setEditMode(on);
    });
    if (m_moleculeView)
        connect(m_moleculeView, &MoleculeViewer::editModeChanged, editModeAction, &QAction::setChecked);

    QAction *deleteSelAction = editMenu->addAction(QIcon::fromTheme("edit-delete"), tr("&Delete Selection"));
    deleteSelAction->setShortcut(QKeySequence::Delete);
    deleteSelAction->setToolTip(tr("Delete the selected atoms (Edit mode, single-frame structures)."));
    connect(deleteSelAction, &QAction::triggered, this, [this]() {
        if (m_moleculeView && m_moleculeView->editMode())
            m_moleculeView->deleteSelection();
    });

    QAction *addMoleculeAction = editMenu->addAction(QIcon::fromTheme("list-add"), tr("&Add Molecule to Scene…"));
    addMoleculeAction->setToolTip(tr("Merge a molecule from a file into the current scene (single-frame structures)."));
    connect(addMoleculeAction, &QAction::triggered, this, &MainWindow::addMoleculeToScene);

    QAction *cursorLockAction = editMenu->addAction(tr("&Lock Cursor While Dragging"));
    cursorLockAction->setCheckable(true);
    cursorLockAction->setChecked(m_moleculeView ? m_moleculeView->dragCursorLock() : true);
    cursorLockAction->setToolTip(tr("Pin the cursor at the press point during a move so the drag never runs off-screen (relative drag)."));
    connect(cursorLockAction, &QAction::toggled, this, [this](bool on) {
        if (m_moleculeView) m_moleculeView->setDragCursorLock(on);
    });

    // Claude Generated - UI Restructuring: View Menu for dock visibility and layout presets
    QMenu *viewMenu = menuBar->addMenu(tr("&View"));

    // Claude Generated 2026 - P4: Command palette + mode switch entries (discoverability).
    QAction* paletteAction = viewMenu->addAction(QIcon::fromTheme("edit-find"), tr("Command &Palette…"));
    paletteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    connect(paletteAction, &QAction::triggered, this, &MainWindow::showCommandPalette);

    QMenu* modeMenu = viewMenu->addMenu(tr("&Mode"));
    QAction* exploreAct = modeMenu->addAction(QIcon::fromTheme("view-preview"), tr("&Explore"));
    connect(exploreAct, &QAction::triggered, this, [this]() { setAppMode(DockConfig::AppMode::Explore); });
    QAction* computeAct = modeMenu->addAction(QIcon::fromTheme("system-run"), tr("&Compute"));
    connect(computeAct, &QAction::triggered, this, [this]() { setAppMode(DockConfig::AppMode::Compute); });

    viewMenu->addSeparator();

    // Layout Presets submenu
    QMenu *layoutMenu = viewMenu->addMenu(QIcon::fromTheme("view-choose"), tr("&Layout Presets"));

    QAction *visualizationLayoutAction = layoutMenu->addAction(tr("&Visualization Mode"));
    visualizationLayoutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_1));
    visualizationLayoutAction->setToolTip(tr("Focus on 3D viewer (Ctrl+Alt+1)"));
    connect(visualizationLayoutAction, &QAction::triggered, this,
            [this]() { applyLayoutPreset(DockConfig::LayoutPreset::Visualization); });

    QAction *editingLayoutAction = layoutMenu->addAction(tr("&Editing Mode"));
    editingLayoutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_2));
    editingLayoutAction->setToolTip(tr("Focus on editors (Ctrl+Alt+2)"));
    connect(editingLayoutAction, &QAction::triggered, this,
            [this]() { applyLayoutPreset(DockConfig::LayoutPreset::Editing); });

    QAction *calculationLayoutAction = layoutMenu->addAction(tr("&Calculation Mode"));
    calculationLayoutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_3));
    calculationLayoutAction->setToolTip(tr("Focus on calculation workflow (Ctrl+Alt+3)"));
    connect(calculationLayoutAction, &QAction::triggered, this,
            [this]() { applyLayoutPreset(DockConfig::LayoutPreset::Calculation); });

    QAction *analysisLayoutAction = layoutMenu->addAction(tr("&Analysis Mode (All Panels)"));
    analysisLayoutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_4));
    analysisLayoutAction->setToolTip(tr("Balanced layout with all panels (Ctrl+Alt+4)"));
    connect(analysisLayoutAction, &QAction::triggered, this,
            [this]() { applyLayoutPreset(DockConfig::LayoutPreset::Analysis); });

    viewMenu->addSeparator();

    // Claude Generated (2026-04) - Dock rewrite: toggle actions for the dock architecture.
    QMenu *docksMenu = viewMenu->addMenu(QIcon::fromTheme("view-split-left-right"), tr("&Dock Panels"));

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
    addDockToggle(m_editorsDock,          tr("&Editors"));
    addDockToggle(m_displayDock,          tr("&Display"));
    addDockToggle(m_atomsSimulationDock,  tr("&Atoms && Simulation"));
    addDockToggle(m_outputViewDock,       tr("&Output"));

    // Display options (raises the Display dock) — moved here from Settings (P4).
    QAction *displayOptionsAction = viewMenu->addAction(
        QIcon::fromTheme("preferences-desktop"), tr("Display &Options…"));
    displayOptionsAction->setToolTip(tr("Open the Display panel (style, effects, lighting, tools)"));
    connect(displayOptionsAction, &QAction::triggered, this, &MainWindow::openVisualizationSettings);

    viewMenu->addSeparator();

    // Reset layout: restore the captured baseline (drops preset caches so they re-derive).
    QAction *resetLayoutAction = viewMenu->addAction(QIcon::fromTheme("view-restore"), tr("&Reset to Default Layout"));
    resetLayoutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_0));
    connect(resetLayoutAction, &QAction::triggered, this, [this]() {
        if (m_dockManager) {
            m_dockManager->resetToBaseline();
            statusBar()->showMessage(tr("Layout reset to default"), 2000);
        }
    });

    // Settings Menu
    QMenu *settingsMenu = menuBar->addMenu(tr("&Settings"));

    // Claude Generated 2026 - "Use Invocation Directory" preference
    // Checkbox state is updated in the constructor after settings are loaded.
    m_useInvocationDirAction = settingsMenu->addAction(QIcon::fromTheme("go-home"), tr("Use &Invocation Directory"));
    m_useInvocationDirAction->setCheckable(true);
    m_useInvocationDirAction->setToolTip(
        tr("If enabled, treat the directory from which qurcuma was launched "
           "as the active Working Directory on each launch."));
    connect(m_useInvocationDirAction, &QAction::triggered,
            this, &MainWindow::toggleUseInvocationDirectory);

    settingsMenu->addSeparator();

    // Claude Generated - Visual Polish: Dark mode toggle (checkbox state set later after loading settings)
    m_darkModeAction = settingsMenu->addAction(QIcon::fromTheme("weather-clear-night"), tr("&Dark Mode"));
    m_darkModeAction->setCheckable(true);
    // Note: checkbox state will be updated in constructor after loading settings
    connect(m_darkModeAction, &QAction::triggered, this, &MainWindow::toggleDarkMode);

    settingsMenu->addSeparator();
    QAction *configAction = settingsMenu->addAction(QIcon::fromTheme("preferences-system"), tr("Configure Programs..."));
    connect(configAction, &QAction::triggered, this, &MainWindow::configurePrograms);

    // Claude Generated 2026 - P4: "Molecule" menu merges Simulation (MD/Opt) + Analysis (RMSD).
    QMenu *moleculeMenu = menuBar->addMenu(tr("&Molecule"));

    // Menu entries focus the simulation dock/tab instead of opening a dialog.
    auto showSimDock = [this](SimulationConfig::Mode mode) {
        if (!m_atomsSimulationDock) return;
        m_atomsSimulationDock->show();
        m_atomsSimulationDock->raise();
        if (m_atomsSimulationTabs) m_atomsSimulationTabs->setCurrentIndex(1);
        if (m_simulationControlWidget) {
            SimulationConfig cfg = m_simulationControlWidget->currentConfig();
            cfg.mode = mode;
            m_simulationConfig = cfg;
        }
    };
    QAction *mdAction = moleculeMenu->addAction(
        QIcon::fromTheme("media-playback-start"), tr("Run &MD Simulation"));
    mdAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    mdAction->setToolTip(tr("Focus the simulation dock in MD mode"));
    connect(mdAction, &QAction::triggered, this,
        [showSimDock]() { showSimDock(SimulationConfig::Mode::MolecularDynamics); });

    QAction *optAction = moleculeMenu->addAction(
        QIcon::fromTheme("system-run"), tr("&Geometry Optimization"));
    optAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    optAction->setToolTip(tr("Focus the simulation dock in optimization mode"));
    connect(optAction, &QAction::triggered, this,
        [showSimDock]() { showSimDock(SimulationConfig::Mode::GeometryOptimization); });

    // Claude Generated 2026 - open the live temperature/energy charts (modeless dialog).
    QAction *chartsAction = moleculeMenu->addAction(
        QIcon::fromTheme("office-chart-line"), tr("Simulation &Charts…"));
    chartsAction->setToolTip(tr("Open the live temperature/energy charts for the running simulation."));
    connect(chartsAction, &QAction::triggered, this, [this]() {
        if (!m_simulationChartDialog)
            return;
        m_simulationChartDialog->show();
        m_simulationChartDialog->raise();
        m_simulationChartDialog->activateWindow();
    });

    moleculeMenu->addSeparator();

    QAction *rmsdAction = moleculeMenu->addAction(QIcon::fromTheme("view-object-histogram-linear"),
        tr("&RMSD / Align Structures"));
    rmsdAction->setToolTip(
        tr("Open the Analysis dock (RMSD tab): overlay two structures, align and "
           "optionally reorder atoms (curcuma RMSDDriver)."));
    connect(rmsdAction, &QAction::triggered, this, [this]() { showRMSDTool(); });

    moleculeMenu->addSeparator();

    QAction *centerOriginAction = moleculeMenu->addAction(
        QIcon::fromTheme("snap-orthogonal"), tr("&Center at Origin"));
    centerOriginAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Backspace));
    centerOriginAction->setToolTip(
        tr("Translate all frames so the mass-weighted centre-of-mass is at the origin, "
           "then reset the camera."));
    connect(centerOriginAction, &QAction::triggered, this,
        &MainWindow::centerMoleculeAtOrigin);

    // Help Menu - Claude Generated - Quick Fix: About dialog
    QMenu *helpMenu = menuBar->addMenu(tr("&Help"));
    QAction *aboutAction = helpMenu->addAction(QIcon::fromTheme("help-about"), tr("&About Qurcuma"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);

    // Statusleiste
    setStatusBar(new QStatusBar);
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

    // Claude Generated - Process connections (m_currentProcess already initialized in constructor)
    connect(m_currentProcess, &QProcess::readyReadStandardOutput,
        this, &MainWindow::processOutput);
    connect(m_currentProcess, &QProcess::readyReadStandardError,
        this, &MainWindow::processError);

    // Programmtyp-spezifische Aktionen
    connect(m_programSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [this](int index) {
            QString program = m_programSelector->itemText(index);
            if (m_simulationPrograms.contains(program)) {
                m_commandInput->setEnabled(true);
                m_commandInput->setPlaceholderText("Enter simulation command...");
            } else if (m_visualizerPrograms.contains(program)) {
                m_commandInput->setEnabled(false);
                m_commandInput->setPlaceholderText("Visualisierungsprogramm - kein Kommando nötig");
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
                m_commandCompleter->setModel(new QStringListModel(m_programCommands[program]));
            }
        });
    connect(m_directoryContentView, &QListView::clicked,
        [this](const QModelIndex& index) {
            // Claude Generated 2026 - In Lesson mode the view shows the in-memory
            // lesson model, not the filesystem; load that structure directly.
            if (m_lessonBrowseMode) {
                loadLessonStructureFromIndex(index);
                return;
            }
            QString filePath = m_directoryContentModel->filePath(index);
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
                    m_outputView->setPlainText(QString::fromUtf8(file.readAll()));
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
    // Claude Generated - Phase 1.2: Keyboard shortcuts
    new QShortcut(QKeySequence::New, this, SLOT(createNewDirectory()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_R), this, SLOT(runSimulation()));
    new QShortcut(QKeySequence::Refresh, this, SLOT(runSimulation()));  // F5
    // Claude Generated 2026 - Ctrl+S is bound to the File>Save menu action
    // (m_saveAction) below, so we deliberately omit a second QShortcut here to
    // avoid double-firing. The editor save behaviour is still reachable via
    // saveCurrentStructureAs() / editor shortcuts.
    new QShortcut(Qt::Key_Escape, this, SLOT(cancelCalculation()));
    new QShortcut(QKeySequence::NextChild, this, SLOT(switchEditorTab()));  // Ctrl+Tab

    // Claude Generated - Quick Win: Zoom to fit molecule (Home key)
    new QShortcut(Qt::Key_Home, this, SLOT(zoomToMolecule()));

    // Claude Generated - Quick Fix: Additional shortcuts
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), this, SLOT(clearOutputView()));  // Ctrl+L
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_0), this, SLOT(zoomToMolecule()));   // Ctrl+0
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

        if (m_programCommands.contains(program)) {
            m_commandInput->setPlaceholderText(tr("Enter command for %1...").arg(program));
            m_commandCompleter->setModel(new QStringListModel(m_programCommands[program]));
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
    dialog.setWindowTitle(tr("Programmpfade konfigurieren"));
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

// In mainwindow.cpp:
bool MainWindow::setupCalculationDirectory()
{
    bool ok;
    QString calcName = QInputDialog::getText(this, tr("New Calculation"),
        tr("Name der Rechnung:"), QLineEdit::Normal, "", &ok);
    
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
            tr("Konnte Berechnungsverzeichnis nicht erstellen."));
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

    statusBar()->showMessage(tr("Berechnungsverzeichnis erstellt: ") + calcName);
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
        m_outputView->setPlainText(QString::fromUtf8(file.readAll()));
        file.close();
    }
    if (scrollToBottom) {
        m_outputView->verticalScrollBar()->setValue(m_outputView->verticalScrollBar()->maximum());
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

    bool input_empty = true, structure_empty = true, argument_empty = true;
    // Generiere eindeutige Namen für diese Berechnung
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString structureFile = generateUniqueFileName(m_structureFileEdit->text(), m_structureFileEditExtension->text());
    QString trjFile = generateUniqueFileName(m_structureFileEdit->text(), "trj" + m_structureFileEditExtension->text());
    QString inputFile = generateUniqueFileName(m_inputFileEdit->text(), m_inputFileEditExtension->text());

    QString outputFile = generateUniqueFileName("output", "log");
    // Speichere aktuelle Strukturdaten
    QFile structFile(currentCalculationDir() + QDir::separator() + structureFile);
    if (structFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        structFile.write(m_structureView->toPlainText().toUtf8());
        structure_empty = m_structureView->toPlainText().toUtf8().isEmpty();
        structFile.close();
    }

    // Speichere den Text aus dem Eingabefeld in die Input-Datei
    QFile inputFileObj(currentCalculationDir() + QDir::separator() + inputFile);
    if (inputFileObj.open(QIODevice::WriteOnly | QIODevice::Text)) {
        inputFileObj.write(m_inputView->toPlainText().toLatin1());
        input_empty = m_inputView->toPlainText().toUtf8().isEmpty();
        inputFileObj.close();
    }
    argument_empty = m_commandInput->text().trimmed().isEmpty();
    // Speichere das Kommando in die Historie
    CalculationEntry entry;
    entry.id = timestamp;
    entry.program = program;
    entry.command = m_commandInput->text().trimmed();
    entry.structureFile = structureFile;
    entry.outputFile = outputFile;
    entry.timestamp = QDateTime::currentDateTime();
    entry.status = "started";

    if (program == "orca") {
        QString orcaPath = m_settings.orcaBinaryPath();
        if (orcaPath.isEmpty()) {
            // Claude Generated - Phase 4.1: Enhanced error dialog
            showEnhancedError(tr("ORCA Configuration Error"),
                tr("ORCA binary path is not configured."),
                tr("Please configure the ORCA binary directory in the settings."),
                [this]() {
                    configurePrograms();
                });
            return;
        }
        if (input_empty) {
            // Claude Generated - Phase 4.1: Enhanced error dialog
            showEnhancedError(tr("Input File Empty"),
                tr("Input file is empty."),
                tr("Please fill in the input file with ORCA configuration parameters."),
                nullptr);
            return;
        }
        // ORCA-spezifischer Start
        QString orcaExe = orcaPath + "/orca";
        m_currentProcess->setWorkingDirectory(currentCalculationDir());
        m_currentProcess->setProgram(orcaExe);
        // ORCA erwartet den Input-Dateinamen als Argument

        m_currentProcess->setArguments(QStringList() << inputFile);
        QFile::copy(currentCalculationDir() + QDir::separator() + structureFile, currentCalculationDir() + QDir::separator() + m_structureFileEdit->text() + ".xyz");
    }
    else {
        if (structure_empty) {
            // Claude Generated - Phase 4.1: Enhanced error dialog
            showEnhancedError(tr("Structure Data Missing"),
                tr("Structure data is empty."),
                tr("Please fill in the structure data in the Structure tab. This is required for curcuma and xtb calculations."),
                nullptr);
            return;
        }
        QString programPath = m_settings.getProgramPath(program);
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("OMP_NUM_THREADS", QString::number(m_threads->value()));
        m_currentProcess->setEnvironment(environment.toStringList());
        m_currentProcess->setWorkingDirectory(currentCalculationDir());
        m_currentProcess->setProgram(programPath);

        QStringList args;
        if (program == "curcuma") {
            args = entry.command.split(" ", Qt::SkipEmptyParts);
            if (args.size() >= 1) {
                args.insert(1, structureFile);
            }
        } else if (program == "xtb") {
            args << structureFile;
            args.append(entry.command.split(" ", Qt::SkipEmptyParts));
            connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                [this, entry, trjFile](int exitCode, QProcess::ExitStatus exitStatus) {
                    QString xtbOptLogFile = currentCalculationDir() + QDir::separator() + "xtbopt.xyz";
                    if (QFile::exists(xtbOptLogFile)) {
                        QFile::rename(xtbOptLogFile, trjFile);
                    }
                    xtbOptLogFile = currentCalculationDir() + QDir::separator() + "xtbopt.log";
                    if (QFile::exists(xtbOptLogFile)) {
                        QFile::rename(xtbOptLogFile, trjFile);
                    }
                });
        }
        m_currentProcess->setArguments(args);
    }
    m_currentProcess->setStandardOutputFile(currentCalculationDir() + QDir::separator() + outputFile, QIODevice::Append);
    m_currentProcess->setStandardErrorFile(currentCalculationDir() + QDir::separator() + outputFile, QIODevice::Append);

    // Starte Prozess und füge Eintrag zur Historie hinzu
    m_currentProcess->start();
    addCalculationToHistory(entry);

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

    // Connect cancel button to stop process
    connect(m_progressDialog, &QProgressDialog::canceled, [this]() {
        if (m_currentProcess && m_currentProcess->state() == QProcess::Running) {
            m_currentProcess->kill();
            statusBar()->showMessage(tr("Calculation canceled by user"));
        }
    });

    // Verbinde Prozessende
    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [this, entry](int exitCode, QProcess::ExitStatus exitStatus) {
            // Claude Generated - Phase 1.3: Close progress dialog
            if (m_progressDialog) {
                m_progressDialog->close();
                m_progressDialog = nullptr;
            }

            // Claude Generated - Quick Win: Stop calculation timer
            if (m_calculationTimer) {
                m_calculationTimer->stop();
            }

            // Aktualisiere Status in der Historie
            CalculationEntry updatedEntry = entry;
            updatedEntry.status = (exitCode == 0) ? "completed" : "error";
            addCalculationToHistory(updatedEntry);

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

    });

    // Timer zum regelmäßigen Aktualisieren der Ausgabedatei
    QPointer<QTimer> outputUpdateTimer = new QTimer(this);
    connect(outputUpdateTimer, &QTimer::timeout, [this, outputFile]() {
        updateOutputView(currentCalculationDir() + QDir::separator() + outputFile, true);
    });
    outputUpdateTimer->start(1000); // Aktualisiere alle 1000 ms (1 Sekunde)

    // Stoppe den Timer, wenn der Prozess beendet ist
    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [outputUpdateTimer](int, QProcess::ExitStatus) {
            if (outputUpdateTimer) {
                outputUpdateTimer->stop();
                outputUpdateTimer->deleteLater();
            }
        });

    // Zeige eine Information und setze den Cursor auf "Warten"
    statusBar()->showMessage(tr("Calculation running..."));
    QApplication::setOverrideCursor(Qt::WaitCursor);
}

QString MainWindow::generateUniqueFileName(const QString &baseFileName, const QString &extension)
{
    if (m_uniqueFileNames->isChecked()) {
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        if (extension.isEmpty()) {
            return QString("%1_%2").arg(baseFileName, timestamp);
        } else
            return QString("%1_%2.%3").arg(baseFileName, timestamp, extension);
    } else {
        if (extension.isEmpty()) {
            return QString("%1").arg(baseFileName);
        } else
            return QString("%1.%2").arg(baseFileName, extension);
    }
}

void MainWindow::addCalculationToHistory(const CalculationEntry &entry)
{
    QString historyFile = currentCalculationDir() + QDir::separator() + "calculations.json";
    QList<CalculationEntry> history = loadCalculationHistory(currentCalculationDir());

    // Aktualisiere bestehenden Eintrag oder füge neuen hinzu
    bool updated = false;
    for (int i = 0; i < history.size(); ++i) {
        if (history[i].id == entry.id) {
            history[i] = entry;
            updated = true;
            break;
        }
    }
    if (!updated) {
        history.append(entry);
    }

    // Speichere aktualisierte Historie
    QJsonArray jsonArray;
    for (const auto &calc : history) {
        QJsonObject calcObj;
        calcObj["id"] = calc.id;
        calcObj["program"] = calc.program;
        calcObj["command"] = calc.command;
        calcObj["structureFile"] = calc.structureFile;
        calcObj["outputFile"] = calc.outputFile;
        calcObj["timestamp"] = calc.timestamp.toString(Qt::ISODate);
        calcObj["status"] = calc.status;
        calcObj["unqiueFileNames"] = m_uniqueFileNames->isChecked();
        jsonArray.append(calcObj);
    }

    QJsonObject rootObj;
    rootObj["calculations"] = jsonArray;
    
    QFile file(historyFile);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(rootObj);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}

QList<CalculationEntry> MainWindow::loadCalculationHistory(const QString &path)
{
    QList<CalculationEntry> history;
    QFile file(path + "/calculations.json");
    
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonArray calculations = doc.object()["calculations"].toArray();
        
        for (const auto &calcRef : calculations) {
            QJsonObject calc = calcRef.toObject();
            CalculationEntry entry;
            entry.id = calc["id"].toString();
            entry.program = calc["program"].toString();
            entry.command = calc["command"].toString();
            entry.structureFile = calc["structureFile"].toString();
            entry.outputFile = calc["outputFile"].toString();
            entry.timestamp = QDateTime::fromString(calc["timestamp"].toString(), Qt::ISODate);
            entry.status = calc["status"].toString();
            history.append(entry);
        }
        file.close();
    }
    
    return history;
}

void MainWindow::orcaPlotVib(const QString &filename, int frequency)
{
    QString orcaPath = m_settings.orcaBinaryPath();

        QString orcaExe = orcaPath + "/orca_pltvib";
        QString cfilename = QFileInfo(filename).fileName();
        m_currentProcess->setWorkingDirectory(currentCalculationDir());
        m_currentProcess->setProgram(orcaExe);
        m_currentProcess->setArguments(QStringList() << cfilename << QString::number(frequency));
        m_currentProcess->start();
        m_currentProcess->waitForFinished();

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
        m_currentProcess->setWorkingDirectory(currentCalculationDir());
        m_currentProcess->setProgram(orcaExe);
        m_currentProcess->setArguments(QStringList() << nfilePath << "-molden");
        m_currentProcess->start();
        m_currentProcess->waitForFinished();
        arguments << nfilePath + ".molden.input";
    }else
        arguments << filePath;  // Übergebe den Dateipfad als Argument

    // Debug-Ausgabe

    if (!QProcess::startDetached(programPath, arguments)) {
        QMessageBox::warning(this, tr("Error"),
            tr("Could not start %1.").arg(visualizer));
    }
}


bool MainWindow::checkProgramPath(const QString &program)
{
    QString path = m_settings.getProgramPath(program);
    if (path.isEmpty()) {
        QMessageBox::warning(this, "Fehler",
            "Bitte konfigurieren Sie zuerst den Pfad für " + program);
        return false;
    }
    return true;
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
        m_commandInput->setPlaceholderText("Visualisierungsprogramm - kein Kommando nötig");
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

void MainWindow::processOutput()
{
    QByteArray output = m_currentProcess->readAllStandardOutput();
    m_outputView->append(QString::fromUtf8(output));
}

void MainWindow::processError()
{
    QByteArray error = m_currentProcess->readAllStandardError();
    m_outputView->append("Error: " + QString::fromUtf8(error));
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

void MainWindow::startNewCalculation()
{
    QString program = m_programSelector->currentText();

    // Prüfe ob ein Programm ausgewählt ist
    if (program.isEmpty()) {
        QMessageBox::warning(this, tr("Error"),
            tr("Please select a program first."));
        return;
    }

    // Prüfe ob Input vorhanden ist
    if (m_inputView->toPlainText().isEmpty() && program == "orca") {
        QMessageBox::warning(this, tr("Error"),
            tr("Bitte geben Sie zuerst Input-Daten ein."));
        return;
    }

    // Je nach Programmtyp die entsprechende Aktion ausführen
    if (m_simulationPrograms.contains(program)) {
        runSimulation();
    }
}


void MainWindow::updateDirectoryContent()
{
    // Claude Generated - Handle empty calculation directory
    if (!isValidCalculationDir()) {
        // Show working directory itself if no calculation dir selected
        QString fullPath = m_workingDirectory;
        QModelIndex rootIndex = m_directoryContentModel->setRootPath(fullPath);
        m_directoryContentView->setRootIndex(rootIndex);
    } else {
        QString fullPath = currentCalculationDir();
        QModelIndex rootIndex = m_directoryContentModel->setRootPath(fullPath);
        m_directoryContentView->setRootIndex(rootIndex);
    }
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
            m_outputView->setPlainText(QString::fromUtf8(outputFile.readAll()));
            outputFile.close();
        }
    } else {
        m_outputView->clear();
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

void MainWindow::saveCalculationInfo()
{
    CalculationEntry info;
    info.program = m_programSelector->currentText();
    info.command = m_commandInput->text();
    info.structureFile = m_structureFileEdit->text();
    info.inputFile = m_inputFileEdit->text();
    info.outputFile = "compute.log";  // oder andere Output-Datei
    info.timestamp = QDateTime::currentDateTime();

    QJsonObject json;
    json["program"] = info.program;
    json["command"] = info.command;
    json["structureFile"] = info.structureFile;
    json["inputFile"] = info.inputFile;
    json["outputFile"] = info.outputFile;
    json["timestamp"] = info.timestamp.toString(Qt::ISODate);
    
    // Füge Programm-spezifische Informationen hinzu
    QJsonObject programInfo;
    if (info.program == "orca") {
        programInfo["type"] = "quantum-chemistry";
        // Weitere ORCA-spezifische Informationen
    } else if (info.program == "xtb") {
        programInfo["type"] = "semi-empirical";
        // Weitere XTB-spezifische Informationen
    } else if (info.program == "curcuma") {
        programInfo["type"] = "modeling";
        // Weitere Curcuma-spezifische Informationen
    }
    json["programInfo"] = programInfo;

    // Optional: Füge System-Informationen hinzu
    QJsonObject systemInfo;
    systemInfo["hostname"] = QSysInfo::machineHostName();
    systemInfo["os"] = QSysInfo::prettyProductName();
    json["systemInfo"] = systemInfo;

    // Speichere JSON-Datei
    QFile jsonFile(currentCalculationDir() + "/calculation.json");
    if (jsonFile.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(json);
        jsonFile.write(doc.toJson(QJsonDocument::Indented));
        jsonFile.close();
    }
}

void MainWindow::loadCalculationInfo(const QString &path)
{
    QFile jsonFile(path + "/calculation.json");
    if (jsonFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll());
        QJsonObject json = doc.object();
        
        // Setze UI-Elemente basierend auf den gespeicherten Informationen
        if (json.contains("program")) {
            int index = m_programSelector->findText(json["program"].toString());
            if (index >= 0) {
                m_programSelector->setCurrentIndex(index);
            }
        }
        
        if (json.contains("command")) {
            m_commandInput->setText(json["command"].toString());
        }
        
        // ... Weitere Informationen laden
        
        jsonFile.close();
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
void MainWindow::switchWorkingDirectory(const QString& path)
{
    if (path.isEmpty() || !QDir(path).exists()) {
        QMessageBox::warning(this, tr("Error"),
            tr("Directory does not exist: %1").arg(path));
        return;
    }

    m_workingDirectory = path;
    m_settings.setLastUsedWorkingDirectory(path);
    m_projectModel->setRootPath(path);
    m_projectListView->setRootIndex(m_projectModel->index(path));

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
void MainWindow::cancelCalculation()
{
    if (m_currentProcess && m_currentProcess->state() == QProcess::Running) {
        m_currentProcess->kill();
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

void MainWindow::saveCurrentEditor()
{
    // Get current focused editor and save its content
    // This is a placeholder implementation
    QTextEdit* currentEditor = nullptr;

    if (m_structureView->hasFocus()) {
        currentEditor = m_structureView;
    } else if (m_inputView->hasFocus()) {
        currentEditor = m_inputView;
    } else if (m_outputView->hasFocus()) {
        currentEditor = m_outputView;
    }

    if (currentEditor) {
        // In a full implementation, would save to file
        statusBar()->showMessage(tr("Editor content ready to save"));
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
// Lessons (OER teaching scenarios) - Claude Generated 2026. See lesson.h.
// ============================================================================

// Open a self-contained *.qlesson.json, unpack its embedded structures into a
// sibling working directory (so they appear in the file browser), and adopt it
// as the in-memory lesson. Clicking a structure later restores its conditions
// via applyLessonConditions() (hooked into loadMoleculeFile()).
void MainWindow::openLesson(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Open Lesson"), tr("Could not read %1").arg(path));
        return;
    }
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
    f.close();
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, tr("Open Lesson"),
            tr("Invalid lesson JSON: %1").arg(perr.errorString()));
        return;
    }
    QString err;
    Lesson lesson = lessonFromJson(doc.object(), &err);
    if (!err.isEmpty()) {
        QMessageBox::warning(this, tr("Open Lesson"), err);
        return;
    }
    if (lesson.structures.isEmpty()) {
        QMessageBox::information(this, tr("Open Lesson"), tr("This lesson contains no structures."));
        return;
    }

    // Unpack into <file-dir>/<stem>/ so the structures show up in the browser.
    const QFileInfo fi(path);
    QString stem = fi.fileName();
    stem.remove(QStringLiteral(".qlesson.json"), Qt::CaseInsensitive);
    stem.remove(QStringLiteral(".json"), Qt::CaseInsensitive);
    if (stem.isEmpty()) stem = QStringLiteral("lesson");
    const QString targetDir = fi.absoluteDir().filePath(stem);

    QString extractErr;
    if (!extractLesson(lesson, targetDir, &extractErr)) {
        QMessageBox::warning(this, tr("Open Lesson"), extractErr);
        return;
    }

    m_lesson = lesson;  // adopt so further edits / re-save work
    m_lessonFilePath = path;  // remember source so "Save Lesson" can overwrite it
    switchWorkingDirectory(targetDir);
    // Refresh the in-memory list (count); the extracted .xyz already show in the
    // file browser, so stay in Files mode rather than auto-switching.
    refreshLessonStructureView(/*autoShow=*/false);

    const QString title = lesson.meta.title.isEmpty() ? fi.fileName() : lesson.meta.title;
    const QString author = lesson.meta.authors.isEmpty() ? QString() : lesson.meta.authors.first().name;
    setWindowTitle(QStringLiteral("Qurcuma — %1%2").arg(title,
        author.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(author)));
    statusBar()->showMessage(tr("Lesson '%1' loaded: %2 structure(s) in %3")
        .arg(title).arg(lesson.structures.size()).arg(targetDir), 5000);
}

// Write the in-memory lesson as a self-contained *.qlesson.json (inline XYZ).
void MainWindow::saveLesson(const QString& path)
{
    if (m_lesson.structures.isEmpty()) {
        QMessageBox::information(this, tr("Save Lesson"),
            tr("The lesson is empty. Use 'Add Current Structure to Lesson…' first."));
        return;
    }
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    if (m_lesson.meta.created.isEmpty())
        m_lesson.meta.created = now;
    m_lesson.meta.modified = now;
    m_lesson.meta.qurcumaVersion = QCoreApplication::applicationVersion();

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Save Lesson"), tr("Could not write %1").arg(path));
        return;
    }
    f.write(QJsonDocument(lessonToJson(m_lesson, /*inlineXyz=*/true)).toJson(QJsonDocument::Indented));
    f.close();
    m_lessonFilePath = path;  // remember target so a follow-up "Save Lesson" overwrites it
    statusBar()->showMessage(
        tr("Lesson saved: %1 (%2 structure(s))").arg(path).arg(m_lesson.structures.size()), 4000);
}

// Resolve the save target and call saveLesson(). With forceDialog==false, a known
// current lesson path (from openLesson/saveLesson) is overwritten silently — the
// "Save Lesson" behaviour the user expects. Otherwise (Save As, or no known path)
// a Save dialog is shown, defaulting to the current path. Claude Generated 2026.
bool MainWindow::saveLessonInteractive(bool forceDialog)
{
    if (m_lesson.structures.isEmpty()) {
        QMessageBox::information(this, tr("Save Lesson"),
            tr("The lesson is empty. Use 'Add Current Structure to Lesson…' first."));
        return false;
    }

    QString path = m_lessonFilePath;
    if (forceDialog || path.isEmpty()) {
        const QString startDir = !m_lessonFilePath.isEmpty()
            ? QFileInfo(m_lessonFilePath).absolutePath()
            : (m_workingDirectory.isEmpty() ? QDir::homePath() : m_workingDirectory);
        const QString suggestion = !m_lessonFilePath.isEmpty()
            ? QFileInfo(m_lessonFilePath).fileName()
            : QStringLiteral("lesson.qlesson.json");
        path = QFileDialog::getSaveFileName(this, tr("Save Lesson"),
            QDir(startDir).filePath(suggestion),
            tr("Qurcuma Lesson (*.qlesson.json);;All Files (*)"));
        if (path.isEmpty())
            return false;  // user cancelled
        if (!path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
            path += QStringLiteral(".qlesson.json");
    }
    saveLesson(path);
    return true;
}

// Build a LessonStructure from atoms + the dock's current simulation conditions,
// append it, and return its row. No UI changes — callers decide what to reveal.
int MainWindow::appendLessonStructureFromAtoms(const QString& name,
    const QVector<MoleculeViewer::Atom>& atoms)
{
    LessonStructure s;
    s.name = name.isEmpty()
        ? tr("Structure %1").arg(m_lesson.structures.size() + 1) : name;
    s.xyz = atomsToXyz(atoms, s.name);
    s.sim = m_simulationControlWidget ? m_simulationControlWidget->currentConfig() : SimulationConfig{};
    m_lesson.structures.push_back(s);
    return static_cast<int>(m_lesson.structures.size()) - 1;
}

// Capture the currently displayed structure as a new lesson entry. No dialogs: it
// is added with a default name, then selected so the user fills in name/notes/role
// in the inline detail editor.
void MainWindow::addCurrentStructureToLesson()
{
    if (!m_moleculeView)
        return;
    const QVector<MoleculeViewer::Atom> atoms = m_moleculeView->getCurrentFrameAtoms();
    if (atoms.isEmpty()) {
        statusBar()->showMessage(tr("No structure to add"), 3000);
        return;
    }
    const QString defaultName = QFileInfo(m_currentMoleculeFilePath).completeBaseName();
    const int row = appendLessonStructureFromAtoms(defaultName, atoms);

    refreshLessonStructureView(/*autoShow=*/true);  // switch to Lesson mode + count
    if (m_directoryContentView && m_lessonStructureModel)
        m_directoryContentView->setCurrentIndex(m_lessonStructureModel->index(row, 0));
    showLessonStructureDetails(row);
    if (m_structNameEdit) {
        m_structNameEdit->setFocus();
        m_structNameEdit->selectAll();  // ready to rename immediately
    }
    statusBar()->showMessage(
        tr("Added structure — edit name/notes/role below, then Save Lesson"), 5000);
}

// Add a structure straight from the file browser (context menu / drag-drop) without
// loading it into the viewer. Stays in the current browser mode (just bumps the
// count) so the user can add several files in a row.
void MainWindow::addFileToLesson(const QString& filePath)
{
    QVector<MoleculeViewer::Atom> atoms;
    QVector<MoleculeViewer::Bond> bonds;
    if (!parseFirstFrame(filePath, atoms, bonds) || atoms.isEmpty()) {
        statusBar()->showMessage(
            tr("Could not read structure: %1").arg(QFileInfo(filePath).fileName()), 3000);
        return;
    }
    appendLessonStructureFromAtoms(QFileInfo(filePath).completeBaseName(), atoms);
    refreshLessonStructureView(/*autoShow=*/false);  // bump count, keep current mode
    statusBar()->showMessage(
        tr("Added '%1' to lesson (%2). Switch to Lesson to edit details.")
            .arg(QFileInfo(filePath).completeBaseName()).arg(m_lesson.structures.size()), 4000);
}

// Edit the lesson-level metadata (title, authors with ORCID/institution, ...).
void MainWindow::editLessonMetadata()
{
    LessonMetadataDialog dlg(m_lesson.meta, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_lesson.meta = dlg.metadata();
        refreshLessonMetaWidget();  // reflect changes in the inline widget
    }
}

// If the just-loaded file belongs to an unpacked lesson (a lesson.json sidecar in
// its directory references it by name), restore that structure's stored
// simulation conditions into the dock. No-op for ordinary files.
void MainWindow::applyLessonConditions(const QString& filePath)
{
    if (!m_simulationControlWidget)
        return;
    const QFileInfo fi(filePath);
    const QString sidecar = fi.absoluteDir().filePath(QStringLiteral("lesson.json"));
    if (!QFile::exists(sidecar))
        return;
    QFile f(sidecar);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return;
    const Lesson lesson = lessonFromJson(doc.object());
    const QString fname = fi.fileName();
    for (const LessonStructure& s : lesson.structures) {
        if (s.file == fname) {
            m_simulationControlWidget->applyConfig(s.sim);
            QString msg = tr("Lesson conditions applied: %1").arg(s.name);
            if (!s.description.isEmpty())
                msg += QStringLiteral(" — ") + s.description;
            statusBar()->showMessage(msg, 5000);
            return;
        }
    }
}

// Swap the content view between the filesystem model and the in-memory lesson
// model (no second view), and show/hide the lesson metadata + per-structure detail
// widgets. Restores the filesystem root index when returning to Files mode.
void MainWindow::setBrowserMode(bool lessonMode)
{
    if (!m_directoryContentView)
        return;
    m_lessonBrowseMode = lessonMode;
    if (m_filesModeBtn) m_filesModeBtn->setChecked(!lessonMode);
    if (m_lessonModeBtn) m_lessonModeBtn->setChecked(lessonMode);
    if (m_lessonMetaWidget) m_lessonMetaWidget->setVisible(lessonMode);
    const bool haveSel = (m_currentLessonRow >= 0 && m_currentLessonRow < m_lesson.structures.size());
    if (m_lessonStructWidget) m_lessonStructWidget->setVisible(lessonMode && haveSel);
    if (lessonMode) {
        refreshLessonMetaWidget();
        m_directoryContentView->setModel(m_lessonStructureModel);
        m_directoryContentView->setRootIndex(QModelIndex());  // flat list
    } else {
        m_directoryContentView->setModel(m_directoryContentModel);
        updateDirectoryContent();  // restore the filesystem root index
    }
}

// Refresh the in-memory lesson-structure model and the Lesson toggle's count. With
// autoShow, switch to Lesson mode so the user sees a just-added structure.
void MainWindow::refreshLessonStructureView(bool autoShow)
{
    if (m_lessonStructureModel)
        m_lessonStructureModel->refresh();
    const int n = static_cast<int>(m_lesson.structures.size());
    if (m_lessonModeBtn)
        m_lessonModeBtn->setText(tr("Lesson (%1)").arg(n));
    if (autoShow && n > 0 && !m_lessonBrowseMode)
        setBrowserMode(true);
}

// Mirror the lesson-level metadata into the inline metadata widget.
void MainWindow::refreshLessonMetaWidget()
{
    if (m_lessonTitleEdit) m_lessonTitleEdit->setText(m_lesson.meta.title);
    if (m_lessonDescEdit) m_lessonDescEdit->setText(m_lesson.meta.description);
    if (m_lessonAuthorsLabel) {
        QStringList names;
        for (const LessonAuthor& a : m_lesson.meta.authors)
            names << (a.name.isEmpty() ? a.orcid : a.name);
        m_lessonAuthorsLabel->setText(names.isEmpty() ? tr("(none)") : names.join(QStringLiteral(", ")));
    }
}

// Populate the per-structure detail editor from structure @p row (or hide it when
// the row is invalid). The role combo is signal-blocked so populating it doesn't
// write back. Claude Generated 2026.
void MainWindow::showLessonStructureDetails(int row)
{
    m_currentLessonRow = row;
    const bool valid = (row >= 0 && row < m_lesson.structures.size());
    if (m_lessonStructWidget)
        m_lessonStructWidget->setVisible(valid && m_lessonBrowseMode);
    if (!valid)
        return;
    const LessonStructure& s = m_lesson.structures.at(row);
    if (m_structNameEdit) m_structNameEdit->setText(s.name);     // setText: no textEdited
    if (m_structDescEdit) m_structDescEdit->setText(s.description);
    if (m_structRoleCombo) {
        QSignalBlocker blk(m_structRoleCombo);
        int idx = 0;
        if (s.role == QLatin1String("start")) idx = 1;
        else if (s.role == QLatin1String("intermediate")) idx = 2;
        else if (s.role == QLatin1String("target")) idx = 3;
        m_structRoleCombo->setCurrentIndex(idx);
    }
}

// Load an in-memory lesson structure: parse its embedded XYZ into the viewer and
// restore its stored simulation conditions. No working-directory switch and no
// source path (it is in-memory authored geometry), so a later Save prompts.
void MainWindow::loadLessonStructureFromIndex(const QModelIndex& index)
{
    if (!m_lessonStructureModel || !m_moleculeView)
        return;
    const LessonStructure* s = m_lessonStructureModel->at(index.row());
    if (!s)
        return;
    QVector<MoleculeViewer::Atom> atoms;
    if (!xyzToAtoms(s->xyz, atoms)) {
        statusBar()->showMessage(tr("Could not parse lesson structure '%1'").arg(s->name), 3000);
        return;
    }

    QVector<QVector<MoleculeViewer::Atom>> allAtoms { atoms };
    QVector<QVector<MoleculeViewer::Bond>> allBonds;  // empty => viewer auto-detects bonds
    m_moleculeView->clearScenePublic();
    m_moleculeView->setTrajectoryData(allAtoms, allBonds);
    if (m_centerOnLoad)
        m_moleculeView->centerAtOrigin();

    if (m_simulationControlWidget) {
        m_simulationControlWidget->setMolecule(m_moleculeView->getCurrentFrameAtoms(),
            m_moleculeView->getCurrentFrameBonds());
        m_simulationControlWidget->applyConfig(s->sim);  // restore stored conditions
        m_simulationControlWidget->setStructureModified(false);
    }
    m_currentMoleculeFilePath.clear();  // in-memory: force Save-As on a later save
    m_structureModified = false;
    if (m_saveAction) m_saveAction->setEnabled(true);
    if (m_saveAsAction) m_saveAsAction->setEnabled(true);
    captureInitialSnapshot(s->name, m_moleculeView->getCurrentFrameAtoms(),
        m_moleculeView->getCurrentFrameBonds());
    showLessonStructureDetails(index.row());  // bind the inline detail editor to it
    statusBar()->showMessage(tr("Loaded lesson structure: %1").arg(s->name), 4000);
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

// Claude Generated 2026 - In-dock reset: reload the current source file to
// discard any MD/Opt changes and start over from the original structure.
void MainWindow::reloadCurrentFile()
{
    resetToOriginalSnapshot();
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
// Claude Generated 2026 - Parse the first frame of a structure file into viewer atoms/
// bonds. Local parser instances keep the main parsers' state untouched.
bool MainWindow::parseFirstFrame(const QString& filePath, QVector<MoleculeViewer::Atom>& atoms,
    QVector<MoleculeViewer::Bond>& bonds)
{
    atoms.clear();
    bonds.clear();
    if (filePath.isEmpty() || !QFile::exists(filePath))
        return false;
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == "xyz") {
        XYZParser parser;
        XYZParser::XYZFrame frame;
        if (parser.parseTrajectory(filePath) && parser.getFrameCount() > 0 && parser.getFrame(0, frame))
            XYZParser::convertToMoleculeViewer(frame, atoms, bonds);
    } else if (suffix == "vtf") {
        VTFParser parser;
        VTFParser::VTFFrame frame;
        if (parser.parseTrajectory(filePath) && parser.getFrameCount() > 0 && parser.getFrame(0, frame))
            VTFParser::convertToMoleculeViewer(frame, atoms, bonds);
    } else if (suffix == "pdb") {
        PDBParser parser;
        PDBParser::PDBFrame frame;
        if (parser.parseFile(filePath, frame))
            PDBParser::convertToMoleculeViewer(frame, atoms, bonds, parser.getBonds());
    } else if (suffix == "mol2") {
        MOL2Parser parser;
        MOL2Parser::MOL2Molecule molecule;
        if (parser.parseFile(filePath, molecule))
            MOL2Parser::convertToMoleculeViewer(molecule, atoms, bonds);
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

// Claude Generated - Quick Win: Zoom to fit molecule
void MainWindow::zoomToMolecule()
{
    if (m_moleculeView) {
        m_moleculeView->resetViewToMolecule();
        statusBar()->showMessage(tr("Zoomed to fit molecule"), 1500);
    }
}

// Claude Generated - Quick Fix: Clear output view
void MainWindow::clearOutputView()
{
    m_outputView->clear();
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
void MainWindow::openVisualizationSettings()
{
    if (!m_displayDock)
        return;
    if (m_displayPanel)
        m_displayPanel->loadCurrentSettings();
    m_displayDock->show();
    m_displayDock->raise();
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

    // Auto-seed the reference from the currently displayed structure.
    seedRMSDReference();

    if (!targetFile.isEmpty())
        m_rmsdWidget->setTargetFile(targetFile);

    // Focus the Editors dock and switch to the RMSD / Align tab (index 2).
    if (m_editorsDock) {
        m_editorsDock->show();
        m_editorsDock->raise();
        m_editorsDock->activateWindow();
    }
    if (m_editorsTabs)
        m_editorsTabs->setCurrentIndex(2);  // Structure=0, Input=1, RMSD / Align=2
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
    m_rmsdWidget->setReferenceStructure(refAtoms, m_moleculeView->getCurrentFrameBonds(), refName);
}

// Claude Generated - Quick Fix: Show about dialog
void MainWindow::showAboutDialog()
{
    QMessageBox aboutBox(this);
    aboutBox.setWindowTitle(tr("About Qurcuma"));
    aboutBox.setIcon(QMessageBox::Information);
    aboutBox.setText(tr("Qurcuma 1.0"));
    aboutBox.setInformativeText(
        tr("Interactive molecular visualization for computational chemistry\n\n"
           "A Qt-based GUI for visualizing molecular structures and trajectories "
           "from quantum chemistry simulations.\n\n"
           "Supports: VTF, XYZ file formats"));
    aboutBox.setDetailedText(
        tr("Built with Qt %1\n"
           "Copyright 2025").arg(QT_VERSION_STR));
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
    Q_UNUSED(darkMode);

    // --- Disabled 2026: do not load any custom stylesheet. ---
    // QString stylesheetPath = darkMode ?
    //     ":/stylesheets/dark.qss" :
    //     ":/stylesheets/light.qss";
    // QFile styleFile(stylesheetPath);
    // if (styleFile.open(QFile::ReadOnly)) {
    //     QString styleSheet = QString::fromUtf8(styleFile.readAll());
    //     qApp->setStyleSheet(styleSheet);
    //     styleFile.close();
    // } else {
    //     qWarning() << "[Dark Mode] FAILED to open stylesheet file:" << stylesheetPath;
    //     qWarning() << "[Dark Mode] Error:" << styleFile.errorString();
    // }

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
        m_displayPanel->loadCurrentSettings();
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
    SelectionManager *selectionMgr = m_moleculeView->getSelectionManager();
    if (!selectionMgr) return;

    // Get current frame atoms count
    const auto& selectedAtoms = m_moleculeView->getSelectedAtoms();
    // We need to select all atoms - assuming we have access to atom count
    // For now, we'll select the first N atoms if we know the count
    if (!selectedAtoms.isEmpty()) {
        // Count selected atoms to determine total count
        int maxIndex = *std::max_element(selectedAtoms.begin(), selectedAtoms.end());
        for (int i = 0; i <= maxIndex; ++i) {
            if (!selectionMgr->isSelected(i)) {
                selectionMgr->selectAtom(i, true);  // Append to selection
            }
        }
    }
    m_moleculeView->update();  // Trigger redraw
    statusBar()->showMessage(tr("Selected all atoms"), 1500);
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
    } else if (m_dockManager) {
        // Fallback: Apply default layout if no dock state saved (backward compatibility)
        m_dockManager->applyPreset(DockConfig::LayoutPreset::Analysis);
    }

    if (m_workspaceManager) {
        m_workspaceManager->updateWorkspaceLastUsed(ws.id);
    }

    statusBar()->showMessage(tr("Workspace '%1' restored").arg(ws.name), 3000);
}

void MainWindow::updateWorkspaceMenu(QMenu* menu)
{
    if (!menu || !m_workspaceManager) return;

    auto workspaces = m_workspaceManager->listWorkspaces();
    if (workspaces.isEmpty()) {
        menu->addAction(tr("(No saved workspaces)"))->setEnabled(false);
        return;
    }

    for (int i = 0; i < std::min(5, static_cast<int>(workspaces.size())); ++i) {
        Settings::Workspace ws = workspaces[i];  // Copy instead of reference
        QAction* action = menu->addAction(ws.name);
        connect(action, &QAction::triggered, [this, ws]() {
            restoreWorkspaceState(ws);
        });
    }
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

    if (suffix == "xyz") {
        // XYZ file loading
        if (m_xyzParser->parseTrajectory(filePath)) {
            // Load XYZ data as text
            QFile file(filePath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                m_structureView->setPlainText(QString::fromUtf8(file.readAll()));
                m_structureFileEdit->setText(QFileInfo(filePath).fileName());
                file.close();
            }

            // Get frame count and setup trajectory data
            int frameCount = m_xyzParser->getFrameCount();
            DEBUG_LOG << "XYZ: frameCount =" << frameCount;
            m_moleculeView->setFrameCount(frameCount);

            // Reset molecule viewer for new file
            m_moleculeView->clearScenePublic();

            // Convert all frames to trajectory data
            QVector<QVector<MoleculeViewer::Atom>> allAtoms;
            QVector<QVector<MoleculeViewer::Bond>> allBonds;

            for (int i = 0; i < frameCount; ++i) {
                XYZParser::XYZFrame frame;
                if (m_xyzParser->getFrame(i, frame)) {
                    QVector<MoleculeViewer::Atom> atoms;
                    QVector<MoleculeViewer::Bond> bonds;
                    XYZParser::convertToMoleculeViewer(frame, atoms, bonds);
                    allAtoms.append(atoms);
                    allBonds.append(bonds);
                    DEBUG_LOG << "XYZ: Loaded frame" << i << "- atoms:" << atoms.size() << "bonds:" << bonds.size();
                } else {
                    DEBUG_LOG << "XYZ: Failed to load frame" << i;
                }
            }

            DEBUG_LOG << "XYZ: Total frames loaded:" << allAtoms.size();
            m_moleculeView->setTrajectoryData(allAtoms, allBonds);
            if (m_centerOnLoad) m_moleculeView->centerAtOrigin();

            // Claude Generated - Feed the loaded molecule into the inline
            // simulation widget so the user can start a run without manually
            // re-selecting it. First frame is used as the simulation input.
            if (m_simulationControlWidget && !allAtoms.isEmpty())
                m_simulationControlWidget->setMolecule(m_moleculeView->getCurrentFrameAtoms(),
                    m_moleculeView->getCurrentFrameBonds());
            // Claude Generated 2026 - Fresh load: clear the modified flag, cache
            // the new source path (so a follow-up Save overwrites it), and
            // enable the File>Save actions. The Save action is enabled as soon
            // as a molecule is on screen, regardless of modification state.
            m_currentMoleculeFilePath = filePath;
            m_structureModified = false;
            if (m_simulationControlWidget)
                m_simulationControlWidget->setStructureModified(false);
            if (m_saveAction) m_saveAction->setEnabled(true);
            if (m_saveAsAction) m_saveAsAction->setEnabled(true);
            // Store an in-memory snapshot of the original geometry so the
            // in-dock Reset button can restore it without reloading the file.
            // Snapshot 0 is the automatic load-time snapshot and also seeds the
            // manual snapshot history list.
            if (!allAtoms.isEmpty()) {
                captureInitialSnapshot(filePath, m_moleculeView->getCurrentFrameAtoms(),
                    m_moleculeView->getCurrentFrameBonds());
            }
            fileLoaded = true;
        } else {
            m_moleculeView->clearScenePublic();
            qWarning() << "Failed to parse XYZ file:" << filePath;
        }
    }
    else if (suffix == "vtf") {
        m_vtfParser = new VTFParser();
        if (m_vtfParser->parseTrajectory(filePath)) {
            // Load VTF data as text
            QFile file(filePath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                m_structureView->setPlainText(QString::fromUtf8(file.readAll()));
                m_structureFileEdit->setText(QFileInfo(filePath).fileName());
                file.close();
            }

            // Get frame count and setup trajectory data
            int frameCount = m_vtfParser->getFrameCount();
            DEBUG_LOG << "VTF: frameCount =" << frameCount;
            m_moleculeView->setFrameCount(frameCount);

            // Reset molecule viewer for new file
            m_moleculeView->clearScenePublic();

            // Convert all frames to trajectory data
            QVector<QVector<MoleculeViewer::Atom>> allAtoms;
            QVector<QVector<MoleculeViewer::Bond>> allBonds;

            for (int i = 0; i < frameCount; ++i) {
                VTFParser::VTFFrame frame;
                if (m_vtfParser->getFrame(i, frame)) {
                    QVector<MoleculeViewer::Atom> atoms;
                    QVector<MoleculeViewer::Bond> bonds;
                    VTFParser::convertToMoleculeViewer(frame, atoms, bonds);
                    allAtoms.append(atoms);
                    allBonds.append(bonds);
                    DEBUG_LOG << "VTF: Loaded frame" << i << "- atoms:" << atoms.size() << "bonds:" << bonds.size();
                } else {
                    DEBUG_LOG << "VTF: Failed to load frame" << i;
                }
            }

            DEBUG_LOG << "VTF: Total frames loaded:" << allAtoms.size();
            m_moleculeView->setTrajectoryData(allAtoms, allBonds);
            if (m_centerOnLoad) m_moleculeView->centerAtOrigin();

            // Claude Generated - Feed first frame into the simulation widget.
            if (m_simulationControlWidget && !allAtoms.isEmpty())
                m_simulationControlWidget->setMolecule(m_moleculeView->getCurrentFrameAtoms(),
                    m_moleculeView->getCurrentFrameBonds());
            // Claude Generated 2026 - Fresh load: clear the modified flag, cache
            // the new source path (so a follow-up Save overwrites it), and
            // enable the File>Save actions. The Save action is enabled as soon
            // as a molecule is on screen, regardless of modification state.
            m_currentMoleculeFilePath = filePath;
            m_structureModified = false;
            if (m_simulationControlWidget)
                m_simulationControlWidget->setStructureModified(false);
            if (m_saveAction) m_saveAction->setEnabled(true);
            if (m_saveAsAction) m_saveAsAction->setEnabled(true);
            // Store an in-memory snapshot of the original geometry so the
            // in-dock Reset button can restore it without reloading the file.
            // Snapshot 0 is the automatic load-time snapshot and also seeds the
            // manual snapshot history list.
            if (!allAtoms.isEmpty()) {
                captureInitialSnapshot(filePath, m_moleculeView->getCurrentFrameAtoms(),
                    m_moleculeView->getCurrentFrameBonds());
            }
            fileLoaded = true;
        } else {
            m_moleculeView->clearScenePublic();
            qWarning() << "Failed to parse VTF file:" << filePath;
        }
    }
    else if (suffix == "pdb" || suffix == "mol2") {
        // PDB/MOL2 support - placeholder for future implementation
        statusBar()->showMessage(tr("PDB/MOL2 support coming soon"), 2000);
    }
    else {
        statusBar()->showMessage(tr("Unsupported file format: %1").arg(suffix), 2000);
    }

    // Claude Generated 2026 - "Open file follows its own directory" semantics.
    // When a file is loaded successfully, the user almost always wants to
    // work next to the file (output files, sidecar data, related files).
    // We auto-switch the Working Directory to the file's parent directory.
    // Failure cases (parse error, unsupported format) leave the current
    // Working Directory untouched. switchWorkingDirectory already shows a
    // status-bar message and updates recent files.
    if (fileLoaded) {
        const QString fileDir = QFileInfo(filePath).absolutePath();
        if (!fileDir.isEmpty() && QDir(fileDir).exists() && fileDir != m_workingDirectory) {
            switchWorkingDirectory(fileDir);
        }
        // Claude Generated 2026 - If this file belongs to an unpacked lesson (a
        // lesson.json sidecar in its directory references it), restore the stored
        // simulation conditions into the dock. No-op for ordinary files.
        applyLessonConditions(filePath);
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

    // Load the downloaded file into the viewer based on file extension
    if (filePath.endsWith(".xyz", Qt::CaseInsensitive)) {
        if (m_xyzParser->parseTrajectory(localPath)) {
            int frameCount = m_xyzParser->getFrameCount();
            m_moleculeView->setFrameCount(frameCount);

            XYZParser::XYZFrame frame;
            if (m_xyzParser->getFrame(0, frame)) {
                QVector<MoleculeViewer::Atom> atoms;
                QVector<MoleculeViewer::Bond> bonds;
                XYZParser::convertToMoleculeViewer(frame, atoms, bonds);
                m_moleculeView->addMolecule(atoms, bonds);
            }
        }
    } else if (filePath.endsWith(".vtf", Qt::CaseInsensitive)) {
        m_vtfParser = new VTFParser();
        if (m_vtfParser->parseTrajectory(localPath)) {
            int frameCount = m_vtfParser->getFrameCount();
            m_moleculeView->setFrameCount(frameCount);

            VTFParser::VTFFrame frame;
            if (m_vtfParser->getFrame(0, frame)) {
                QVector<MoleculeViewer::Atom> atoms;
                QVector<MoleculeViewer::Bond> bonds;
                VTFParser::convertToMoleculeViewer(frame, atoms, bonds);
                m_moleculeView->addMolecule(atoms, bonds);
            }
        }
    } else if (filePath.endsWith(".pdb", Qt::CaseInsensitive)) {
        PDBParser pdbParser;
        PDBParser::PDBFrame frame;
        if (pdbParser.parseFile(localPath, frame)) {
            QVector<MoleculeViewer::Atom> atoms;
            QVector<MoleculeViewer::Bond> bonds;
            PDBParser::convertToMoleculeViewer(frame, atoms, bonds, pdbParser.getBonds());
            m_moleculeView->addMolecule(atoms, bonds);
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to parse PDB file: %1").arg(pdbParser.getLastError()));
        }
    } else if (filePath.endsWith(".mol2", Qt::CaseInsensitive)) {
        MOL2Parser mol2Parser;
        MOL2Parser::MOL2Molecule molecule;
        if (mol2Parser.parseFile(localPath, molecule)) {
            QVector<MoleculeViewer::Atom> atoms;
            QVector<MoleculeViewer::Bond> bonds;
            MOL2Parser::convertToMoleculeViewer(molecule, atoms, bonds);
            m_moleculeView->addMolecule(atoms, bonds);
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to parse MOL2 file: %1").arg(mol2Parser.getLastError()));
        }
    } else {
        QMessageBox::warning(this, tr("Unsupported Format"),
            tr("File format not supported: %1").arg(filePath));
        return;
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
    m_displayDock = m_dockManager->displayDockImpl();
    m_editorsDock = m_dockManager->editorsDockImpl();
    m_atomsSimulationDock = m_dockManager->atomsSimulationDockImpl();
    // Pull the wrapped internal widgets into MainWindow members so the rest of the
    // code can keep using them during the migration.
    m_editorsTabs = m_editorsDock ? m_editorsDock->editorsTabs() : nullptr;
    m_structureView = m_editorsDock ? m_editorsDock->structureView() : nullptr;
    m_inputView = m_editorsDock ? m_editorsDock->inputView() : nullptr;
    m_structureFileEdit = m_editorsDock ? m_editorsDock->structureFileEdit() : nullptr;
    m_structureFileEditExtension = m_editorsDock ? m_editorsDock->structureFileEditExtension() : nullptr;
    m_inputFileEdit = m_editorsDock ? m_editorsDock->inputFileEdit() : nullptr;
    m_inputFileEditExtension = m_editorsDock ? m_editorsDock->inputFileEditExtension() : nullptr;
    m_rmsdWidget = m_editorsDock ? m_editorsDock->rmsdWidget() : nullptr;

    if (m_atomsSimulationDock) {
        m_atomsSimulationTabs = m_atomsSimulationDock->tabs();
        m_atomListPanel = m_atomsSimulationDock->atomListPanel();
        m_simulationControlWidget = m_atomsSimulationDock->simulationControlWidget();
        m_snapshotsWidget = m_atomsSimulationDock->snapshotsWidget();
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
        m_lessonMetaWidget = m_projectDock->lessonMetaWidget();
        m_lessonTitleEdit = m_projectDock->lessonTitleEdit();
        m_lessonDescEdit = m_projectDock->lessonDescEdit();
        m_lessonAuthorsLabel = m_projectDock->lessonAuthorsLabel();
        m_lessonStructWidget = m_projectDock->lessonStructWidget();
        m_structNameEdit = m_projectDock->structNameEdit();
        m_structDescEdit = m_projectDock->structDescEdit();
        m_structRoleCombo = m_projectDock->structRoleCombo();

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
                this, [this]() { setBrowserMode(false); });
        connect(m_lessonModeBtn, &QToolButton::clicked,
                this, [this]() { setBrowserMode(true); });

        // Content view remote-file handling
#ifdef USE_SFTP
        connect(m_directoryContentView, &QListView::doubleClicked,
                this, &MainWindow::onRemoteFileDoubleClicked);
#endif

        // In-memory lesson structures model (owned by MainWindow, installed into the dock's view)
        m_lessonStructureModel = new LessonStructureModel(&m_lesson.structures, this);

        // Lesson metadata editors
        connect(m_lessonTitleEdit, &QLineEdit::textEdited, this,
            [this](const QString& t) { m_lesson.meta.title = t.trimmed(); });
        connect(m_lessonDescEdit, &QLineEdit::textEdited, this,
            [this](const QString& t) { m_lesson.meta.description = t.trimmed(); });
        connect(m_projectDock->editAuthorsButton(), &QToolButton::clicked,
                this, &MainWindow::editLessonMetadata);

        // Per-structure detail editors
        connect(m_structNameEdit, &QLineEdit::textEdited, this, [this](const QString& t) {
            if (m_currentLessonRow >= 0 && m_currentLessonRow < m_lesson.structures.size()) {
                m_lesson.structures[m_currentLessonRow].name = t;
                m_lessonStructureModel->refresh();
            }
        });
        connect(m_structDescEdit, &QLineEdit::textEdited, this, [this](const QString& t) {
            if (m_currentLessonRow >= 0 && m_currentLessonRow < m_lesson.structures.size())
                m_lesson.structures[m_currentLessonRow].description = t;
        });
        connect(m_structRoleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                if (m_currentLessonRow >= 0 && m_currentLessonRow < m_lesson.structures.size()) {
                    m_lesson.structures[m_currentLessonRow].role =
                        (idx <= 0) ? QString() : m_structRoleCombo->currentText();
                    m_lessonStructureModel->refresh();
                }
            });

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

    // ==================== EDITORS DOCK (right, top) ====================
    // Phase 2: wrapper is created by DockManager. Only wire the signals here.
    if (m_editorsDock) {
        // "Apply → Viewer" button lives inside the wrapper now.
        connect(m_editorsDock, &EditorsDock::structureApplyRequested,
                this, &MainWindow::applyStructureTextToViewer);

        // RMSD / align tool signals.
        if (m_rmsdWidget) {
            connect(m_rmsdWidget, &RMSDWidget::overlayRequested, this,
                [this](const QVector<MoleculeViewer::Atom>& refAtoms,
                    const QVector<MoleculeViewer::Bond>& refBonds,
                    const QVector<MoleculeViewer::Atom>& targetAtoms) {
                    if (m_moleculeView)
                        m_moleculeView->showOverlay(refAtoms, refBonds, targetAtoms);
                });
            connect(m_rmsdWidget, &RMSDWidget::seedReferenceRequested,
                    this, &MainWindow::seedRMSDReference);
        }
    }

    // ==================== ATOMS & SIMULATION DOCK (right, below Editors) ====================
    // Phase 3: wrapper is created by DockManager. Internal widgets were pulled above.

    // ==================== DISPLAY DOCK (right) ====================
    // Phase 2: wrapper is created by DockManager. Pull the panel and wire signals.
    if (m_displayDock)
        m_displayPanel = m_displayDock->displayPanel();
    if (!m_displayPanel) {
        // Fallback if DockManager was not initialized; should not happen.
        m_displayPanel = new DisplayPanel(m_moleculeView, &m_settings, this);
    }
    connect(m_displayPanel, &DisplayPanel::centerOnLoadChanged, this, [this](bool on) {
        m_centerOnLoad = on;
        Settings::VisualizationSettings vs = m_settings.getVisualizationSettings();
        vs.centerOnLoad = on;
        m_settings.setVisualizationSettings(vs);
    });
    connect(m_displayPanel, &DisplayPanel::potVectorFieldChanged,
        this, [this](bool on, int res) {
            if (m_moleculeView) m_moleculeView->setWallVectorField(on, res);
        });
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
    connect(m_snapshotsWidget, &SnapshotsWidget::takeSnapshotRequested,
        this, [this]() {
            takeSnapshot();
        });
    connect(m_snapshotsWidget, &SnapshotsWidget::restoreSnapshotRequested,
        this, [this](int index) {
            if (index >= 0 && index < m_snapshots.size())
                restoreSnapshot(m_snapshots[index]);
        });
    // Claude Generated 2026 - Protect snapshot 0 (original geometry) from deletion.
    // Deleting it would break the Reset-to-original invariant.
    connect(m_snapshotsWidget, &SnapshotsWidget::deleteSnapshotRequested,
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
    // Phase 2: wrapper is created by DockManager. Pull the log view and clear signal.
    if (m_outputViewDock)
        m_outputView = m_outputViewDock->outputView();
    if (!m_outputView) {
        // Fallback if DockManager was not initialized; should not happen.
        m_outputView = new QTextEdit;
        m_outputView->setPlaceholderText(tr("Output"));
        m_outputView->setReadOnly(true);
    }
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

// Claude Generated - UI Restructuring: Layout preset dispatcher
// Phase 5: all preset implementations live in DockManager; MainWindow only adds
// the status-bar message here.
void MainWindow::applyLayoutPreset(DockConfig::LayoutPreset preset)
{
    if (m_dockManager)
        m_dockManager->applyPreset(preset);

    QString msg;
    switch (preset) {
    case DockConfig::LayoutPreset::Visualization: msg = tr("Layout: Visualization Mode"); break;
    case DockConfig::LayoutPreset::Editing:       msg = tr("Layout: Editing Mode"); break;
    case DockConfig::LayoutPreset::Calculation:   msg = tr("Layout: Calculation Mode"); break;
    case DockConfig::LayoutPreset::Analysis:      msg = tr("Layout: Analysis Mode (All Panels)"); break;
    case DockConfig::LayoutPreset::Teaching:      msg = tr("Layout: Teaching Mode"); break;
    }
    if (!msg.isEmpty())
        statusBar()->showMessage(msg, 2000);
}

// Claude Generated (2026-04) - Save global dock/geometry on close so next start
// restores the user's last arrangement. Per-workspace save is orthogonal.
void MainWindow::closeEvent(QCloseEvent* event)
{
    QSettings uiSettings;
    uiSettings.setValue("ui/geometry", saveGeometry());
    uiSettings.setValue("ui/dockState", saveState());
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

// Claude Generated 2026 - Application-level key filter: WASD = pitch/yaw, QE = roll
// rotate the 3D scene from anywhere (the file browser used to eat the arrow keys), and
// Shift+WASDQE nudges the selection in Edit mode. Skipped while a text widget has focus
// and when Ctrl/Alt/Meta are held (so Ctrl+A etc. keep working).
bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
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
                int added = 0;
                for (const QUrl& url : de->mimeData()->urls()) {
                    const QString path = url.toLocalFile();
                    const QString suf = QFileInfo(path).suffix().toLower();
                    if (suf != QLatin1String("xyz") && suf != QLatin1String("vtf")
                        && suf != QLatin1String("pdb") && suf != QLatin1String("mol2"))
                        continue;
                    QVector<MoleculeViewer::Atom> atoms;
                    QVector<MoleculeViewer::Bond> bonds;
                    if (parseFirstFrame(path, atoms, bonds) && !atoms.isEmpty()) {
                        appendLessonStructureFromAtoms(QFileInfo(path).completeBaseName(), atoms);
                        ++added;
                    }
                }
                if (added > 0) {
                    refreshLessonStructureView(/*autoShow=*/false);
                    statusBar()->showMessage(
                        tr("Added %1 structure(s) to lesson (%2 total)")
                            .arg(added).arg(m_lesson.structures.size()), 4000);
                }
                de->acceptProposedAction();
                return true;
            }
        }
    }

    if (event->type() == QEvent::KeyPress && m_moleculeView) {
        auto* ke = static_cast<QKeyEvent*>(event);
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
void MainWindow::wireSimulationWorker(SimulationWorker* worker)
{
    if (!worker)
        return;

    if (m_moleculeView) {
        // Claude Generated 2026 - Critical: a new worker run must re-arm the
        // viewer's throttled moleculeUpdated emit. Otherwise the *first* frame
        // of e.g. an MD-after-Opt run is dropped (m_moleculeDirty was still
        // true from the previous run's first frame), the sim dock's m_atoms
        // cache never sees the new geometry, and the new run starts from the
        // stale cache instead of the previous run's final coordinates.
        m_moleculeView->resetSimDirty();

        connect(worker, &SimulationWorker::frameReady,
            m_moleculeView, &MoleculeViewer::updateSimulationFrame,
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
    // Note: loadMoleculeFile() now auto-switches the working directory to
    // the file's parent directory on success, so no explicit switch here.
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

// Claude Generated 2026: Switch the working directory to the file's parent dir
// after a successful load. Extracted as a separate entry point so it can be
// called from any place that loads a molecule file (CLI, File menu, drag-drop).
void MainWindow::setWorkingDirFromArg(const QString& dir)
{
    QString target = dir.isEmpty() ? m_invocationDir : dir;
    if (target.isEmpty() || !QDir(target).exists()) return;
    if (target == m_workingDirectory) return;
    switchWorkingDirectory(target);
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
