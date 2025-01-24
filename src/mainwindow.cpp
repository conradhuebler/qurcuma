#include "settings.h"
#include <QApplication>
#include <QCheckBox>
#include <QCompleter>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointer>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShortcut>
#include <QStatusBar>
#include <QStringListModel>
#include <QSysInfo>
#include <QThread>
#include <QTimer>
#include <QToolButton>

#include <QFile>
#include <QTextStream>
#include <QString>  
#include "view.h"
#include "frequencydialog.h"
#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();
    createMenus();
    setupConnections();

    // Arbeitsverzeichnis aus Settings laden
    m_workingDirectory = m_settings.workingDirectory();
    if (!m_workingDirectory.isEmpty()) {
        m_projectModel->setRootPath(m_workingDirectory);
        m_projectListView->setRootIndex(m_projectModel->index(m_workingDirectory));
    }

    m_currentProcess = new QProcess(this);
    QString lastDir = m_settings.lastUsedWorkingDirectory();
    if (!lastDir.isEmpty() && QDir(lastDir).exists()) {
        switchWorkingDirectory(lastDir);
    }
}

MainWindow::~MainWindow()
{
    //delete m_currentProcess;
}

void MainWindow::setupUI()
{
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

    // Directory icon
    m_chooseDirectory = new QPushButton(tr("Select Directory"));
    m_chooseDirectory->setIcon(QIcon::fromTheme("folder-open", QIcon(":/icons/folder.png")));
    currentDirLayout->addWidget(m_chooseDirectory);

    // Current path label with selection support
    m_currentPathLabel = new QLabel;
    m_currentPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_currentPathLabel->setWordWrap(true);
    m_currentPathLabel->setStyleSheet("QLabel { padding: 5px; background-color: palette(base); border: 1px solid palette(mid); }");
    currentDirLayout->addWidget(m_currentPathLabel, 1);

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
    QLabel* dirListLabel = new QLabel(tr("Directory Content"));
    dirListLabel->setStyleSheet("font-weight: bold;");
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

    // Bookmark list with context menu support
    m_bookmarkListView = new QListWidget;
    m_bookmarkListView->setContextMenuPolicy(Qt::CustomContextMenu);
    leftLayout->addWidget(m_bookmarkListView);

    // Add left widget to splitter
    m_splitter->addWidget(leftWidget);

    // Middle section: File content view
    QWidget *middleWidget = new QWidget;
    QVBoxLayout *middleLayout = new QVBoxLayout(middleWidget);

    // Make new calculation button and create directory
    m_newCalculationButton = new QPushButton(tr("New Calculation"));
    // m_newCalculationButton->setIcon(QIcon::fromTheme("document-new", QIcon(":/icons/document-new.png")));
    middleLayout->addWidget(m_newCalculationButton);

    m_currentProjectLabel = new QLabel(m_currentCalculationDir);
    m_currentProjectLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_currentProjectLabel->setWordWrap(true);

    middleLayout->addWidget(m_currentProjectLabel);

    // Setup directory content view for files
    m_directoryContentView = new QListView;
    m_directoryContentModel = new QFileSystemModel(this);
    m_directoryContentModel->setFilter(QDir::NoDotAndDotDot | QDir::Files);
    // Set file filters for relevant chemistry files
    m_directoryContentModel->setNameFilters(QStringList()
        << "*.xyz" << "*.inp" << "*.log" << "*.out"
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
    programLayout->addWidget(new QLabel(tr("Program:")));
    programLayout->addWidget(m_programSelector);
    rightLayout->addLayout(programLayout);

    // Command input with auto-completion
    QHBoxLayout *commandLayout = new QHBoxLayout;
    m_commandInput = new QLineEdit;
    m_commandInput->setPlaceholderText("Enter command...");

    m_commandCompleter = new QCompleter(this);
    m_commandCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_commandCompleter->setFilterMode(Qt::MatchContains);
    m_commandInput->setCompleter(m_commandCompleter);

    // choose number of threads
    m_threads = new QSpinBox;
    m_threads->setRange(1, QThread::idealThreadCount());
    m_threads->setValue(1);
    m_threads->setToolTip(tr("Number of threads to use"));

    m_uniqueFileNames = new QCheckBox(tr("Unique file names"));
    m_uniqueFileNames->setToolTip(tr("Generate unique file names for each calculation"));

    // Run calculation button
    m_runCalculation = new QPushButton(tr("Run calculation"));

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
    m_structureFileEditExtension = new QLineEdit("xyz");
    structureFileLayout->addWidget(m_structureFileEdit);
    structureFileLayout->addWidget(m_structureFileEditExtension);
    structureLayout->addLayout(structureFileLayout);

    // Structure editor
    m_structureView = new QTextEdit;
    m_structureView->setPlaceholderText("Structure data");
    structureLayout->addWidget(m_structureView);

    editorTabs->addTab(structureTab, tr("Structure"));

    // Input tab
    QWidget *inputTab = new QWidget;
    QVBoxLayout *inputLayout = new QVBoxLayout(inputTab);
    
    QHBoxLayout *inputFileLayout = new QHBoxLayout;
    inputFileLayout->addWidget(new QLabel(tr("Input file:")));
    m_inputFileEdit = new QLineEdit("input"); // Default name
    m_inputFileEditExtension = new QLineEdit("");
    inputFileLayout->addWidget(m_inputFileEdit);
    inputFileLayout->addWidget(m_inputFileEditExtension);
    inputLayout->addLayout(inputFileLayout);

    // Input editor
    m_inputView = new QTextEdit;
    m_inputView->setPlaceholderText("Input data");
    inputLayout->addWidget(m_inputView);

    editorTabs->addTab(inputTab, tr("Input"));

    m_moleculeView = new MoleculeViewer;
    editorTabs->addTab(m_moleculeView, tr("Structure Viewer"));

    rightLayout->addWidget(editorTabs);

    // Output view (read-only)
    m_outputView = new QTextEdit;
    m_outputView->setPlaceholderText("Output");
    m_outputView->setReadOnly(true);
    rightLayout->addWidget(m_outputView);

    // Shortcut for toggling left panel (Ctrl+B)
    QShortcut* toggleLeftPanelShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_B), this);
    connect(toggleLeftPanelShortcut, &QShortcut::activated, this, &MainWindow::toggleLeftPanel);

    // Initial updates
    updatePathLabel(m_workingDirectory);
    updateBookmarkView();

    // Window settings
    resize(1200, 800);
    setWindowTitle("Qurcuma");

    // Set initial splitter sizes (20:30:50 ratio)
    m_splitter->setSizes(QList<int>() << 240 << 360 << 600);
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
                
                QAction *avogadroAction = contextMenu.addAction(tr("Mit Avogadro öffnen"));
                QAction *iboviewAction = contextMenu.addAction(tr("Mit IboView öffnen"));

                connect(avogadroAction, &QAction::triggered,
                    [this, filePath]() { openWithVisualizer(filePath, "avogadro"); });
                connect(iboviewAction, &QAction::triggered,
                    [this, filePath]() { openWithVisualizer(filePath, "iboview"); });

                contextMenu.exec(m_directoryContentView->viewport()->mapToGlobal(pos));
            }else if(filePath.endsWith(".gbw", Qt::CaseInsensitive) || filePath.endsWith(".loc", Qt::CaseInsensitive) || filePath.endsWith(".ges", Qt::CaseInsensitive)) 
            {
                QMenu contextMenu(this);
                QAction *fileNameAction = contextMenu.addAction(tr("mit IboView öffnen"));
                connect(fileNameAction, &QAction::triggered, [this, filePath]() { openWithVisualizer(filePath, "iboview"); });
                contextMenu.exec(m_directoryContentView->viewport()->mapToGlobal(pos));

            }else if(filePath.contains("molden"))
            {
                QMenu contextMenu(this);
                QAction *fileNameAction = contextMenu.addAction(tr("mit IboView öffnen"));
                connect(fileNameAction, &QAction::triggered, [this, filePath]() { openWithVisualizer(filePath, "iboview"); });
                contextMenu.exec(m_directoryContentView->viewport()->mapToGlobal(pos));
            }else if(filePath.contains("hess"))
            {
                QMenu contextMenu(this);

                QPair<int, int>frequencies = countImaginaryFrequencies(filePath);
                QAction *freq_action = contextMenu.addAction(tr("Imaginäre Frequenzen: %1\nRegulare Frequenzen %2").arg(frequencies.first).arg(frequencies.second));
                freq_action->setEnabled(false);
                contextMenu.addSeparator();
                QAction *plotvib = contextMenu.addAction(tr("Vibrationsmoden erstellen"));

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

            }
        });
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

    // Datei-Menü
    QMenu *fileMenu = menuBar->addMenu(tr("&Datei"));

    fileMenu->addSeparator();
    fileMenu->addAction(tr("Beenden"), this, &QWidget::close);

    // Einstellungen-Menü
    QMenu *settingsMenu = menuBar->addMenu(tr("&Einstellungen"));
    settingsMenu->addAction(tr("Programme konfigurieren..."), 
        this, &MainWindow::configurePrograms);

    // Statusleiste
    setStatusBar(new QStatusBar);
}

void MainWindow::setupConnections()
{
    // Kommandozeilen-Verbindung
    connect(m_commandInput, &QLineEdit::returnPressed,
        this, &MainWindow::runCommand);

    // Programmauswahl-Verbindung
    connect(m_programSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MainWindow::programSelected);

    // Projekt-Auswahl-Verbindung
    connect(m_projectListView, &QListView::clicked,
        this, &MainWindow::projectSelected);

    // Prozess-Verbindungen
    m_currentProcess = new QProcess(this);
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
                m_commandInput->setPlaceholderText("Simulationskommando eingeben...");
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

    connect(m_projectListView->selectionModel(),
        &QItemSelectionModel::currentChanged,
        [this](const QModelIndex& current, const QModelIndex&) {
            if (current.isValid()) {
                QString path = m_projectModel->filePath(current);
                updateDirectoryContent(path);
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

    connect(m_projectListView->selectionModel(),
        &QItemSelectionModel::currentChanged,
        [this](const QModelIndex& current, const QModelIndex&) {
            if (current.isValid()) {
                QString path = m_projectModel->filePath(current);
                updateDirectoryContent(path);
            }
        });
    connect(m_directoryContentView, &QListView::clicked,
        [this](const QModelIndex& index) {
            QString filePath = m_directoryContentModel->filePath(index);
            QString suffix = QFileInfo(filePath).suffix().toLower();
            QString basename = QFileInfo(filePath).baseName();
            if (suffix == "xyz") {
                // XYZ-Datei in Structure View laden
                QFile file(filePath);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QByteArray data = file.readAll();
                    m_structureView->setPlainText(QString::fromUtf8(data));
                    m_structureFileEdit->setText(QFileInfo(filePath).fileName());
                    file.close();
                    if(data.isEmpty() || filePath.contains("trj"))
                        return;

                    QStringList lines = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);
                    int num_atoms = lines[0].toInt();
                    if(lines.size() < num_atoms + 2)
                        return;
                   // qDebug() << "Number of atoms: " << num_atoms << lines.size();
                    if (num_atoms > 0) {
                        QVector<MoleculeViewer::Atom> atoms;
                        for (int i = 2; i < num_atoms + 1; ++i) {
                            //qDebug() << lines[i];
                            QStringList parts = lines[i].split(QRegularExpression("\\s+"));
                            if (parts.size() >= 4) {
                                MoleculeViewer::Atom atom;
                                atom.element = parts[0];
                                atom.position = QVector3D(parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat());
                                atoms.append(atom);
                            }
                        }
                        m_moleculeView->addMolecule(atoms);
                    }

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

    connect(m_bookmarkListView, &QListWidget::itemClicked,
        [this](QListWidgetItem* item) {
            if (item) {
                switchWorkingDirectory(item->data(Qt::UserRole).toString());
            }
        });

    // Kontextmenü für Bookmarks
    connect(m_bookmarkListView, &QListWidget::customContextMenuRequested,
        [this](const QPoint& pos) {
            QListWidgetItem* item = m_bookmarkListView->itemAt(pos);
            if (!item)
                return;

            QMenu contextMenu(this);
            QAction* removeAction = contextMenu.addAction(tr("Remove Bookmark"));

            connect(removeAction, &QAction::triggered, [this, item]() {
                QString path = item->data(Qt::UserRole).toString();
                m_settings.removeWorkingDirectory(path);
                updateBookmarkView();
            });

            contextMenu.exec(m_bookmarkListView->viewport()->mapToGlobal(pos));
        });

    connect(m_bookmarkButton, &QToolButton::clicked, [this]() {
        if (!m_workingDirectory.isEmpty()) {
            m_settings.addWorkingDirectory(m_workingDirectory);
            updateBookmarkView();
            statusBar()->showMessage(tr("Directory bookmarked: %1")
                                         .arg(QDir(m_workingDirectory).dirName()),
                3000);
        }
    });

    // Verbindung für Verzeichniswechsel im projectListView
    connect(m_projectListView, &QListView::clicked, [this](const QModelIndex& index) {
        QString path = m_projectModel->filePath(index);
        if (path.isEmpty())
            return;

        QDir dir(path);
        if (dir.exists()) {
            // Wenn ".." gewählt wurde, gehe zum übergeordneten Verzeichnis
            if (m_projectModel->fileName(index) == "..") {
                dir.cdUp();
                path = dir.absolutePath();
                switchWorkingDirectory(path);
            }

            // Wenn es sich um ein übergeordnetes Verzeichnis des Arbeitsverzeichnisses handelt
            if (!path.startsWith(m_workingDirectory)) {
                // Optional: Frage den Benutzer, ob das Arbeitsverzeichnis gewechselt werden soll
                QMessageBox::StandardButton reply = QMessageBox::question(this,
                    tr("Change Working Directory"),
                    tr("Do you want to set %1 as your working directory?").arg(path),
                    QMessageBox::Yes | QMessageBox::No);

                if (reply == QMessageBox::Yes) {
                    switchWorkingDirectory(path);
                    m_settings.addWorkingDirectory(path);
                    updateBookmarkView();
                }
            } else {
                updateDirectoryContent(path);
            }
        }
    });

    connect(m_chooseDirectory, &QPushButton::clicked, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this,
            tr("Choose directory"),
            m_workingDirectory.isEmpty() ? QDir::homePath() : m_workingDirectory);
        if (!dir.isEmpty()) {
            switchWorkingDirectory(dir);
        }
    });
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
                updateBookmarkView();
                statusBar()->showMessage(tr("Directory bookmarked: %1")
                                             .arg(QDir(path).dirName()),
                    3000);
            });

            QAction* setWorkDirAction = contextMenu.addAction(tr("Set as Working Directory"));
            connect(setWorkDirAction, &QAction::triggered, [this, path]() {
                switchWorkingDirectory(path);
                m_settings.addWorkingDirectory(path);
                updateBookmarkView();
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
            m_commandInput->setPlaceholderText(tr("Kommando für %1 eingeben...").arg(program));
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
        
        hbox->addWidget(new QLabel(tr("ORCA Binärverzeichnis")));
        hbox->addWidget(pathEdit);
        hbox->addWidget(browseBtn);
        layout->addLayout(hbox);

        connect(browseBtn, &QPushButton::clicked, [=]() {
            QString path = QFileDialog::getExistingDirectory(this, 
                tr("ORCA Binärverzeichnis wählen"), 
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
                    tr("Pfad für ") + program, 
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
    QString calcName = QInputDialog::getText(this, tr("Neue Rechnung"),
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
        QMessageBox::warning(this, tr("Fehler"),
            tr("Bitte wählen Sie zuerst ein gültiges Arbeitsverzeichnis."));
        return false;
    }

    // Erstelle das Unterverzeichnis
    m_currentCalculationDir = workDir.filePath(calcName);
    m_currentProjectLabel->setText(m_currentCalculationDir);
    QDir calcDir(currentCalculationDir());

    if (calcDir.exists()) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, tr("Verzeichnis existiert"),
            tr("Das Verzeichnis existiert bereits. Möchten Sie es überschreiben?"),
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply == QMessageBox::No) {
            return false;
        }
        // Lösche existierendes Verzeichnis
        calcDir.removeRecursively();
    }

    if (!workDir.mkdir(calcName)) {
        QMessageBox::warning(this, tr("Fehler"),
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

    QString dirName = QInputDialog::getText(this, tr("Neues Verzeichnis"),
        tr("Verzeichnisname:"), QLineEdit::Normal, suggestedName, &ok);
    
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
        QMessageBox::warning(this, tr("Fehler"),
            tr("Ein Verzeichnis mit diesem Namen existiert bereits."));
        return;
    }

    if (!workDir.mkdir(dirName)) {
        QMessageBox::warning(this, tr("Fehler"),
            tr("Konnte Verzeichnis nicht erstellen."));
        return;
    }
    m_structureView->clear();
    m_inputFileEdit->clear();
    m_inputView->clear();

    // Programm-spezifische Initialisierung
    QString program = m_programSelector->currentText();
    setupProgramSpecificDirectory(newDirPath, program);

    // Aktualisiere die Ansicht und setze das neue Verzeichnis als aktuell
    updateDirectoryContent(newDirPath);
    m_currentCalculationDir = dirName;
    m_currentProjectLabel->setText(m_currentCalculationDir);

    statusBar()->showMessage(tr("Verzeichnis erstellt: ") + dirName);
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
        QMessageBox::warning(this, tr("Fehler"), 
            tr("Bitte wählen Sie ein Simulationsprogramm."));
        return;
    }

    bool input_empty = true, structure_empty = true, argument_empty = true;
    // Generiere eindeutige Namen für diese Berechnung
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString structureFile = generateUniqueFileName(m_structureFileEdit->text(), m_structureFileEditExtension->text());
    QString trjFile = generateUniqueFileName(m_structureFileEdit->text(), "trj" + m_structureFileEditExtension->text());
    QString inputFile = generateUniqueFileName(m_inputFileEdit->text(), m_inputFileEditExtension->text());

    QString outputFile = generateUniqueFileName("output", "log");
    qDebug() << currentCalculationDir() + QDir::separator() + outputFile;
    qDebug() << "Output file: " << outputFile << "Structure file: " << structureFile << "Input file: " << inputFile << "Timestamp: " << timestamp;
    // Speichere aktuelle Strukturdaten
    QFile structFile(currentCalculationDir() + QDir::separator() + structureFile);
    if (structFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        structFile.write(m_structureView->toPlainText().toUtf8());
        qDebug() << m_structureView->toPlainText().toUtf8();
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
            QMessageBox::warning(this, tr("Fehler"),
                tr("Bitte konfigurieren Sie zuerst das ORCA Binärverzeichnis."));
            return;
        }
        if (input_empty) {
            QMessageBox::warning(this, tr("Fehler"),
                tr("Bitte füllen Sie die Input-Datei aus."));
            return;
        }
        // ORCA-spezifischer Start
        QString orcaExe = orcaPath + "/orca";
        m_currentProcess->setWorkingDirectory(currentCalculationDir());
        m_currentProcess->setProgram(orcaExe);
        // ORCA erwartet den Input-Dateinamen als Argument

        m_currentProcess->setArguments(QStringList() << inputFile);
        qDebug() << "copying file " << structureFile << " to " << m_structureFileEdit->text() + ".xyz";
        QFile::copy(currentCalculationDir() + QDir::separator() + structureFile, currentCalculationDir() + QDir::separator() + m_structureFileEdit->text() + ".xyz");
    } 
    else {
        if (structure_empty) {
            QMessageBox::warning(this, tr("Fehler"),
                tr("Bitte füllen Sie die Strukturdaten aus."));
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
    qDebug() << currentCalculationDir() + QDir::separator() + outputFile;
    m_currentProcess->setStandardOutputFile(currentCalculationDir() + QDir::separator() + outputFile, QIODevice::Append);
    m_currentProcess->setStandardErrorFile(currentCalculationDir() + QDir::separator() + outputFile, QIODevice::Append);

    // Starte Prozess und füge Eintrag zur Historie hinzu
    m_currentProcess->start();
    qDebug() << "Starting process" << m_currentProcess->program() << m_currentProcess->arguments() << m_currentProcess->workingDirectory() << m_currentProcess->error() << m_currentProcess->errorString();
    addCalculationToHistory(entry);

    // Verbinde Prozessende
    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [this, entry](int exitCode, QProcess::ExitStatus exitStatus) {
            // Aktualisiere Status in der Historie
            CalculationEntry updatedEntry = entry;
            updatedEntry.status = (exitCode == 0) ? "completed" : "error";
            addCalculationToHistory(updatedEntry);

            updateOutputView(currentCalculationDir() + QDir::separator() + entry.outputFile);
            statusBar()->showMessage(exitCode == 0 ? 
                tr("Berechnung erfolgreich beendet") : 
                tr("Berechnung mit Fehler beendet (Code: %1)").arg(exitCode));

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
    statusBar()->showMessage(tr("Berechnung läuft..."));
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
        qDebug() << "Starting process" << m_currentProcess->program() << m_currentProcess->arguments() << m_currentProcess->workingDirectory() << m_currentProcess->error() << m_currentProcess->errorString();
        m_currentProcess->waitForFinished();
        qDebug() << m_currentProcess->readAllStandardOutput();
        qDebug() << m_currentProcess->readAllStandardError();
        qDebug() << m_currentProcess->errorString();

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
        QMessageBox::warning(this, tr("Fehler"),
            tr("Pfad für %1 nicht konfiguriert.").arg(visualizer));
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
        qDebug() << fileDir + "/tmp.gbw" << nfilePath;
        m_currentProcess->setWorkingDirectory(currentCalculationDir());
        m_currentProcess->setProgram(orcaExe);
        m_currentProcess->setArguments(QStringList() << nfilePath << "-molden");
        m_currentProcess->start();
        m_currentProcess->waitForFinished();
        arguments << nfilePath + ".molden.input";
    }else
        arguments << filePath;  // Übergebe den Dateipfad als Argument

    // Debug-Ausgabe
    qDebug() << "Starting" << visualizer << "with file:" << filePath;
    qDebug() << "Program path:" << programPath;
    qDebug() << "Arguments:" << arguments;

    if (!QProcess::startDetached(programPath, arguments)) {
        QMessageBox::warning(this, tr("Fehler"),
            tr("Konnte %1 nicht starten.").arg(visualizer));
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
        m_commandInput->setPlaceholderText("Simulationskommando eingeben...");
    } else if (m_visualizerPrograms.contains(program)) {
        m_commandInput->setEnabled(false);
        m_commandInput->setPlaceholderText("Visualisierungsprogramm - kein Kommando nötig");
    }
}

void MainWindow::projectSelected(const QModelIndex &index)
{
    if (!index.isValid()) return;

    QString path = m_projectModel->fileName(index);
    m_currentCalculationDir = path;
    updateDirectoryContent(path);
    syncRightView(path);
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
}

void MainWindow::startNewCalculation()
{
    QString program = m_programSelector->currentText();

    // Prüfe ob ein Programm ausgewählt ist
    if (program.isEmpty()) {
        QMessageBox::warning(this, tr("Fehler"),
            tr("Bitte wählen Sie zuerst ein Programm aus."));
        return;
    }

    // Prüfe ob Input vorhanden ist
    if (m_inputView->toPlainText().isEmpty() && program == "orca") {
        QMessageBox::warning(this, tr("Fehler"),
            tr("Bitte geben Sie zuerst Input-Daten ein."));
        return;
    }

    // Je nach Programmtyp die entsprechende Aktion ausführen
    if (m_simulationPrograms.contains(program)) {
        runSimulation();
    }
}


void MainWindow::updateDirectoryContent(const QString &path)
{
    qDebug() << "Updating directory content:" << path << currentCalculationDir() << m_workingDirectory;
    // m_currentCalculationDir = QDir::nameFiltersFromString(path).last();
    m_directoryContentModel->setRootPath(currentCalculationDir());
    qDebug() << m_directoryContentModel->rootPath();
    m_directoryContentView->setRootIndex(m_directoryContentModel->index(m_directoryContentModel->rootPath()));
    qDebug() << "Directory content updated:" << path << currentCalculationDir();
}


void MainWindow::syncRightView(const QString &path)
{
    qDebug() << "Syncing right view with:" << path << currentCalculationDir();
    QDir dir(currentCalculationDir());

    QFile defaultStructure(currentCalculationDir() + QDir::separator() + "input.xyz");
    if (defaultStructure.exists() && defaultStructure.open(QIODevice::ReadOnly | QIODevice::Text)) {
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

void MainWindow::updateBookmarkView()
{
    m_bookmarkListView->clear();
    QStringList dirs = m_settings.workingDirectories();

    for (const QString& dir : dirs) {
        QListWidgetItem* item = new QListWidgetItem(QDir(dir).dirName());
        item->setData(Qt::UserRole, dir);
        item->setToolTip(dir);
        m_bookmarkListView->addItem(item);
    }
}

void MainWindow::updatePathLabel(const QString& path)
{
    QString displayPath = QDir::toNativeSeparators(path); // Korrekte Pfadtrenner für das OS
    m_currentPathLabel->setText(displayPath);
    m_currentPathLabel->setToolTip(displayPath); // Zeigt den vollen Pfad als Tooltip
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

    // UI aktualisieren
    updateDirectoryContent(path);
    updatePathLabel(path); // Aktualisiere das Pfad-Label
    statusBar()->showMessage(tr("Working directory changed to: %1").arg(path));
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