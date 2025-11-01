#include "settings.h"
#include "selectionmanager.h"  // Claude Generated - Phase 2A
#include <algorithm>  // Claude Generated - for std::min/std::max
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QCompleter>
#include <QDir>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDirIterator>
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
#include <QScrollBar>
#include <QShortcut>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringListModel>
#include <QSysInfo>
#include <QThread>
#include <QTime>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include <QFile>
#include <QTextStream>
#include <QString>
#include "view.h"
#include "frequencydialog.h"
#include "visualizationsettingsdialog.h"

#include "dialogs/nmrspectrumdialog.h"
#include "workspacemanager.h"  // Claude Generated Phase 4
#include "mainwindow.h"

// Claude Generated - Conditional debug logging
#ifdef QT_DEBUG
#define DEBUG_LOG qDebug()
#else
#define DEBUG_LOG if(false) qDebug()
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
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

    m_nmrDialog = new NMRSpectrumDialog(this);
    m_vtfParser = new VTFParser();
    m_xyzParser = new XYZParser();

    // Arbeitsverzeichnis aus Settings laden
    m_workingDirectory = m_settings.workingDirectory();
    if (!m_workingDirectory.isEmpty()) {
        m_projectModel->setRootPath(m_workingDirectory);
        m_projectListView->setRootIndex(m_projectModel->index(m_workingDirectory));
    }
    QString lastDir = m_settings.lastUsedWorkingDirectory();
    if (!lastDir.isEmpty() && QDir(lastDir).exists()) {
        switchWorkingDirectory(lastDir);
    }

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
    //delete m_currentProcess;
}

void MainWindow::setupUI()
{
    // Claude Generated - Quick Win: Enable drag and drop
    setAcceptDrops(true);

    // Create main widget
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    // Create main splitter for flexible sizing
    m_splitter = new QSplitter(Qt::Horizontal);
    mainLayout->addWidget(m_splitter);

    // Left side: File browser and bookmarks
    QWidget *leftWidget = new QWidget;
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setSpacing(5);

    // Current directory widget with bookmark button
    QWidget* currentDirWidget = new QWidget;
    QHBoxLayout* currentDirLayout = new QHBoxLayout(currentDirWidget);
    currentDirLayout->setContentsMargins(0, 0, 0, 0);

    // Directory icon - Claude Generated Phase 2.1
    m_chooseDirectory = new QPushButton(tr("Choose Working Directory"));
    m_chooseDirectory->setIcon(QIcon::fromTheme("folder-open", QIcon(":/icons/folder.png")));
    currentDirLayout->addWidget(m_chooseDirectory);

    // Claude Generated Phase 1 - Breadcrumb navigation bar
    m_breadcrumbBar = new BreadcrumbBar;
    m_breadcrumbBar->setHomeDirectory(QDir::homePath());
    connect(m_breadcrumbBar, &BreadcrumbBar::pathSelected, this, &MainWindow::switchWorkingDirectory);
    currentDirLayout->addWidget(m_breadcrumbBar, 1);

    // Bookmark button
    m_bookmarkButton = new QToolButton;
    m_bookmarkButton->setIcon(QIcon::fromTheme("bookmark-new", QIcon(":/icons/bookmark.png")));
    m_bookmarkButton->setToolTip(tr("Bookmark current directory"));
    currentDirLayout->addWidget(m_bookmarkButton);

    leftLayout->addWidget(currentDirWidget);

    // Separator line
    QFrame* line1 = new QFrame;
    line1->setFrameShape(QFrame::HLine);
    line1->setFrameShadow(QFrame::Sunken);
    leftLayout->addWidget(line1);

    // Directory content section
    // Claude Generated - Phase 3.1: Renamed for clarity and added tooltip
    QLabel* dirListLabel = new QLabel(tr("Calculation Directories"));
    dirListLabel->setStyleSheet("font-weight: bold;");
    dirListLabel->setToolTip(tr("Subdirectories for individual calculations"));
    leftLayout->addWidget(dirListLabel);

    // Project list view setup
    m_projectListView = new QListView;
    m_projectModel = new QFileSystemModel(this);
    m_projectModel->setRootPath(m_workingDirectory);
    m_projectModel->setFilter(QDir::AllDirs | QDir::NoDot);
    m_projectModel->setReadOnly(true);
    m_projectListView->setModel(m_projectModel);
    m_projectListView->setRootIndex(m_projectModel->index(m_workingDirectory));
    leftLayout->addWidget(m_projectListView);

    // Another separator
    QFrame* line2 = new QFrame;
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    leftLayout->addWidget(line2);

    // Bookmarks section
    QLabel* bookmarksLabel = new QLabel(tr("Bookmarks"));
    bookmarksLabel->setStyleSheet("font-weight: bold;");
    leftLayout->addWidget(bookmarksLabel);

    // Claude Generated Phase 3.2 - Bookmark tree with hierarchical folder support
    m_bookmarkTreeView = new QTreeWidget;
    m_bookmarkTreeView->setHeaderLabels(QStringList() << tr("Bookmarks"));
    m_bookmarkTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_bookmarkTreeView->setDragDropMode(QAbstractItemView::InternalMove);
    leftLayout->addWidget(m_bookmarkTreeView);

    // Claude Generated Phase 4.3 - Workspace list section
    QFrame* line3 = new QFrame;
    line3->setFrameShape(QFrame::HLine);
    line3->setFrameShadow(QFrame::Sunken);
    leftLayout->addWidget(line3);

    // Workspace header with add button
    QWidget* workspaceHeader = new QWidget;
    QHBoxLayout* wsHeaderLayout = new QHBoxLayout(workspaceHeader);
    wsHeaderLayout->setContentsMargins(0, 0, 0, 0);
    wsHeaderLayout->setSpacing(5);

    QLabel* workspacesLabel = new QLabel(tr("Workspaces"));
    workspacesLabel->setStyleSheet("font-weight: bold;");
    wsHeaderLayout->addWidget(workspacesLabel);

    QPushButton* newWorkspaceButton = new QPushButton("+");
    newWorkspaceButton->setMaximumWidth(30);
    newWorkspaceButton->setToolTip(tr("Save current state as workspace"));
    connect(newWorkspaceButton, &QPushButton::clicked, this, &MainWindow::saveCurrentWorkspace);
    wsHeaderLayout->addWidget(newWorkspaceButton);

    leftLayout->addWidget(workspaceHeader);

    // Workspace list
    m_workspaceListView = new QListWidget;
    m_workspaceListView->setContextMenuPolicy(Qt::CustomContextMenu);
    leftLayout->addWidget(m_workspaceListView);

    // Add left widget to splitter
    m_splitter->addWidget(leftWidget);

    // Middle section: File content view
    QWidget *middleWidget = new QWidget;
    QVBoxLayout *middleLayout = new QVBoxLayout(middleWidget);

    // Make new calculation button and create directory - Claude Generated Phase 2.1
    // Claude Generated - Visual Polish: Button icons
    m_newCalculationButton = new QPushButton(tr("Create Calculation Directory"));
    m_newCalculationButton->setIcon(QIcon::fromTheme("folder-new", QIcon()));
    m_newCalculationButton->setToolTip(tr("Create a new calculation directory (Ctrl+N)"));
    m_newCalculationButton->setIconSize(QSize(16, 16));
    middleLayout->addWidget(m_newCalculationButton);

    // Claude Generated - Quick Fix: Project label with copy button
    QWidget* pathWidget = new QWidget;
    QHBoxLayout* pathLayout = new QHBoxLayout(pathWidget);
    pathLayout->setContentsMargins(0, 0, 0, 0);

    m_currentProjectLabel = new QLabel(m_currentCalculationDir);
    m_currentProjectLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_currentProjectLabel->setWordWrap(false);
    pathLayout->addWidget(m_currentProjectLabel);

    QPushButton* copyPathButton = new QPushButton;
    copyPathButton->setIcon(QIcon::fromTheme("edit-copy"));
    copyPathButton->setToolTip(tr("Copy current path to clipboard"));
    copyPathButton->setMaximumWidth(30);
    connect(copyPathButton, &QPushButton::clicked, this, &MainWindow::copyCurrentPath);
    pathLayout->addWidget(copyPathButton);

    middleLayout->addWidget(pathWidget);

    // Claude Generated - Phase 3.3: Visual state indicators
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

    middleLayout->addWidget(stateWidget);

    // Setup directory content view for files
    m_directoryContentView = new QListView;
    m_directoryContentModel = new QFileSystemModel(this);
    m_directoryContentModel->setFilter(QDir::NoDotAndDotDot | QDir::Files);
    // Set file filters for relevant chemistry files
    m_directoryContentModel->setNameFilters(QStringList()
        << "*.xyz" << "*.vtf" << "*.inp" << "*.log" << "*.out"
        << "*.hess" << "*.gbw" << "*.txt" << "*.*" << "input");
    m_directoryContentModel->setNameFilterDisables(false);
    m_directoryContentView->setModel(m_directoryContentModel);

    // Setup context menu for files
    m_directoryContentView->setContextMenuPolicy(Qt::CustomContextMenu);
    setupContextMenu();
    middleLayout->addWidget(m_directoryContentView);
    m_splitter->addWidget(middleWidget);

    // Right section: Program controls and editors
    QWidget *rightWidget = new QWidget;
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    m_splitter->addWidget(rightWidget);

    // Initialize available program commands
    initializeProgramCommands();

    // Program selection dropdown
    QHBoxLayout *programLayout = new QHBoxLayout;
    m_programSelector = new QComboBox;
    m_programSelector->addItems(m_simulationPrograms);
    m_programSelector->setToolTip(tr("Choose computational chemistry program"));
    programLayout->addWidget(new QLabel(tr("Program:")));
    programLayout->addWidget(m_programSelector);

    // Claude Generated - Quick Win: Calculation timer label
    m_timerLabel = new QLabel("00:00:00");
    m_timerLabel->setStyleSheet("font-weight: bold; color: #0066cc;");
    m_timerLabel->setMinimumWidth(70);
    m_timerLabel->setToolTip(tr("Elapsed calculation time"));
    programLayout->addStretch();
    programLayout->addWidget(m_timerLabel);

    rightLayout->addLayout(programLayout);

    // Command input with auto-completion
    QHBoxLayout *commandLayout = new QHBoxLayout;
    m_commandInput = new QLineEdit;
    m_commandInput->setPlaceholderText("Enter command...");
    m_commandInput->setToolTip(tr("Enter program-specific command arguments"));

    m_commandCompleter = new QCompleter(this);
    m_commandCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_commandCompleter->setFilterMode(Qt::MatchContains);
    m_commandInput->setCompleter(m_commandCompleter);

    // choose number of threads
    m_threads = new QSpinBox;
    m_threads->setRange(1, QThread::idealThreadCount());
    m_threads->setValue(1);
    m_threads->setToolTip(tr("Number of parallel threads for calculation"));

    m_uniqueFileNames = new QCheckBox(tr("Unique file names"));
    m_uniqueFileNames->setToolTip(tr("Append timestamp to output filenames"));

    // Run calculation button - Claude Generated Phase 2.1
    m_runCalculation = new QPushButton(tr("Start Calculation"));
    m_runCalculation->setIcon(QIcon::fromTheme("system-run", QIcon()));
    m_runCalculation->setToolTip(tr("Start calculation with selected program (Ctrl+R)"));
    m_runCalculation->setIconSize(QSize(16, 16));

    commandLayout->addWidget(m_commandInput, 3);
    commandLayout->addWidget(m_threads);
    commandLayout->addWidget(m_uniqueFileNames);
    commandLayout->addWidget(m_runCalculation);
    rightLayout->addLayout(commandLayout);

    // Tab widget for structure and input editors
    QTabWidget *editorTabs = new QTabWidget;

    // Structure tab
    QWidget *structureTab = new QWidget;
    QVBoxLayout *structureLayout = new QVBoxLayout(structureTab);
    
    QHBoxLayout *structureFileLayout = new QHBoxLayout;
    structureFileLayout->addWidget(new QLabel(tr("Structure file:")));
    m_structureFileEdit = new QLineEdit("input"); // Default name
    m_structureFileEdit->setToolTip(tr("Base name for structure file"));
    m_structureFileEditExtension = new QLineEdit("xyz");
    structureFileLayout->addWidget(m_structureFileEdit);
    structureFileLayout->addWidget(m_structureFileEditExtension);
    structureLayout->addLayout(structureFileLayout);

    // Structure editor - Claude Generated Phase 2.3
    m_structureView = new ModifiableTextEdit;
    m_structureView->setPlaceholderText("Structure data");
    structureLayout->addWidget(m_structureView);

    // Claude Generated - Visual Polish: Tab icons
    editorTabs->addTab(structureTab, QIcon::fromTheme("document-properties"), tr("Structure"));

    // Input tab
    QWidget *inputTab = new QWidget;
    QVBoxLayout *inputLayout = new QVBoxLayout(inputTab);

    QHBoxLayout *inputFileLayout = new QHBoxLayout;
    inputFileLayout->addWidget(new QLabel(tr("Input file:")));
    m_inputFileEdit = new QLineEdit("input"); // Default name
    m_inputFileEdit->setToolTip(tr("Base name for input file"));
    m_inputFileEditExtension = new QLineEdit("");
    inputFileLayout->addWidget(m_inputFileEdit);
    inputFileLayout->addWidget(m_inputFileEditExtension);
    inputLayout->addLayout(inputFileLayout);

    // Input editor - Claude Generated Phase 2.3
    m_inputView = new ModifiableTextEdit;
    m_inputView->setPlaceholderText("Input data");
    inputLayout->addWidget(m_inputView);

    editorTabs->addTab(inputTab, QIcon::fromTheme("document-edit"), tr("Input"));

    m_moleculeView = new MoleculeViewer;

    // Claude Generated - Apply saved visualization settings
    Settings::VisualizationSettings vizSettings = m_settings.getVisualizationSettings();
    m_moleculeView->setRenderingMode(static_cast<MoleculeViewer::RenderingMode>(vizSettings.renderingMode));
    m_moleculeView->setColorScheme(static_cast<MoleculeViewer::ColorScheme>(vizSettings.colorScheme));
    m_moleculeView->setAtomTransparency(vizSettings.atomTransparency);
    m_moleculeView->setAtomShininess(vizSettings.atomShininess);
    m_moleculeView->setAtomScaleFactor(vizSettings.atomScaleFactor);
    m_moleculeView->setBondThickness(vizSettings.bondThickness);
    m_moleculeView->setFogEnabled(vizSettings.fogEnabled);
    m_moleculeView->setFogIntensity(vizSettings.fogIntensity);

    // Frame navigation is now handled directly in MoleculeViewer control panel


    // Create integrated structure viewer widget with navigation controls
    QWidget *structureViewerWidget = new QWidget;
    QVBoxLayout *structureViewerLayout = new QVBoxLayout(structureViewerWidget);
    
    // Add the 3D molecule viewer
    structureViewerLayout->addWidget(m_moleculeView, 1);  // Stretch factor 1
    
    editorTabs->addTab(structureViewerWidget, QIcon::fromTheme("document-import"), tr("Structure Viewer"));

    rightLayout->addWidget(editorTabs);

    // Output view (read-only) with clear button
    // Claude Generated - Quick Fix: Output view with clear button
    QWidget* outputWidget = new QWidget;
    QVBoxLayout* outputLayout = new QVBoxLayout(outputWidget);
    outputLayout->setContentsMargins(0, 0, 0, 0);

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

    rightLayout->addWidget(outputWidget);

    // Shortcut for toggling left panel (Ctrl+B)
    QShortcut* toggleLeftPanelShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_B), this);
    connect(toggleLeftPanelShortcut, &QShortcut::activated, this, &MainWindow::toggleLeftPanel);

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
    resize(1200, 800);
    setWindowTitle("Qurcuma");

    // Set initial splitter sizes (20:30:50 ratio)
    m_splitter->setSizes(QList<int>() << 240 << 360 << 600);
}

void MainWindow::createToolbars()
{
    QToolBar* toolbar = new QToolBar(this);
    QAction* toggleNMR = toolbar->addAction(tr("NMR Spektren"));
    connect(toggleNMR, &QAction::triggered, [this]() { m_nmrDialog->show(); });
    addToolBar(toolbar);
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
                                
                                // Frame controls are now managed by MoleculeViewer
                            }
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

    // Settings Menu
    QMenu *settingsMenu = menuBar->addMenu(tr("&Settings"));

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
                    m_moleculeView->setVTFTrajectoryData(allAtoms, allBonds);

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
}

void MainWindow::setupShortcuts()
{
    // Claude Generated - Phase 1.2: Keyboard shortcuts
    new QShortcut(QKeySequence::New, this, SLOT(createNewDirectory()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_R), this, SLOT(runSimulation()));
    new QShortcut(QKeySequence::Refresh, this, SLOT(runSimulation()));  // F5
    new QShortcut(QKeySequence::Save, this, SLOT(saveCurrentEditor()));
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

void MainWindow::toggleLeftPanel()
{
    QList<int> sizes = m_splitter->sizes();
    if (sizes[0] > 0) { // If left panel is visible
        m_lastLeftPanelWidth = sizes[0]; // Store current width
        sizes[0] = 0; // Set width to 0
    } else { // If left panel is hidden
        sizes[0] = m_lastLeftPanelWidth > 0 ? m_lastLeftPanelWidth : 240; // Restore previous width
    }
    m_splitter->setSizes(sizes);
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
void MainWindow::applyStylesheet(bool darkMode)
{
    QString stylesheetPath = darkMode ?
        ":/stylesheets/dark.qss" :
        ":/stylesheets/light.qss";

    DEBUG_LOG << "[Dark Mode] Attempting to load stylesheet:" << stylesheetPath;

    QFile styleFile(stylesheetPath);
    if (styleFile.open(QFile::ReadOnly)) {
        QString styleSheet = QString::fromUtf8(styleFile.readAll());
        DEBUG_LOG << "[Dark Mode] Stylesheet loaded successfully, size:" << styleSheet.length() << "bytes";
        qApp->setStyleSheet(styleSheet);
        styleFile.close();
        m_darkModeEnabled = darkMode;
        m_settings.setDarkMode(darkMode);
        DEBUG_LOG << "[Dark Mode] Settings saved: darkMode =" << darkMode;
        statusBar()->showMessage(
            darkMode ? tr("Dark Mode enabled") : tr("Light Mode enabled"),
            2000);
    } else {
        qWarning() << "[Dark Mode] FAILED to open stylesheet file:" << stylesheetPath;
        qWarning() << "[Dark Mode] Error:" << styleFile.errorString();
    }
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
    ws.splitterStates = m_splitter->saveState();
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

    if (!ws.splitterStates.isEmpty() && m_splitter) {
        m_splitter->restoreState(ws.splitterStates);
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