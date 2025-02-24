#pragma once

#include "settings.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDateTime>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMainWindow>
#include <QMap>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "dialogs/nmrspectrumdialog.h"
class MoleculeViewer;


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
    void createToolbars();
    void createMenus();
    void setupProjectViewContextMenu();
    void setupConnections();
    void loadSettings();
    bool checkProgramPath(const QString &program);

    bool setupCalculationDirectory();
    void updateOutputView(const QString& logFile, bool scrollToBottom = false);

    void createNewDirectory();
    void setupProgramSpecificDirectory(const QString &dirPath, const QString &program);
    void updateDirectoryContent(const QString &path);

    QPair<int, int> countImaginaryFrequencies(const QString &filename);
    void initializeProgramCommands();
    void updateCommandLineVisibility(const QString &program);
    void setupContextMenu();
    void openWithVisualizer(const QString &filePath, const QString &visualizer);
    void orcaPlotVib(const QString &outputFile, int freqNumber);
    void syncRightView(const QString &path);
    void saveCalculationInfo();
    void loadCalculationInfo(const QString &path);
    QList<CalculationEntry> loadCalculationHistory(const QString &path);
    void addCalculationToHistory(const CalculationEntry &entry);
    QString generateUniqueFileName(const QString &baseFileName, const QString &extension);
    QString currentCalculationDir() const
    {
        return m_workingDirectory + QDir::separator() + m_currentCalculationDir;
    }
    void setupBookmarkView();
    void updateBookmarkView();
    void switchWorkingDirectory(const QString& path);

    void updatePathLabel(const QString& path);
    void toggleLeftPanel();

    QListWidget* m_bookmarkListView;
    QListView* m_projectListView;
    QListView* m_directoryContentView;
    QLineEdit* m_commandInput;
    QLineEdit *m_inputFileEdit, *m_inputFileEditExtension;
    QLineEdit *m_structureFileEdit, *m_structureFileEditExtension;
    QComboBox* m_programSelector;
    QTextEdit* m_structureView;
    QTextEdit* m_inputView;
    QTextEdit* m_outputView;
    QPushButton *m_newCalculationButton, *m_chooseDirectory, *m_runCalculation;
    QCheckBox* m_uniqueFileNames;
    QSpinBox* m_threads;
    QFileSystemModel* m_projectModel;
    QFileSystemModel* m_directoryContentModel;
    QProcess* m_currentProcess;
    Settings m_settings;
    QMap<QString, QStringList> m_programCommands;
    QCompleter* m_commandCompleter;
    QToolButton* m_bookmarkButton;
    QSplitter* m_splitter;
    QLabel *m_currentPathLabel, *m_currentProjectLabel;
    MoleculeViewer *m_moleculeView;
    NMRSpectrumDialog* m_nmrDialog;

    QStringList m_simulationPrograms{ "curcuma", "orca", "xtb" };
    QStringList m_visualizerPrograms{ "iboview", "avogadro" };

    QString m_workingDirectory;    
    QString m_currentCalculationDir; // Aktuelles Berechnungsverzeichnis
    int m_lastLeftPanelWidth = 0;
    QVector<QPair<int, double>> m_frequencies;
};