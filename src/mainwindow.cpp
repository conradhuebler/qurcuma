#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QCompleter>   
#include <QStringListModel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QSysInfo>

#include "settings.h"

#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();
    createMenus();
    setupConnections();

    // Arbeitsverzeichnis aus Settings laden
    m_workingDirectory = settings.workingDirectory();
    if (!m_workingDirectory.isEmpty()) {
        projectModel->setRootPath(m_workingDirectory);
        projectListView->setRootIndex(projectModel->index(m_workingDirectory));
    }

    m_currentProcess = new QProcess(this);
}

MainWindow::~MainWindow()
{
    //delete m_currentProcess;
}

void MainWindow::setupUI()
{
    // Hauptwidget erstellen
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // Hauptlayout erstellen
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    // Splitter für flexible Größenanpassung
    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    mainLayout->addWidget(splitter);

    // Linke Seite: Container für Button und Projektliste
    QWidget *leftWidget = new QWidget;
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    
    // Neuer "Neues Verzeichnis" Button
    newDirectoryButton = new QPushButton(tr("Neues Verzeichnis"));
    leftLayout->addWidget(newDirectoryButton);
    
    // Projektliste
    projectListView = new QListView;
    projectModel = new QFileSystemModel(this);
    projectModel->setRootPath(m_workingDirectory);
    projectModel->setFilter(QDir::NoDotAndDotDot | QDir::Dirs);
    projectListView->setModel(projectModel);
    leftLayout->addWidget(projectListView);
    
    splitter->addWidget(leftWidget);

    // Mittlere Seite: Verzeichnisinhalt
    QWidget *middleWidget = new QWidget;
    QVBoxLayout *middleLayout = new QVBoxLayout(middleWidget);
    
    directoryContentView = new QListView;
        directoryContentModel = new QFileSystemModel(this);
    directoryContentModel->setFilter(QDir::NoDotAndDotDot | QDir::Files);
    // Zeige alle relevanten Dateien
    directoryContentModel->setNameFilters(QStringList() 
        << "*.xyz" << "*.inp" << "*.log" << "*.out" 
        << "*.hess" << "*.gbw" << "*.txt" << "*.*");
    directoryContentModel->setNameFilterDisables(false);  // Verstecke nicht-matchende Dateien
    directoryContentView->setModel(directoryContentModel);

    // Context Menu für Dateien
    directoryContentView->setContextMenuPolicy(Qt::CustomContextMenu);
    setupContextMenu();
    middleLayout->addWidget(directoryContentView);
    splitter->addWidget(middleWidget);

    // Rechte Seite: Container
    QWidget *rightWidget = new QWidget;
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    splitter->addWidget(rightWidget);

    initializeProgramCommands();

    // Programmauswahl mit angepasstem Verhalten
    QHBoxLayout *programLayout = new QHBoxLayout;
    programSelector = new QComboBox;
    programSelector->addItems(simulationPrograms);
    programLayout->addWidget(new QLabel(tr("Programm:")));
    programLayout->addWidget(programSelector);
    rightLayout->addLayout(programLayout);

    // Kommandozeile mit Autovervollständigung
    QHBoxLayout *commandLayout = new QHBoxLayout;
    commandInput = new QLineEdit;
    commandInput->setPlaceholderText("Kommando eingeben...");
    
    // Erstelle Completer
    commandCompleter = new QCompleter(this);
    commandCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    commandCompleter->setFilterMode(Qt::MatchContains);
    commandInput->setCompleter(commandCompleter);
    
    newCalculationButton = new QPushButton(tr("Neue Rechnung"));
    
    commandLayout->addWidget(commandInput, 3);
    commandLayout->addWidget(newCalculationButton);
    rightLayout->addLayout(commandLayout);
    
    commandLayout->addWidget(commandInput, 3);
    commandLayout->addWidget(newCalculationButton);
    rightLayout->addLayout(commandLayout);

// Tab-Widget für Struktur und Input
    QTabWidget *editorTabs = new QTabWidget;
    
    // Struktur-Tab
    QWidget *structureTab = new QWidget;
    QVBoxLayout *structureLayout = new QVBoxLayout(structureTab);
    
    QHBoxLayout *structureFileLayout = new QHBoxLayout;
    structureFileLayout->addWidget(new QLabel(tr("Strukturdatei:")));
    structureFileEdit = new QLineEdit("input.xyz");  // Standard-Name
    structureFileLayout->addWidget(structureFileEdit);
    structureLayout->addLayout(structureFileLayout);
    
    structureView = new QTextEdit;
    structureView->setPlaceholderText("Strukturdaten");
    structureLayout->addWidget(structureView);
    
    editorTabs->addTab(structureTab, tr("Struktur"));

    // Input-Tab
    QWidget *inputTab = new QWidget;
    QVBoxLayout *inputLayout = new QVBoxLayout(inputTab);
    
    QHBoxLayout *inputFileLayout = new QHBoxLayout;
    inputFileLayout->addWidget(new QLabel(tr("Input-Datei:")));
    inputFileEdit = new QLineEdit("input.inp");  // Standard-Name
    inputFileLayout->addWidget(inputFileEdit);
    inputLayout->addLayout(inputFileLayout);
    
    inputView = new QTextEdit;
    inputView->setPlaceholderText("Input-Daten");
    inputLayout->addWidget(inputView);
    
    editorTabs->addTab(inputTab, tr("Input"));

    rightLayout->addWidget(editorTabs);

    // Output-Ansicht
    outputView = new QTextEdit;
    outputView->setPlaceholderText("Ausgabe");
    outputView->setReadOnly(true);
    rightLayout->addWidget(outputView);

    // Fenstereinstellungen
    resize(1200, 800);
    setWindowTitle("Qurcuma");

    // Splitter-Größenverhältnis einstellen (20:30:50)
    splitter->setSizes(QList<int>() << 240 << 360 << 600);
    updateCommandLineVisibility(programSelector->currentText());
}

void MainWindow::setupContextMenu()
{
    connect(directoryContentView, &QListView::customContextMenuRequested,
        [this](const QPoint &pos) {
            QModelIndex index = directoryContentView->indexAt(pos);
            if (!index.isValid()) return;

            QString filePath = directoryContentModel->filePath(index);
            if (!filePath.endsWith(".xyz", Qt::CaseInsensitive)) return;

            QMenu contextMenu(this);
            
            // Dateiname zur Information anzeigen
            QAction *fileNameAction = contextMenu.addAction(QFileInfo(filePath).fileName());
            fileNameAction->setEnabled(false);
            contextMenu.addSeparator();
            
            QAction *avogadroAction = contextMenu.addAction(tr("Mit Avogadro öffnen"));
            QAction *iboviewAction = contextMenu.addAction(tr("Mit IboView öffnen"));

            connect(avogadroAction, &QAction::triggered, 
                [this, filePath]() { 
                    openWithVisualizer(filePath, "avogadro"); 
                });
            connect(iboviewAction, &QAction::triggered, 
                [this, filePath]() { 
                    openWithVisualizer(filePath, "iboview"); 
                });

            contextMenu.exec(directoryContentView->viewport()->mapToGlobal(pos));
        });
}

void MainWindow::initializeProgramCommands()
{
    // Curcuma Befehle
    programCommands["curcuma"] = QStringList{
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
    programCommands["xtb"] = QStringList{
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
    
    QAction *setWorkDirAction = fileMenu->addAction(tr("Arbeitsverzeichnis..."));
    connect(setWorkDirAction, &QAction::triggered, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, 
            tr("Arbeitsverzeichnis wählen"),
            m_workingDirectory.isEmpty() ? QDir::homePath() : m_workingDirectory);
        if (!dir.isEmpty()) {
            m_workingDirectory = dir;
            settings.setWorkingDirectory(dir);
            projectModel->setRootPath(dir);
            projectListView->setRootIndex(projectModel->index(dir));
        }
    });

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
    connect(commandInput, &QLineEdit::returnPressed, 
        this, &MainWindow::runCommand);

    // Programmauswahl-Verbindung
    connect(programSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MainWindow::programSelected);

    // Projekt-Auswahl-Verbindung
    connect(projectListView, &QListView::clicked,
        this, &MainWindow::projectSelected);

    // Prozess-Verbindungen
    m_currentProcess = new QProcess(this);
    connect(m_currentProcess, &QProcess::readyReadStandardOutput,
        this, &MainWindow::processOutput);
    connect(m_currentProcess, &QProcess::readyReadStandardError,
        this, &MainWindow::processError);

    // Programmtyp-spezifische Aktionen
    connect(programSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [this](int index) {
            QString program = programSelector->itemText(index);
            if (simulationPrograms.contains(program)) {
                commandInput->setEnabled(true);
                commandInput->setPlaceholderText("Simulationskommando eingeben...");
            } else if (visualizerPrograms.contains(program)) {
                commandInput->setEnabled(false);
                commandInput->setPlaceholderText("Visualisierungsprogramm - kein Kommando nötig");
            }
        });

    // Verbindung für den "Neue Rechnung" Button
    connect(newCalculationButton, &QPushButton::clicked,
            this, &MainWindow::startNewCalculation);

                connect(newDirectoryButton, &QPushButton::clicked,
            this, &MainWindow::createNewDirectory);
            
    connect(projectListView->selectionModel(), 
            &QItemSelectionModel::currentChanged,
            [this](const QModelIndex &current, const QModelIndex &) {
                if (current.isValid()) {
                    QString path = projectModel->filePath(current);
                    updateDirectoryContent(path);
                }
            });

                connect(programSelector, &QComboBox::currentTextChanged,
            this, &MainWindow::updateCommandLineVisibility);

    // Verbinde Programmauswahl mit Completer-Aktualisierung
    connect(programSelector, &QComboBox::currentTextChanged,
            [this](const QString &program) {
                if (programCommands.contains(program)) {
                    commandCompleter->setModel(new QStringListModel(programCommands[program]));
                }
            });

    connect(projectListView->selectionModel(), 
        &QItemSelectionModel::currentChanged,
        [this](const QModelIndex &current, const QModelIndex &) {
            if (current.isValid()) {
                QString path = projectModel->filePath(current);
                m_currentCalculationDir = path;
                updateDirectoryContent(path);
            }
    });
     connect(directoryContentView, &QListView::clicked,
        [this](const QModelIndex &index) {
            QString filePath = directoryContentModel->filePath(index);
            QString suffix = QFileInfo(filePath).suffix().toLower();
            QString basename = QFileInfo(filePath).baseName();
            if (suffix == "xyz") {
                // XYZ-Datei in Structure View laden
                QFile file(filePath);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    structureView->setPlainText(QString::fromUtf8(file.readAll()));
                    structureFileEdit->setText(QFileInfo(filePath).fileName());
                    file.close();
                }
            }
            else if (suffix == "log" || suffix == "out" || suffix == "txt") {
                // Log/Output-Dateien in Output View laden
                QFile file(filePath);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    outputView->setPlainText(QString::fromUtf8(file.readAll()));
                    file.close();
                }
            }
            else if (suffix == "inp" || basename == "input") {
                // Input-Dateien in Input View laden
                QFile file(filePath);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    inputView->setPlainText(QString::fromUtf8(file.readAll()));
                    inputFileEdit->setText(QFileInfo(filePath).fileName());
                    file.close();
                }
            }
    });
}
// Anpassung der Kommandozeilen-Logik
void MainWindow::updateCommandLineVisibility(const QString &program)
{
    if (program == "orca") {
        commandInput->setVisible(false);
        commandInput->setEnabled(false);
        inputFileEdit->setText("input.inp");
        inputFileEdit->setReadOnly(true);
    } else {
        commandInput->setVisible(true);
        commandInput->setEnabled(true);
        inputFileEdit->setReadOnly(false);
        
        if (program == "xtb") {
            // Bei xtb wird der Strukturdateiname direkt nach dem Programmnamen verwendet
            connect(structureFileEdit, &QLineEdit::textChanged, this, [this]() {
                QString command = commandInput->text();
                // Entferne alten Dateinamen falls vorhanden
                command = command.split(" ").first();
                command += " " + structureFileEdit->text();
                commandInput->setText(command);
            });
        } else if (program == "curcuma") {
            // Bei curcuma folgt der Dateiname nach dem Befehl
            connect(commandCompleter, QOverload<const QString &>::of(&QCompleter::activated),
                this, [this](const QString &text) {
                    QString command = text + " " + structureFileEdit->text();
                    commandInput->setText(command);
                });
        }
        
        if (programCommands.contains(program)) {
            commandInput->setPlaceholderText(tr("Kommando für %1 eingeben...").arg(program));
            commandCompleter->setModel(new QStringListModel(programCommands[program]));
        }
    }
}

void MainWindow::setupProgramSpecificDirectory(const QString &dirPath, const QString &program)
{
    // Struktur speichern wenn vorhanden
    if (!structureView->toPlainText().isEmpty()) {
        QString structureFileName = structureFileEdit->text();
        QFile structureFile(dirPath + "/" + structureFileName);
        if (structureFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            structureFile.write(structureView->toPlainText().toUtf8());
            structureFile.close();
        }
    }

    if (program == "orca") {
        // ORCA-spezifische Initialisierung
        QFile inputFile(dirPath + "/input.inp");
        if (inputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            inputFile.write(inputView->toPlainText().toUtf8());
            inputFile.close();
        }
    }
    else if (program == "xtb") {
        // XTB-spezifische Initialisierung
        QString command = commandInput->text();
        if (!command.isEmpty()) {
            QFile commandFile(dirPath + "/command.txt");
            if (commandFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                commandFile.write(command.toUtf8());
                commandFile.close();
            }
        }
        
        // Input speichern falls vorhanden
        if (!inputView->toPlainText().isEmpty()) {
            QString inputFileName = inputFileEdit->text();
            QFile inputFile(dirPath + "/" + inputFileName);
            if (inputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                inputFile.write(inputView->toPlainText().toUtf8());
                inputFile.close();
            }
        }
    }
    else if (program == "curcuma") {
        QString command = commandInput->text();
        if (!command.isEmpty()) {
            QFile commandFile(dirPath + "/command.txt");
            if (commandFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                commandFile.write(command.toUtf8());
                commandFile.close();
            }
        }
        
        // Input speichern falls vorhanden
        if (!inputView->toPlainText().isEmpty()) {
            QString inputFileName = inputFileEdit->text();
            QFile inputFile(dirPath + "/" + inputFileName);
            if (inputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                inputFile.write(inputView->toPlainText().toUtf8());
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
        QLineEdit *pathEdit = new QLineEdit(settings.orcaBinaryPath());
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
            settings.setOrcaBinaryPath(pathEdit->text());
        });
    }

    // Andere Programme (außer orca)
    for (const QString &program : simulationPrograms + visualizerPrograms) {
        if (program != "orca") {  // ORCA überspringen, da bereits behandelt
            QHBoxLayout *hbox = new QHBoxLayout();
            QLineEdit *pathEdit = new QLineEdit(settings.getProgramPath(program));
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
                    settings.setProgramPath(program, path);
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
    QDir calcDir(m_currentCalculationDir);

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
    if (!structureView->toPlainText().isEmpty()) {
        QFile structFile(calcDir.filePath("input.xyz"));
        if (structFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            structFile.write(structureView->toPlainText().toUtf8());
            structFile.close();
        }
    }

    if (!inputView->toPlainText().isEmpty()) {
        QFile inputFile(calcDir.filePath("input.inp"));
        if (inputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            inputFile.write(inputView->toPlainText().toUtf8());
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
        QDir dir(m_currentCalculationDir);
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

    // Programm-spezifische Initialisierung
    QString program = programSelector->currentText();
    setupProgramSpecificDirectory(newDirPath, program);

    // Aktualisiere die Ansicht und setze das neue Verzeichnis als aktuell
    updateDirectoryContent(newDirPath);
    m_currentCalculationDir = newDirPath;

    statusBar()->showMessage(tr("Verzeichnis erstellt: ") + dirName);
}

void MainWindow::updateOutputView(const QString &logFile)
{
    QFile file(logFile);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        outputView->setPlainText(QString::fromUtf8(file.readAll()));
        file.close();
    }
}

void MainWindow::runSimulation()
{
    QString program = programSelector->currentText();
    if (!simulationPrograms.contains(program)) {
        QMessageBox::warning(this, tr("Fehler"), 
            tr("Bitte wählen Sie ein Simulationsprogramm."));
        return;
    }

    // Generiere eindeutige Namen für diese Berechnung
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString structureFile = generateUniqueFileName("structure", "xyz");
    QString outputFile = generateUniqueFileName("output", "log");

    // Speichere aktuelle Strukturdaten
    QFile structFile(m_currentCalculationDir + "/" + structureFile);
    if (structFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        structFile.write(structureView->toPlainText().toUtf8());
        structFile.close();
    }

        CalculationEntry entry;
    entry.id = timestamp;
    entry.program = program;
    entry.command = commandInput->text().trimmed();
    entry.structureFile = structureFile;
    entry.outputFile = outputFile;
    entry.timestamp = QDateTime::currentDateTime();
    entry.status = "started";

    if (program == "orca") {
        QString orcaPath = settings.orcaBinaryPath();
        if (orcaPath.isEmpty()) {
            QMessageBox::warning(this, tr("Fehler"),
                tr("Bitte konfigurieren Sie zuerst das ORCA Binärverzeichnis."));
            return;
        }
        // ORCA-spezifischer Start
        QString orcaExe = orcaPath + "/orca";
        m_currentProcess->setWorkingDirectory(m_currentCalculationDir);
        m_currentProcess->setProgram(orcaExe);
        // ORCA erwartet den Input-Dateinamen als Argument
        m_currentProcess->setArguments(QStringList() << "input.inp");
    } 
    else {
        QString programPath = settings.getProgramPath(program);
        m_currentProcess->setWorkingDirectory(m_currentCalculationDir);
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
        }
        m_currentProcess->setArguments(args);
    }

     m_currentProcess->setStandardOutputFile(m_currentCalculationDir + "/" + outputFile, QIODevice::Append);
    m_currentProcess->setStandardErrorFile(m_currentCalculationDir + "/" + outputFile, QIODevice::Append);

    // Starte Prozess und füge Eintrag zur Historie hinzu
    m_currentProcess->start();
    addCalculationToHistory(entry);

    // Verbinde Prozessende
    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [this, entry](int exitCode, QProcess::ExitStatus exitStatus) {
            // Aktualisiere Status in der Historie
            CalculationEntry updatedEntry = entry;
            updatedEntry.status = (exitCode == 0) ? "completed" : "error";
            addCalculationToHistory(updatedEntry);
            
            updateOutputView(m_currentCalculationDir + "/" + entry.outputFile);
            statusBar()->showMessage(exitCode == 0 ? 
                tr("Berechnung erfolgreich beendet") : 
                tr("Berechnung mit Fehler beendet (Code: %1)").arg(exitCode));
    });
}

QString MainWindow::generateUniqueFileName(const QString &baseFileName, const QString &extension)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return QString("%1_%2.%3").arg(baseFileName, timestamp, extension);
}

void MainWindow::addCalculationToHistory(const CalculationEntry &entry)
{
    QString historyFile = m_currentCalculationDir + "/calculations.json";
    QList<CalculationEntry> history = loadCalculationHistory(m_currentCalculationDir);
    
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

void MainWindow::openWithVisualizer(const QString &filePath, const QString &visualizer)
{
    QString programPath = settings.getProgramPath(visualizer);
    if (programPath.isEmpty()) {
        QMessageBox::warning(this, tr("Fehler"),
            tr("Pfad für %1 nicht konfiguriert.").arg(visualizer));
        return;
    }

    QStringList arguments;
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
    QString path = settings.getProgramPath(program);
    if (path.isEmpty()) {
        QMessageBox::warning(this, "Fehler",
            "Bitte konfigurieren Sie zuerst den Pfad für " + program);
        return false;
    }
    return true;
}

void MainWindow::runCommand()
{
    QString program = programSelector->currentText();
    if (simulationPrograms.contains(program)) {
        runSimulation();
    }
}

void MainWindow::programSelected(int index)
{
    QString program = programSelector->itemText(index);
    if (simulationPrograms.contains(program)) {
        commandInput->setEnabled(true);
        commandInput->setPlaceholderText("Simulationskommando eingeben...");
    } else if (visualizerPrograms.contains(program)) {
        commandInput->setEnabled(false);
        commandInput->setPlaceholderText("Visualisierungsprogramm - kein Kommando nötig");
    }
}

void MainWindow::projectSelected(const QModelIndex &index)
{
    if (!index.isValid()) return;
    
    QString path = projectModel->filePath(index);
    m_currentCalculationDir = path;
    updateDirectoryContent(path);
    syncRightView(path);
}

void MainWindow::processOutput()
{
    QByteArray output = m_currentProcess->readAllStandardOutput();
    outputView->append(QString::fromUtf8(output));
}

void MainWindow::processError()
{
    QByteArray error = m_currentProcess->readAllStandardError();
    outputView->append("Error: " + QString::fromUtf8(error));
}

void MainWindow::loadSettings()
{
    m_workingDirectory = settings.workingDirectory();
    if (!m_workingDirectory.isEmpty()) {
        projectModel->setRootPath(m_workingDirectory);
        projectListView->setRootIndex(projectModel->index(m_workingDirectory));
    }
}

void MainWindow::startNewCalculation()
{
    QString program = programSelector->currentText();
    
    // Prüfe ob ein Programm ausgewählt ist
    if (program.isEmpty()) {
        QMessageBox::warning(this, tr("Fehler"),
            tr("Bitte wählen Sie zuerst ein Programm aus."));
        return;
    }

    // Prüfe ob Input vorhanden ist
    if (inputView->toPlainText().isEmpty() && program == "orca") {
        QMessageBox::warning(this, tr("Fehler"),
            tr("Bitte geben Sie zuerst Input-Daten ein."));
        return;
    }

    // Je nach Programmtyp die entsprechende Aktion ausführen
    if (simulationPrograms.contains(program)) {
        runSimulation();
    } 
}


void MainWindow::updateDirectoryContent(const QString &path)
{
    directoryContentModel->setRootPath(path);
    directoryContentView->setRootIndex(directoryContentModel->index(path));
}


void MainWindow::syncRightView(const QString &path)
{
    QDir dir(path);

    QFile defaultStructure(path + "/input.xyz");
        if (defaultStructure.exists() && defaultStructure.open(QIODevice::ReadOnly | QIODevice::Text)) {
            structureView->setPlainText(QString::fromUtf8(defaultStructure.readAll()));
            structureFileEdit->setText("input.xyz");
            defaultStructure.close();
        } else {
            // Wenn nicht vorhanden, nach anderen xyz-Dateien suchen
            QStringList xyzFiles = dir.entryList(QStringList() << "*.xyz", QDir::Files);
            if (!xyzFiles.isEmpty()) {
                QFile structureFile(dir.filePath(xyzFiles.first()));
                if (structureFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    structureView->setPlainText(QString::fromUtf8(structureFile.readAll()));
                    structureFileEdit->setText(xyzFiles.first());
                    structureFile.close();
                }
            } else {
                structureView->clear();
                structureFileEdit->setText("input.xyz");  // Setze Standard-Namen
            }
        }

    // Output-Dateien suchen und laden (*.log oder *.out)
    QStringList outputFiles = dir.entryList(QStringList() << "*.log" << "*.out", QDir::Files);
    if (!outputFiles.isEmpty()) {
        QFile outputFile(dir.filePath(outputFiles.first()));
        if (outputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            outputView->setPlainText(QString::fromUtf8(outputFile.readAll()));
            outputFile.close();
        }
    } else {
        outputView->clear();
    }

    // Input-Datei suchen und laden
    QStringList inputFiles = dir.entryList(QStringList() << "input.inp", QDir::Files);
    if (!inputFiles.isEmpty()) {
        QFile inputFile(dir.filePath(inputFiles.first()));
        if (inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            inputView->setPlainText(QString::fromUtf8(inputFile.readAll()));
            inputFileEdit->setText(inputFiles.first());
            inputFile.close();
        }
    } else {
        inputView->clear();
        inputFileEdit->clear();
    }
}

void MainWindow::saveCalculationInfo()
{
    CalculationEntry info;
    info.program = programSelector->currentText();
    info.command = commandInput->text();
    info.structureFile = structureFileEdit->text();
    info.inputFile = inputFileEdit->text();
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
    QFile jsonFile(m_currentCalculationDir + "/calculation.json");
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
            int index = programSelector->findText(json["program"].toString());
            if (index >= 0) {
                programSelector->setCurrentIndex(index);
            }
        }
        
        if (json.contains("command")) {
            commandInput->setText(json["command"].toString());
        }
        
        // ... Weitere Informationen laden
        
        jsonFile.close();
    }
}