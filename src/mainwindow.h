#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListView>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProcess>
#include <QFileSystemModel>
#include <QSplitter>
#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include "settings.h"

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
  
private:
    void setupUI();
    void createMenus();
    void setupConnections();
    void loadSettings();
    bool checkProgramPath(const QString &program);

    bool setupCalculationDirectory();
    void updateOutputView(const QString &logFile);

    void createNewDirectory();
    void setupProgramSpecificDirectory(const QString &dirPath, const QString &program);
    void updateDirectoryContent(const QString &path);

  
    void initializeProgramCommands();
    void updateCommandLineVisibility(const QString &program);
    void setupContextMenu();
    void openWithVisualizer(const QString &filePath, const QString &visualizer);
    void syncRightView(const QString &path);
    void saveCalculationInfo();
    void loadCalculationInfo(const QString &path);
    QList<CalculationEntry> loadCalculationHistory(const QString &path);
    void addCalculationToHistory(const CalculationEntry &entry);
    QString generateUniqueFileName(const QString &baseFileName, const QString &extension);

    QListView *projectListView;
    QLineEdit *commandInput, *inputFileEdit, *structureFileEdit;
    QComboBox *programSelector;
    QTextEdit *structureView;
    QTextEdit *inputView;
    QTextEdit *outputView;
    QPushButton *newCalculationButton;  // Neuer Button
    QFileSystemModel *projectModel;
    QProcess *m_currentProcess;
    Settings settings;

    QPushButton *newDirectoryButton;
    QListView *directoryContentView;
    QFileSystemModel *directoryContentModel;
    QMap<QString, QStringList> programCommands;
    QCompleter *commandCompleter;
  


    // Listen für verschiedene Programmtypen
    QStringList simulationPrograms{"curcuma", "orca", "xtb"};
    QStringList visualizerPrograms{"iboview", "avogadro"};

    QString m_workingDirectory;    
    QString m_currentCalculationDir; // Aktuelles Berechnungsverzeichnis

};

#endif
