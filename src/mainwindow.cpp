#include "settings.h"
#include "selectionmanager.h"  // Claude Generated - Phase 2A
#include "atomlistpanel.h"  // Claude Generated - Phase 2C
#include "sftpmodel.hpp"  // Claude Generated - Remote Directory Mounting
#include "dialogs/sftpdialog.h"  // Claude Generated - Remote Directory Mounting
#include "simulationcontrolwidget.h"  // Claude Generated - Interactive Simulation Integration
// Claude Generated 2026 - Phase 6: SimulationDialog removed; the dock widget is the sole sim UI.
#include <algorithm>  // Claude Generated - for std::min/std::max
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QCompleter>
#include <QDir>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QDockWidget>  // Claude Generated - Phase 2C - For AtomListPanel dock
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QInputDialog>
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
#include "visualizationsettingsdialog.h"

#include "dialogs/nmrspectrumdialog.h"
#include "dialogs/sftpdialog.h"  // Claude Generated - SFTP remote file access
#include "workspacemanager.h"  // Claude Generated Phase 4
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

    // Claude Generated - Quick Fix: Set window title and version
    setWindowTitle("Qurcuma 1.0 - Molecular Visualization");

    setupUI();
    createToolbars();
    createMenus();
    setupConnections();
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
    setCentralWidget(m_moleculeView);

    // Initialize available program commands before creating docks
    initializeProgramCommands();

    // Claude Generated - UI Restructuring: Create all dock widgets
    createDockWidgets();

    // Claude Generated - UI Restructuring: Enable dock features AFTER dock creation
    setDockOptions(QMainWindow::AllowTabbedDocks |
                   QMainWindow::AnimatedDocks |
                   QMainWindow::AllowNestedDocks |
                   QMainWindow::GroupedDragging);

    setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
    setTabPosition(Qt::RightDockWidgetArea, QTabWidget::North);
    setTabPosition(Qt::TopDockWidgetArea, QTabWidget::North);
    setTabPosition(Qt::BottomDockWidgetArea, QTabWidget::North);

    // Setup context menu for file list
    setupContextMenu();

    // Update initial state
    updateRemoteDirectoriesView();

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

    // Claude Generated - Phase 2A: Selection shortcuts
    new QShortcut(Qt::CTRL | Qt::Key_A, this, SLOT(selectAllAtoms()));       // Ctrl+A for select all
    new QShortcut(Qt::Key_Escape, this, SLOT(clearAtomSelection()));          // Escape for clear selection

    // Initial updates
    updatePathLabel(m_workingDirectory);
    updateBookmarkTree();

    // Window settings
    resize(1400, 900);  // Larger default size for flexible docking
    setWindowTitle("Qurcuma");

    // Claude Generated (2026-04) - Dock rewrite: capture baseline after Qt finished
    // placement, then prefer the globally persisted layout from QSettings. Falls
    // back to applyAnalysisLayout() only on first run.
    QTimer::singleShot(0, this, [this]() {
        m_defaultDockState = saveState();
        QSettings uiSettings;
        const QByteArray savedGeometry = uiSettings.value("ui/geometry").toByteArray();
        const QByteArray savedState = uiSettings.value("ui/dockState").toByteArray();
        if (!savedGeometry.isEmpty()) restoreGeometry(savedGeometry);
        if (!savedState.isEmpty()) {
            restoreState(savedState);
        } else {
            applyAnalysisLayout();
        }
    });
}

void MainWindow::createToolbars()
{
    // Claude Generated (2026-04) - Dock rewrite: program controls moved from Top dock
    // into a proper toolbar. Main toolbar carries the full calculation command line.
    QToolBar* toolbar = new QToolBar(tr("Calculation"), this);
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
                        // VTF-Datei im 3D-Viewer laden mit Trajektorie-Support
                        if (m_vtfParser->parseTrajectory(filePath)) {
                            int frameCount = m_vtfParser->getFrameCount();
                            m_moleculeView->setFrameCount(frameCount);
                            
                            VTFParser::VTFFrame frame;
                            if (m_vtfParser->getFrame(0, frame)) {
                                QVector<MoleculeViewer::Atom> atoms;
                                QVector<MoleculeViewer::Bond> bonds;
                                VTFParser::convertToMoleculeViewer(frame, atoms, bonds);
                                m_moleculeView->addMolecule(atoms, bonds);
                                // Claude Generated - Pass to simulation widget for interactive simulation
                                if (m_simulationControlWidget)
                                    m_simulationControlWidget->setMolecule(atoms, bonds);
                                
                                // Frame controls are now managed by MoleculeViewer
                            }
                        }
                    });

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

    // Claude Generated - SFTP: Open Remote File
    QAction *openRemoteAction = fileMenu->addAction(QIcon::fromTheme("folder-remote"), tr("Open &Remote File..."));
    openRemoteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
    connect(openRemoteAction, &QAction::triggered, this, [this]() {
        SftpDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            QString localPath = dialog.getLocalPath();
            if (!localPath.isEmpty()) {
                // Load the downloaded file using existing parsers
                loadMoleculeFile(localPath);
                statusBar()->showMessage(tr("Loaded remote file: %1").arg(QFileInfo(localPath).fileName()), 3000);

                // Update recent connections menu (Claude Generated)
                updateRecentConnectionsMenu();
            }
        }
    });

    // Claude Generated - Phase SFTP Integration: Recent remote connections menu
    m_recentConnectionsMenu = fileMenu->addMenu(QIcon::fromTheme("network-server"), tr("Recent Remote &Connections"));
    m_recentConnectionsMenu->setEnabled(false);
    updateRecentConnectionsMenu();

    fileMenu->addSeparator();

    // Claude Generated - Quick Win: Recent files menu
    m_recentFilesMenu = fileMenu->addMenu(tr("&Recent Files"));
    m_recentFilesMenu->setEnabled(false);

    // Claude Generated Phase 4.5 - Workspace menu
    m_workspaceMenu = fileMenu->addMenu(tr("&Workspaces"));

    QAction *saveWorkspaceAction = m_workspaceMenu->addAction(tr("&Save Current Workspace..."));
    saveWorkspaceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(saveWorkspaceAction, &QAction::triggered, this, &MainWindow::saveCurrentWorkspace);

    QAction *loadWorkspaceAction = m_workspaceMenu->addAction(tr("&Load Workspace..."));
    loadWorkspaceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(loadWorkspaceAction, &QAction::triggered, [this]() {
        // For now, load workspace can be done via the sidebar list
        statusBar()->showMessage(tr("Use workspace list in sidebar to load"), 2000);
    });

    m_workspaceMenu->addSeparator();

    fileMenu->addSeparator();
    // Claude Generated - Visual Polish: Menu icons
    QAction *quitAction = fileMenu->addAction(QIcon::fromTheme("application-exit"), tr("&Quit"));
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    // Claude Generated - Quick Win: Edit Menu with Copy/Paste
    QMenu *editMenu = menuBar->addMenu(tr("&Edit"));

    QAction *copyAction = editMenu->addAction(QIcon::fromTheme("edit-copy"), tr("&Copy Structure"));
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, &MainWindow::copyStructureToClipboard);

    QAction *pasteAction = editMenu->addAction(QIcon::fromTheme("edit-paste"), tr("&Paste Structure"));
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, this, &MainWindow::pasteStructureFromClipboard);

    // Claude Generated - UI Restructuring: View Menu for dock visibility and layout presets
    QMenu *viewMenu = menuBar->addMenu(tr("&View"));

    // Layout Presets submenu
    QMenu *layoutMenu = viewMenu->addMenu(QIcon::fromTheme("view-choose"), tr("&Layout Presets"));

    QAction *visualizationLayoutAction = layoutMenu->addAction(tr("&Visualization Mode"));
    visualizationLayoutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_1));
    visualizationLayoutAction->setToolTip(tr("Focus on 3D viewer (Ctrl+Alt+1)"));
    connect(visualizationLayoutAction, &QAction::triggered, this, &MainWindow::applyVisualizationLayout);

    QAction *editingLayoutAction = layoutMenu->addAction(tr("&Editing Mode"));
    editingLayoutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_2));
    editingLayoutAction->setToolTip(tr("Focus on editors (Ctrl+Alt+2)"));
    connect(editingLayoutAction, &QAction::triggered, this, &MainWindow::applyEditingLayout);

    QAction *calculationLayoutAction = layoutMenu->addAction(tr("&Calculation Mode"));
    calculationLayoutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_3));
    calculationLayoutAction->setToolTip(tr("Focus on calculation workflow (Ctrl+Alt+3)"));
    connect(calculationLayoutAction, &QAction::triggered, this, &MainWindow::applyCalculationLayout);

    QAction *analysisLayoutAction = layoutMenu->addAction(tr("&Analysis Mode (All Panels)"));
    analysisLayoutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_4));
    analysisLayoutAction->setToolTip(tr("Balanced layout with all panels (Ctrl+Alt+4)"));
    connect(analysisLayoutAction, &QAction::triggered, this, &MainWindow::applyAnalysisLayout);

    viewMenu->addSeparator();

    // Claude Generated (2026-04) - Dock rewrite: toggle actions for the 5-dock architecture.
    QMenu *docksMenu = viewMenu->addMenu(QIcon::fromTheme("view-split-left-right"), tr("&Dock Panels"));

    auto addDockToggle = [&docksMenu, this](QDockWidget* dock, const QString& label, const QKeySequence& shortcut = QKeySequence()) {
        if (!dock) return;
        QAction* act = docksMenu->addAction(label);
        act->setCheckable(true);
        act->setChecked(dock->isVisible());
        if (!shortcut.isEmpty()) act->setShortcut(shortcut);
        connect(act, &QAction::toggled, dock, &QDockWidget::setVisible);
        connect(dock, &QDockWidget::visibilityChanged, act, &QAction::setChecked);
    };

    addDockToggle(m_projectDock,          tr("&Project"),            QKeySequence(Qt::CTRL | Qt::Key_B));
    addDockToggle(m_navigationDock,       tr("&Navigation"));
    addDockToggle(m_editorsDock,          tr("&Editors"));
    addDockToggle(m_atomsSimulationDock,  tr("&Atoms && Simulation"));
    addDockToggle(m_outputViewDock,       tr("&Output"));

    viewMenu->addSeparator();

    // Reset layout: restore the captured baseline (drops preset caches so they re-derive).
    QAction *resetLayoutAction = viewMenu->addAction(QIcon::fromTheme("view-restore"), tr("&Reset to Default Layout"));
    resetLayoutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_0));
    connect(resetLayoutAction, &QAction::triggered, this, [this]() {
        if (!m_defaultDockState.isEmpty()) {
            m_presetStates.clear();
            restoreState(m_defaultDockState);
            statusBar()->showMessage(tr("Layout reset to default"), 2000);
        }
    });

    // Settings Menu
    QMenu *settingsMenu = menuBar->addMenu(tr("&Settings"));

    // Claude Generated 2026 - "Use Invocation Directory" preference
    // Checkbox state is updated in the constructor after settings are loaded.
    m_useInvocationDirAction = settingsMenu->addAction(tr("Use &Invocation Directory"));
    m_useInvocationDirAction->setCheckable(true);
    m_useInvocationDirAction->setToolTip(
        tr("If enabled, treat the directory from which qurcuma was launched "
           "as the active Working Directory on each launch."));
    connect(m_useInvocationDirAction, &QAction::triggered,
            this, &MainWindow::toggleUseInvocationDirectory);

    settingsMenu->addSeparator();

    // Claude Generated - Visual Polish: Dark mode toggle (checkbox state set later after loading settings)
    m_darkModeAction = settingsMenu->addAction(tr("&Dark Mode"));
    m_darkModeAction->setCheckable(true);
    // Note: checkbox state will be updated in constructor after loading settings
    connect(m_darkModeAction, &QAction::triggered, this, &MainWindow::toggleDarkMode);

    settingsMenu->addSeparator();

    // Claude Generated - Visualization Settings
    QAction *visSettingsAction = settingsMenu->addAction(QIcon::fromTheme("preferences-desktop"), tr("&Visualization Settings..."));
    connect(visSettingsAction, &QAction::triggered, this, &MainWindow::openVisualizationSettings);

    settingsMenu->addSeparator();
    QAction *configAction = settingsMenu->addAction(QIcon::fromTheme("preferences-system"), tr("Configure Programs..."));
    connect(configAction, &QAction::triggered, this, &MainWindow::configurePrograms);

    // Simulation Menu - Claude Generated - Interactive Simulation Integration
    QMenu *simulationMenu = menuBar->addMenu(tr("&Simulation"));

    // Claude Generated 2026 - Phase 6: menu entries focus the dock instead of opening a dialog.
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
    QAction *mdAction = simulationMenu->addAction(
        QIcon::fromTheme("media-playback-start"), tr("Run &MD Simulation"));
    mdAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    mdAction->setToolTip(tr("Focus the simulation dock in MD mode"));
    connect(mdAction, &QAction::triggered, this,
        [showSimDock]() { showSimDock(SimulationConfig::Mode::MolecularDynamics); });

    QAction *optAction = simulationMenu->addAction(
        QIcon::fromTheme("system-run"), tr("&Geometry Optimization"));
    optAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    optAction->setToolTip(tr("Focus the simulation dock in optimization mode"));
    connect(optAction, &QAction::triggered, this,
        [showSimDock]() { showSimDock(SimulationConfig::Mode::GeometryOptimization); });

    simulationMenu->addSeparator();

    QAction *toggleSimPanelAction = simulationMenu->addAction(tr("Show Simulation &Panel"));
    toggleSimPanelAction->setCheckable(true);
    toggleSimPanelAction->setChecked(true);
    if (m_atomsSimulationDock && m_atomsSimulationTabs) {
        // Claude Generated (2026-04) - Simulation is now a tab inside m_atomsSimulationDock.
        // "Show Panel" both makes the dock visible and activates the Simulation tab (index 1).
        toggleSimPanelAction->setChecked(m_atomsSimulationDock->isVisible());
        connect(toggleSimPanelAction, &QAction::toggled, this, [this](bool on) {
            if (!m_atomsSimulationDock) return;
            m_atomsSimulationDock->setVisible(on);
            if (on && m_atomsSimulationTabs) m_atomsSimulationTabs->setCurrentIndex(1);
        });
        connect(m_atomsSimulationDock, &QDockWidget::visibilityChanged,
            toggleSimPanelAction, &QAction::setChecked);
    }

    // Help Menu - Claude Generated - Quick Fix: About dialog
    QMenu *helpMenu = menuBar->addMenu(tr("&Help"));
    QAction *aboutAction = helpMenu->addAction(tr("&About Qurcuma"));
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
            QString filePath = m_directoryContentModel->filePath(index);
            QString suffix = QFileInfo(filePath).suffix().toLower();
            QString basename = QFileInfo(filePath).baseName();
            if (suffix == "xyz") {
                // XYZ-Datei mit Parser laden
                if (m_xyzParser->parseTrajectory(filePath)) {
                    // Lade XYZ-Daten als Text anzeigen
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
                    // Set trajectory data in molecule viewer
                    // This automatically loads and displays the first frame
                    m_moleculeView->setTrajectoryData(allAtoms, allBonds);

                    // Frame controls are now managed by MoleculeViewer
                } else {
                    // Clear any previous content if parsing fails
                    m_moleculeView->clearScenePublic();
                    qWarning() << "Failed to parse XYZ file:" << filePath;
                }
            }
            else if (suffix == "vtf") {
                // VTF-Datei mit Parser laden
                m_vtfParser = new VTFParser(); // Fresh parser for each file
                if (m_vtfParser->parseTrajectory(filePath)) {
                    // Lade VTF-Daten als Text anzeigen
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
                    // Set trajectory data in molecule viewer
                    // This automatically loads and displays the first frame
                    m_moleculeView->setTrajectoryData(allAtoms, allBonds);

                    // Frame controls are now managed by MoleculeViewer
                } else {
                    // Clear any previous content if parsing fails
                    m_moleculeView->clearScenePublic();
                    qWarning() << "Failed to parse VTF file:" << filePath;
                }
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
    connect(m_bookmarkTreeView, &QTreeWidget::itemClicked,
        this, &MainWindow::onBookmarkItemClicked);

    connect(m_bookmarkTreeView, &QTreeWidget::customContextMenuRequested,
        this, &MainWindow::onBookmarkContextMenu);

    // Claude Generated Phase 4.3 - Workspace list signals
    if (m_workspaceListView) {
        connect(m_workspaceListView, &QListWidget::itemClicked,
            this, &MainWindow::onWorkspaceItemClicked);

        connect(m_workspaceListView, &QListWidget::customContextMenuRequested,
            this, &MainWindow::onWorkspaceContextMenu);
    }

    connect(m_bookmarkButton, &QToolButton::clicked, [this]() {
        if (!m_workingDirectory.isEmpty()) {
            m_settings.addWorkingDirectory(m_workingDirectory);
            updateBookmarkTree();
            statusBar()->showMessage(tr("Directory bookmarked: %1")
                                         .arg(QDir(m_workingDirectory).dirName()),
                3000);
        }
    });

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

    // Claude Generated - Quick Win: Copy/Paste structures shortcut
    new QShortcut(QKeySequence::Paste, this, SLOT(pasteStructureFromClipboard()));

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
                m_settings.addWorkingDirectory(path);
                updateBookmarkTree();
                statusBar()->showMessage(tr("Directory bookmarked: %1")
                                             .arg(QDir(path).dirName()),
                    3000);
            });

            QAction* setWorkDirAction = contextMenu.addAction(tr("Set as Working Directory"));
            connect(setWorkDirAction, &QAction::triggered, [this, path]() {
                switchWorkingDirectory(path);
                m_settings.addWorkingDirectory(path);
                updateBookmarkTree();
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
    updateBookmarkTree();
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
void MainWindow::updateBookmarkTree()
{
    m_bookmarkTreeView->clear();

    // Load bookmarks from settings
    QVector<Settings::BookmarkItem> allBookmarks = m_settings.bookmarks();

    // Build a map for quick parent lookup
    QMap<QString, QTreeWidgetItem*> itemsById;
    itemsById[""] = m_bookmarkTreeView->invisibleRootItem();  // Root

    // First pass: Create all items
    for (const auto& bm : allBookmarks) {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, bm.name);
        item->setData(0, Qt::UserRole, bm.id);  // Store ID
        item->setData(0, Qt::UserRole + 1, bm.path);  // Store path

        // Set icons
        if (bm.isFolder) {
            item->setIcon(0, QIcon::fromTheme("folder", QIcon(":/icons/folder.png")));
        } else {
            item->setIcon(0, QIcon::fromTheme("bookmark", QIcon(":/icons/bookmark.png")));
            // Claude Generated Phase 3.5 - Show path and tags in tooltip
            QString tooltip = bm.path;
            if (!bm.tags.isEmpty()) {
                tooltip += "\nTags: " + bm.tags.join(", ");
            }
            item->setToolTip(0, tooltip);
        }

        // Set color if defined
        if (bm.color.isValid()) {
            item->setBackground(0, bm.color);
        }

        itemsById[bm.id] = item;
    }

    // Second pass: Add items to their parents
    for (const auto& bm : allBookmarks) {
        if (itemsById.contains(bm.id)) {
            QTreeWidgetItem* parentItem = itemsById.value(bm.parentId, m_bookmarkTreeView->invisibleRootItem());
            if (parentItem && parentItem != itemsById[bm.id]) {  // Avoid adding to self
                parentItem->addChild(itemsById[bm.id]);
            }
        }
    }

    // Expand all folders by default
    m_bookmarkTreeView->expandAll();
}

// Claude Generated Phase 3.2 - Handle bookmark tree item clicks
void MainWindow::onBookmarkItemClicked(QTreeWidgetItem* item, int column)
{
    if (!item) return;

    // Get path from item data
    QString path = item->data(0, Qt::UserRole + 1).toString();

    // Only navigate if it's a bookmark (has a path), not a folder
    if (!path.isEmpty()) {
        switchWorkingDirectory(path);
    }
}

// Claude Generated Phase 3.2 - Handle bookmark context menu
void MainWindow::onBookmarkContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_bookmarkTreeView->itemAt(pos);

    QMenu contextMenu(this);

    if (!item) {
        // Empty area context menu
        QAction* addAction = contextMenu.addAction(tr("Add Current Directory as Bookmark"));
        connect(addAction, &QAction::triggered, [this]() {
            if (!m_workingDirectory.isEmpty()) {
                Settings::BookmarkItem bm;
                bm.id = QUuid::createUuid().toString();
                bm.name = QDir(m_workingDirectory).dirName();
                bm.path = m_workingDirectory;
                bm.isFolder = false;
                bm.parentId = "";
                bm.created = QDateTime::currentDateTime();
                m_settings.addBookmark(bm);
                updateBookmarkTree();
                statusBar()->showMessage(tr("Bookmark added"), 2000);
            }
        });

        QAction* folderAction = contextMenu.addAction(tr("New Folder..."));
        connect(folderAction, &QAction::triggered, [this]() {
            QString name = QInputDialog::getText(this,
                tr("New Bookmark Folder"),
                tr("Folder name:"));
            if (!name.isEmpty()) {
                Settings::BookmarkItem folder;
                folder.id = QUuid::createUuid().toString();
                folder.name = name;
                folder.path = "";
                folder.isFolder = true;
                folder.parentId = "";
                folder.created = QDateTime::currentDateTime();
                m_settings.addBookmark(folder);
                updateBookmarkTree();
            }
        });
    } else {
        // Item-specific context menu
        QString itemId = item->data(0, Qt::UserRole).toString();
        Settings::BookmarkItem bm = [this, itemId]() {
            for (const auto& b : m_settings.bookmarks()) {
                if (b.id == itemId) return b;
            }
            return Settings::BookmarkItem();
        }();

        if (bm.isFolder) {
            // Folder options
            QAction* renameAction = contextMenu.addAction(tr("Rename..."));
            connect(renameAction, &QAction::triggered, [this, itemId, item]() {
                QString newName = QInputDialog::getText(this,
                    tr("Rename Folder"),
                    tr("New name:"),
                    QLineEdit::Normal,
                    item->text(0));
                if (!newName.isEmpty()) {
                    auto bookmarks = m_settings.bookmarks();
                    for (auto& b : bookmarks) {
                        if (b.id == itemId) {
                            b.name = newName;
                            m_settings.updateBookmark(itemId, b);
                            break;
                        }
                    }
                    updateBookmarkTree();
                }
            });

            QAction* deleteAction = contextMenu.addAction(tr("Delete Folder"));
            connect(deleteAction, &QAction::triggered, [this, itemId]() {
                m_settings.removeBookmark(itemId);
                updateBookmarkTree();
            });
        } else {
            // Bookmark options
            QAction* openAction = contextMenu.addAction(tr("Open"));
            connect(openAction, &QAction::triggered, [this, bm]() {
                if (!bm.path.isEmpty()) {
                    switchWorkingDirectory(bm.path);
                }
            });

            contextMenu.addSeparator();

            QAction* renameAction = contextMenu.addAction(tr("Rename..."));
            connect(renameAction, &QAction::triggered, [this, itemId, item]() {
                QString newName = QInputDialog::getText(this,
                    tr("Rename Bookmark"),
                    tr("New name:"),
                    QLineEdit::Normal,
                    item->text(0));
                if (!newName.isEmpty()) {
                    auto bookmarks = m_settings.bookmarks();
                    for (auto& b : bookmarks) {
                        if (b.id == itemId) {
                            b.name = newName;
                            m_settings.updateBookmark(itemId, b);
                            break;
                        }
                    }
                    updateBookmarkTree();
                }
            });

            // Claude Generated Phase 3.5 - Minimal tag system
            QAction* tagsAction = contextMenu.addAction(tr("Edit Tags..."));
            connect(tagsAction, &QAction::triggered, [this, itemId]() {
                auto bookmarks = m_settings.bookmarks();
                Settings::BookmarkItem* targetBm = nullptr;
                for (auto& b : bookmarks) {
                    if (b.id == itemId) {
                        targetBm = &b;
                        break;
                    }
                }

                if (targetBm) {
                    QString tagsStr = targetBm->tags.join(", ");
                    QString newTagsStr = QInputDialog::getText(this,
                        tr("Edit Bookmark Tags"),
                        tr("Tags (comma-separated, e.g. #dft, #md):"),
                        QLineEdit::Normal,
                        tagsStr);

                    if (!newTagsStr.isEmpty() || !tagsStr.isEmpty()) {
                        targetBm->tags = newTagsStr.split(",", Qt::SkipEmptyParts);
                        for (auto& tag : targetBm->tags) {
                            tag = tag.trimmed();
                        }
                        m_settings.updateBookmark(itemId, *targetBm);
                        updateBookmarkTree();
                    }
                }
            });

            QAction* removeAction = contextMenu.addAction(tr("Remove Bookmark"));
            connect(removeAction, &QAction::triggered, [this, itemId]() {
                m_settings.removeBookmark(itemId);
                updateBookmarkTree();
                statusBar()->showMessage(tr("Bookmark removed"), 2000);
            });
        }
    }

    contextMenu.exec(m_bookmarkTreeView->viewport()->mapToGlobal(pos));
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
// with Project so hiding the group hides both.
void MainWindow::toggleLeftPanel()
{
    if (m_projectDock) {
        const bool show = !m_projectDock->isVisible();
        m_projectDock->setVisible(show);
        if (m_navigationDock) m_navigationDock->setVisible(show);
        statusBar()->showMessage(
            show ? tr("Project panel shown") : tr("Project panel hidden"),
            1500);
    }
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
void MainWindow::openVisualizationSettings()
{
    if (!m_moleculeView) {
        QMessageBox::warning(this, tr("No Viewer"), tr("Molecule viewer is not available."));
        return;
    }

    // Claude Generated - Pass Settings object for persistence
    // Store dialog pointer for shortcut synchronization (Fix: Shortcuts sync with dialog)
    m_visualizationDialog = new VisualizationSettingsDialog(m_moleculeView, &m_settings, this);
    m_visualizationDialog->setAttribute(Qt::WA_DeleteOnClose);

    // Claude Generated 2026 - "Use Invocation Directory" live toggle from the dialog.
    // Routes the dialog checkbox's toggled signal through the same helper as the
    // menu action, so the menu and dialog stay in sync and the same validation
    // / restoration logic applies to both entry points.
    connect(m_visualizationDialog,
            &VisualizationSettingsDialog::useInvocationDirectoryToggled,
            this, [this](bool enabled) {
        applyUseInvocationDirectoryState(enabled);
    });

    m_visualizationDialog->show();
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
    // Synchronize dialog widgets with current viewer state when shortcut is used
    // This ensures dialog shows correct values even if shortcuts were used while dialog open
    if (m_visualizationDialog && m_visualizationDialog->isVisible()) {
        m_visualizationDialog->loadCurrentSettings();
    }
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
    }

    if (!ws.windowGeometry.isEmpty()) {
        restoreGeometry(ws.windowGeometry);
    }

    // Claude Generated - UI Restructuring: Restore dock widget layout
    if (!ws.dockState.isEmpty()) {
        restoreState(ws.dockState);
    } else {
        // Fallback: Apply default layout if no dock state saved (backward compatibility)
        applyAnalysisLayout();
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

            // Claude Generated - Feed the loaded molecule into the inline
            // simulation widget so the user can start a run without manually
            // re-selecting it. First frame is used as the simulation input.
            if (m_simulationControlWidget && !allAtoms.isEmpty())
                m_simulationControlWidget->setMolecule(allAtoms.first(),
                    !allBonds.isEmpty() ? allBonds.first() : QVector<MoleculeViewer::Bond>{});
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
            fileLoaded = true;
        } else {
            m_moleculeView->clearScenePublic();
            qWarning() << "Failed to parse XYZ file:" << filePath;
        }
    }
    else if (suffix == "vtf") {
        // VTF file loading
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

            // Claude Generated - Feed first frame into the simulation widget.
            if (m_simulationControlWidget && !allAtoms.isEmpty())
                m_simulationControlWidget->setMolecule(allAtoms.first(),
                    !allBonds.isEmpty() ? allBonds.first() : QVector<MoleculeViewer::Bond>{});
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
    }
}

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

// Claude Generated (2026-04) - Dock architecture rewrite. Five focused docks rahmen
// the MoleculeViewer (CentralWidget). Internal QTabWidgets replace fragile Qt
// tabifyDockWidget chains for editors and atoms/simulation, eliminating the
// drift bug where preset switches stacked redundant tab groupings.
void MainWindow::createDockWidgets()
{
    // ==================== PROJECT DOCK (left) ====================
    // Working dir + breadcrumb + calculation directory list + calculation file list
    m_projectDock = new QDockWidget(tr("Project"), this);
    m_projectDock->setObjectName("ProjectDock");

    QWidget* projectWidget = new QWidget;
    QVBoxLayout* projectLayout = new QVBoxLayout(projectWidget);
    projectLayout->setContentsMargins(4, 4, 4, 4);
    projectLayout->setSpacing(4);

    m_chooseDirectory = new QPushButton(tr("Choose Working Directory"));
    m_chooseDirectory->setIcon(QIcon::fromTheme("folder-open", QIcon(":/icons/folder.png")));
    projectLayout->addWidget(m_chooseDirectory);

    m_breadcrumbBar = new BreadcrumbBar;
    m_breadcrumbBar->setHomeDirectory(QDir::homePath());
    connect(m_breadcrumbBar, &BreadcrumbBar::pathSelected, this, &MainWindow::switchWorkingDirectory);
    projectLayout->addWidget(m_breadcrumbBar);

    // Splitter: upper = calculation directories list, lower = files inside selected dir
    QSplitter* projectSplitter = new QSplitter(Qt::Vertical);

    QWidget* calcDirsWidget = new QWidget;
    QVBoxLayout* calcDirsLayout = new QVBoxLayout(calcDirsWidget);
    calcDirsLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* dirListLabel = new QLabel(tr("Calculation Directories"));
    dirListLabel->setStyleSheet("font-weight: bold;");
    calcDirsLayout->addWidget(dirListLabel);
    m_projectListView = new QListView;
    m_projectModel = new QFileSystemModel(this);
    m_projectModel->setRootPath(m_workingDirectory);
    m_projectModel->setFilter(QDir::AllDirs | QDir::NoDot);
    m_projectModel->setReadOnly(true);
    m_projectListView->setModel(m_projectModel);
    m_projectListView->setRootIndex(m_projectModel->index(m_workingDirectory));
    calcDirsLayout->addWidget(m_projectListView);
    m_newCalculationButton = new QPushButton(tr("+ New Calculation Directory"));
    m_newCalculationButton->setIcon(QIcon::fromTheme("folder-new", QIcon()));
    m_newCalculationButton->setToolTip(tr("Create a new calculation directory (Ctrl+N)"));
    calcDirsLayout->addWidget(m_newCalculationButton);
    projectSplitter->addWidget(calcDirsWidget);

    QWidget* calcFilesWidget = new QWidget;
    QVBoxLayout* calcFilesLayout = new QVBoxLayout(calcFilesWidget);
    calcFilesLayout->setContentsMargins(0, 0, 0, 0);

    QWidget* pathWidget = new QWidget;
    QHBoxLayout* pathLayout = new QHBoxLayout(pathWidget);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    m_currentProjectLabel = new QLabel(m_currentCalculationDir);
    m_currentProjectLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_currentProjectLabel->setWordWrap(false);
    pathLayout->addWidget(m_currentProjectLabel, 1);
    QPushButton* copyPathButton = new QPushButton;
    copyPathButton->setIcon(QIcon::fromTheme("edit-copy"));
    copyPathButton->setToolTip(tr("Copy current path to clipboard"));
    copyPathButton->setMaximumWidth(30);
    connect(copyPathButton, &QPushButton::clicked, this, &MainWindow::copyCurrentPath);
    pathLayout->addWidget(copyPathButton);
    calcFilesLayout->addWidget(pathWidget);

    QWidget* stateWidget = new QWidget;
    QHBoxLayout* stateLayout = new QHBoxLayout(stateWidget);
    stateLayout->setContentsMargins(0, 0, 0, 0);
    m_stateIcon = new QLabel("●");
    m_stateIcon->setStyleSheet("color: grey; font-size: 14px;");
    m_stateIcon->setFixedWidth(20);
    stateLayout->addWidget(m_stateIcon);
    m_stateIndicator = new QLabel(tr("No directory selected"));
    stateLayout->addWidget(m_stateIndicator);
    stateLayout->addStretch();
    calcFilesLayout->addWidget(stateWidget);

    m_directoryContentView = new QListView;
    m_directoryContentModel = new QFileSystemModel(this);
    m_directoryContentModel->setFilter(QDir::NoDotAndDotDot | QDir::Files);
    m_directoryContentModel->setNameFilters(QStringList()
        << "*.xyz" << "*.vtf" << "*.pdb" << "*.mol2" << "*.inp" << "*.log" << "*.out"
        << "*.hess" << "*.gbw" << "*.txt" << "*.*" << "input");
    m_directoryContentModel->setNameFilterDisables(false);
    m_directoryContentView->setModel(m_directoryContentModel);
    m_directoryContentView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_directoryContentView, &QListView::doubleClicked, this, &MainWindow::onRemoteFileDoubleClicked);
    calcFilesLayout->addWidget(m_directoryContentView);
    projectSplitter->addWidget(calcFilesWidget);

    projectSplitter->setStretchFactor(0, 1);
    projectSplitter->setStretchFactor(1, 2);
    projectLayout->addWidget(projectSplitter, 1);

    m_projectDock->setWidget(projectWidget);

    // ==================== NAVIGATION DOCK (left, tabbed with Project) ====================
    // Bookmarks / Workspaces / Remote Directories as internal tabs.
    m_navigationDock = new QDockWidget(tr("Navigation"), this);
    m_navigationDock->setObjectName("NavigationDock");
    m_navigationTabs = new QTabWidget;
    m_navigationTabs->setTabPosition(QTabWidget::North);
    m_navigationTabs->setDocumentMode(true);

    // Bookmarks tab
    QWidget* bookmarksWidget = new QWidget;
    QVBoxLayout* bookmarksLayout = new QVBoxLayout(bookmarksWidget);
    bookmarksLayout->setContentsMargins(4, 4, 4, 4);
    QWidget* bookmarkHeader = new QWidget;
    QHBoxLayout* bookmarkHeaderLayout = new QHBoxLayout(bookmarkHeader);
    bookmarkHeaderLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* bookmarksLabel = new QLabel(tr("Bookmarks"));
    bookmarksLabel->setStyleSheet("font-weight: bold;");
    bookmarkHeaderLayout->addWidget(bookmarksLabel, 1);
    m_bookmarkButton = new QToolButton;
    m_bookmarkButton->setIcon(QIcon::fromTheme("bookmark-new", QIcon(":/icons/bookmark.png")));
    m_bookmarkButton->setToolTip(tr("Bookmark current directory"));
    bookmarkHeaderLayout->addWidget(m_bookmarkButton);
    bookmarksLayout->addWidget(bookmarkHeader);
    m_bookmarkTreeView = new QTreeWidget;
    m_bookmarkTreeView->setHeaderLabels(QStringList() << tr("Bookmarks"));
    m_bookmarkTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_bookmarkTreeView->setDragDropMode(QAbstractItemView::InternalMove);
    bookmarksLayout->addWidget(m_bookmarkTreeView);
    m_navigationTabs->addTab(bookmarksWidget, tr("Bookmarks"));

    // Workspaces tab
    QWidget* workspacesWidget = new QWidget;
    QVBoxLayout* workspacesLayout = new QVBoxLayout(workspacesWidget);
    workspacesLayout->setContentsMargins(4, 4, 4, 4);
    QWidget* workspaceHeader = new QWidget;
    QHBoxLayout* wsHeaderLayout = new QHBoxLayout(workspaceHeader);
    wsHeaderLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* workspacesLabel = new QLabel(tr("Workspaces"));
    workspacesLabel->setStyleSheet("font-weight: bold;");
    wsHeaderLayout->addWidget(workspacesLabel, 1);
    QPushButton* newWorkspaceButton = new QPushButton("+");
    newWorkspaceButton->setMaximumWidth(30);
    newWorkspaceButton->setToolTip(tr("Save current state as workspace"));
    connect(newWorkspaceButton, &QPushButton::clicked, this, &MainWindow::saveCurrentWorkspace);
    wsHeaderLayout->addWidget(newWorkspaceButton);
    workspacesLayout->addWidget(workspaceHeader);
    m_workspaceListView = new QListWidget;
    m_workspaceListView->setContextMenuPolicy(Qt::CustomContextMenu);
    workspacesLayout->addWidget(m_workspaceListView);
    m_navigationTabs->addTab(workspacesWidget, tr("Workspaces"));

    // Remote tab
    QWidget* remoteWidget = new QWidget;
    QVBoxLayout* remoteLayout = new QVBoxLayout(remoteWidget);
    remoteLayout->setContentsMargins(4, 4, 4, 4);
    QWidget* remoteHeader = new QWidget;
    QHBoxLayout* remoteHeaderLayout = new QHBoxLayout(remoteHeader);
    remoteHeaderLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* remoteLabel = new QLabel(tr("Remote Directories"));
    remoteLabel->setStyleSheet("font-weight: bold;");
    remoteHeaderLayout->addWidget(remoteLabel, 1);
    QPushButton* addRemoteBtn = new QPushButton("+");
    addRemoteBtn->setMaximumWidth(30);
    addRemoteBtn->setToolTip(tr("Add remote directory"));
    connect(addRemoteBtn, &QPushButton::clicked, this, &MainWindow::onAddRemoteDirectoryClicked);
    remoteHeaderLayout->addWidget(addRemoteBtn);
    remoteLayout->addWidget(remoteHeader);
    m_remoteDirectoriesView = new QTreeWidget;
    m_remoteDirectoriesView->setHeaderHidden(true);
    connect(m_remoteDirectoriesView, &QTreeWidget::itemClicked, this, &MainWindow::onRemoteDirectoryClicked);
    remoteLayout->addWidget(m_remoteDirectoriesView);
    m_navigationTabs->addTab(remoteWidget, tr("Remote"));

    m_navigationDock->setWidget(m_navigationTabs);

    // ==================== EDITORS DOCK (right, top) ====================
    // Structure + Input as internal tabs — avoids Qt's fragile dock-tabify churn.
    m_editorsDock = new QDockWidget(tr("Editors"), this);
    m_editorsDock->setObjectName("EditorsDock");
    m_editorsTabs = new QTabWidget;
    m_editorsTabs->setTabPosition(QTabWidget::North);
    m_editorsTabs->setDocumentMode(true);

    QWidget* structureWidget = new QWidget;
    QVBoxLayout* structureLayout = new QVBoxLayout(structureWidget);
    structureLayout->setContentsMargins(4, 4, 4, 4);
    QHBoxLayout* structureFileLayout = new QHBoxLayout;
    structureFileLayout->addWidget(new QLabel(tr("Structure file:")));
    m_structureFileEdit = new QLineEdit("input");
    m_structureFileEdit->setToolTip(tr("Base name for structure file"));
    m_structureFileEditExtension = new QLineEdit("xyz");
    m_structureFileEditExtension->setMaximumWidth(60);
    structureFileLayout->addWidget(m_structureFileEdit);
    structureFileLayout->addWidget(m_structureFileEditExtension);
    structureLayout->addLayout(structureFileLayout);
    m_structureView = new ModifiableTextEdit;
    m_structureView->setPlaceholderText("Structure data");
    structureLayout->addWidget(m_structureView);
    m_editorsTabs->addTab(structureWidget, tr("Structure"));

    QWidget* inputWidget = new QWidget;
    QVBoxLayout* inputLayout = new QVBoxLayout(inputWidget);
    inputLayout->setContentsMargins(4, 4, 4, 4);
    QHBoxLayout* inputFileLayout = new QHBoxLayout;
    inputFileLayout->addWidget(new QLabel(tr("Input file:")));
    m_inputFileEdit = new QLineEdit("input");
    m_inputFileEdit->setToolTip(tr("Base name for input file"));
    m_inputFileEditExtension = new QLineEdit("");
    m_inputFileEditExtension->setMaximumWidth(60);
    inputFileLayout->addWidget(m_inputFileEdit);
    inputFileLayout->addWidget(m_inputFileEditExtension);
    inputLayout->addLayout(inputFileLayout);
    m_inputView = new ModifiableTextEdit;
    m_inputView->setPlaceholderText("Input data");
    inputLayout->addWidget(m_inputView);
    m_editorsTabs->addTab(inputWidget, tr("Input"));

    m_editorsDock->setWidget(m_editorsTabs);

    // ==================== ATOMS & SIMULATION DOCK (right, below Editors) ====================
    m_atomsSimulationDock = new QDockWidget(tr("Atoms && Simulation"), this);
    m_atomsSimulationDock->setObjectName("AtomsSimulationDock");
    m_atomsSimulationTabs = new QTabWidget;
    m_atomsSimulationTabs->setTabPosition(QTabWidget::North);
    m_atomsSimulationTabs->setDocumentMode(true);

    m_atomListPanel = new AtomListPanel(this);
    m_atomsSimulationTabs->addTab(m_atomListPanel, tr("Atoms"));

    m_simulationControlWidget = new SimulationControlWidget(this);
    m_atomsSimulationTabs->addTab(m_simulationControlWidget, tr("Simulation"));

    m_atomsSimulationDock->setWidget(m_atomsSimulationTabs);

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
    connect(m_simulationControlWidget, &SimulationControlWidget::workerStarted,
        this, &MainWindow::wireSimulationWorker);
    connect(m_simulationControlWidget, &SimulationControlWidget::configChanged,
        this, &MainWindow::onSimulationConfigChanged);
    // Claude Generated 2026 - In-dock "Save" button routes to the central save.
    connect(m_simulationControlWidget, &SimulationControlWidget::saveStructureRequested,
        this, [this]() { saveCurrentStructure(); });
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
    m_outputViewDock = new QDockWidget(tr("Output"), this);
    m_outputViewDock->setObjectName("OutputViewDock");
    QWidget* outputWidget = new QWidget;
    QVBoxLayout* outputLayout = new QVBoxLayout(outputWidget);
    outputLayout->setContentsMargins(4, 4, 4, 4);
    QHBoxLayout* outputHeaderLayout = new QHBoxLayout;
    QLabel* outputLabel = new QLabel(tr("Output"));
    outputLabel->setStyleSheet("font-weight: bold;");
    outputHeaderLayout->addWidget(outputLabel);
    outputHeaderLayout->addStretch();
    QPushButton* clearOutputButton = new QPushButton;
    clearOutputButton->setIcon(QIcon::fromTheme("edit-clear"));
    clearOutputButton->setToolTip(tr("Clear output (Ctrl+L)"));
    clearOutputButton->setMaximumWidth(30);
    connect(clearOutputButton, &QPushButton::clicked, this, &MainWindow::clearOutputView);
    outputHeaderLayout->addWidget(clearOutputButton);
    outputLayout->addLayout(outputHeaderLayout);
    m_outputView = new QTextEdit;
    m_outputView->setPlaceholderText("Output");
    m_outputView->setReadOnly(true);
    outputLayout->addWidget(m_outputView);
    m_outputViewDock->setWidget(outputWidget);

    // ==================== INITIAL PLACEMENT ====================
    // Baseline layout. addDockWidget called exactly once per dock. Project and
    // Navigation share the left area via tabifyDockWidget (stable: only two
    // peers). Editors and Atoms&Simulation split vertically on the right — not
    // tabified, so clicking one never hides the other.
    addDockWidget(Qt::LeftDockWidgetArea, m_projectDock);
    addDockWidget(Qt::LeftDockWidgetArea, m_navigationDock);
    tabifyDockWidget(m_projectDock, m_navigationDock);
    m_projectDock->raise();

    addDockWidget(Qt::RightDockWidgetArea, m_editorsDock);
    addDockWidget(Qt::RightDockWidgetArea, m_atomsSimulationDock);
    splitDockWidget(m_editorsDock, m_atomsSimulationDock, Qt::Vertical);

    addDockWidget(Qt::BottomDockWidgetArea, m_outputViewDock);
}

// Claude Generated - UI Restructuring: Layout preset dispatcher
void MainWindow::applyLayoutPreset(LayoutPreset preset)
{
    switch (preset) {
        case LayoutPreset::Visualization: applyVisualizationLayout(); break;
        case LayoutPreset::Editing:       applyEditingLayout();       break;
        case LayoutPreset::Calculation:   applyCalculationLayout();   break;
        case LayoutPreset::Analysis:      applyAnalysisLayout();      break;
    }
}

// Claude Generated (2026-04) - Preset lazy-cache: first call sets visibility and
// caches saveState(); subsequent calls simply restoreState(). Eliminates the
// drift caused by repeated tabify/split calls.
static inline void applyPresetVisibility(QDockWidget* dock, bool visible) {
    if (dock) dock->setVisible(visible);
}

void MainWindow::applyVisualizationLayout()
{
    const int key = static_cast<int>(LayoutPreset::Visualization);
    auto it = m_presetStates.find(key);
    if (it != m_presetStates.end()) {
        restoreState(*it);
    } else {
        applyPresetVisibility(m_projectDock, true);
        applyPresetVisibility(m_navigationDock, false);
        applyPresetVisibility(m_editorsDock, false);
        applyPresetVisibility(m_atomsSimulationDock, true);
        applyPresetVisibility(m_outputViewDock, false);
        if (m_atomsSimulationTabs) m_atomsSimulationTabs->setCurrentIndex(0);
        if (width() > 0) {
            resizeDocks({m_projectDock, m_atomsSimulationDock},
                        {int(width() * 0.18), int(width() * 0.22)},
                        Qt::Horizontal);
        }
        m_presetStates.insert(key, saveState());
    }
    statusBar()->showMessage(tr("Layout: Visualization Mode"), 2000);
}

void MainWindow::applyEditingLayout()
{
    const int key = static_cast<int>(LayoutPreset::Editing);
    auto it = m_presetStates.find(key);
    if (it != m_presetStates.end()) {
        restoreState(*it);
    } else {
        applyPresetVisibility(m_projectDock, true);
        applyPresetVisibility(m_navigationDock, true);
        applyPresetVisibility(m_editorsDock, true);
        applyPresetVisibility(m_atomsSimulationDock, false);
        applyPresetVisibility(m_outputViewDock, false);
        if (m_editorsTabs) m_editorsTabs->setCurrentIndex(0);
        if (width() > 0) {
            resizeDocks({m_projectDock, m_editorsDock},
                        {int(width() * 0.22), int(width() * 0.32)},
                        Qt::Horizontal);
        }
        m_presetStates.insert(key, saveState());
    }
    statusBar()->showMessage(tr("Layout: Editing Mode"), 2000);
}

void MainWindow::applyCalculationLayout()
{
    const int key = static_cast<int>(LayoutPreset::Calculation);
    auto it = m_presetStates.find(key);
    if (it != m_presetStates.end()) {
        restoreState(*it);
    } else {
        applyPresetVisibility(m_projectDock, true);
        applyPresetVisibility(m_navigationDock, false);
        applyPresetVisibility(m_editorsDock, true);
        applyPresetVisibility(m_atomsSimulationDock, false);
        applyPresetVisibility(m_outputViewDock, true);
        if (height() > 0) {
            resizeDocks({m_outputViewDock}, {int(height() * 0.35)}, Qt::Vertical);
        }
        m_presetStates.insert(key, saveState());
    }
    statusBar()->showMessage(tr("Layout: Calculation Mode"), 2000);
}

void MainWindow::applyAnalysisLayout()
{
    const int key = static_cast<int>(LayoutPreset::Analysis);
    auto it = m_presetStates.find(key);
    if (it != m_presetStates.end()) {
        restoreState(*it);
    } else {
        applyPresetVisibility(m_projectDock, true);
        applyPresetVisibility(m_navigationDock, true);
        applyPresetVisibility(m_editorsDock, true);
        applyPresetVisibility(m_atomsSimulationDock, true);
        applyPresetVisibility(m_outputViewDock, true);
        if (width() > 0) {
            resizeDocks({m_projectDock, m_editorsDock},
                        {int(width() * 0.22), int(width() * 0.28)},
                        Qt::Horizontal);
        }
        if (height() > 0) {
            resizeDocks({m_outputViewDock}, {int(height() * 0.22)}, Qt::Vertical);
        }
        m_presetStates.insert(key, saveState());
    }
    statusBar()->showMessage(tr("Layout: Analysis Mode (All Panels)"), 2000);
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

// Claude Generated - Interactive Simulation Integration

// Claude Generated - Keep shared config in sync when dock widget controls change
void MainWindow::onSimulationConfigChanged(SimulationConfig cfg)
{
    m_simulationConfig = cfg;
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
