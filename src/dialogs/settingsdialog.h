// settingsdialog.h - Settings dialog: programs, general, display, computers, operator metadata
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#pragma once

#include "programpaths.h"

#include <QDialog>
#include <QStringList>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTabWidget;
class QTreeWidget;
class Settings;

/**
 * @brief One dialog for the application settings.
 *
 * Tabs: Programs (path, Browse, Detect, live status per program), General (working directory and
 * the list of known ones, invocation directory, dark mode, clearing the recent-files list),
 * Display (centre of mass to the origin after loading), Computers (only with QURCUMA_REMOTE:
 * the computers saved for remote runs) and Operator (name, ORCID, institution, license).
 * Nothing is written to Settings before OK and Cancel discards, except on the Computers tab,
 * whose Add / Edit / Remove act at once. The dialog does not know MainWindow: changes that must
 * reach the running application leave through the signals below.
 * Claude Generated 2026
 */
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    enum Tab { ProgramsTab, GeneralTab, DisplayTab, ComputersTab, OperatorTab, TabCount };

    /// Reads the current values from @p settings and opens on @p tab (Programs when @p tab is not
    /// built). Claude Generated 2026
    explicit SettingsDialog(Settings& settings, QWidget* parent = nullptr, Tab tab = ProgramsTab);

    /// The working directory in use; shown on the General tab and compared with the choice on OK.
    /// Without this call the last used one from Settings is shown. Claude Generated 2026
    void setCurrentWorkingDirectory(const QString& path);

    /// Writes every field to Settings, emits the signals for what changed, then closes.
    /// Claude Generated 2026
    void accept() override;

signals:
    /// Dark mode was switched (emitted from accept()). Claude Generated 2026
    void darkModeChanged(bool enabled);
    /// "Use the directory qurcuma was started from" was switched (from accept()). Claude Generated 2026
    void useInvocationDirectoryChanged(bool enabled);
    /// Another working directory was chosen (from accept()). Claude Generated 2026
    void workingDirectoryChosen(const QString& path);
    /// "Move the centre of mass to the origin after loading" was switched (from accept()).
    /// Claude Generated 2026
    void centerOnLoadChanged(bool enabled);
    /// The recent-files list was cleared in Settings (from accept()). Claude Generated 2026
    void recentFilesCleared();
    /// A computer for remote runs was added, changed or removed (at once, not on OK).
    /// Claude Generated 2026
    void computersChanged();

private:
    struct ProgramRow {
        programpaths::ProgramSpec spec;
        QString detected;  ///< candidate found when the dialog opened (placeholder text)
        QLineEdit* edit = nullptr;
        QLabel* icon = nullptr;
        QLabel* status = nullptr;
    };

    QWidget* buildProgramsTab();             ///< one grid row per program. Claude Generated 2026
    QWidget* buildGeneralTab();              ///< working directory, dark mode, recent files. Claude Generated 2026
    QWidget* buildDisplayTab();              ///< loading behaviour of the viewer. Claude Generated 2026
#ifdef QURCUMA_REMOTE
    QWidget* buildComputersTab();            ///< computers for remote runs. Claude Generated 2026
#endif
    QWidget* buildOperatorTab();             ///< name, ORCID, institution, license. Claude Generated 2026
    void browse(int row);                    ///< file or folder dialog into the row's field. Claude Generated 2026
    void detect(int row);                    ///< programpaths::detect() into the row's field. Claude Generated 2026
    void updateProgramStatus(int row);       ///< validate() result into the row's status. Claude Generated 2026
    void updateOrcidStatus();                ///< checkOrcid() result into the ORCID status. Claude Generated 2026
    void browseWorkingDirectory();           ///< folder dialog; the choice is applied on OK. Claude Generated 2026
    void useSelectedWorkingDirectory();      ///< the selected list entry becomes the choice. Claude Generated 2026
    void removeSelectedWorkingDirectory();   ///< drops the entry from the list (from Settings on OK). Claude Generated 2026
    void updateWorkingDirectoryView();       ///< current/chosen path, note and button states. Claude Generated 2026
    void clearRecentFiles();                 ///< confirms, then marks the list for clearing on OK. Claude Generated 2026
#ifdef QURCUMA_REMOTE
    void fillComputerList(const QString& select = QString());  ///< saved computers into the tree. Claude Generated 2026
    void addComputer();                      ///< AddComputerDialog; saved at once. Claude Generated 2026
    void editComputer();                     ///< AddComputerDialog on the selected computer. Claude Generated 2026
    void removeComputer();                   ///< confirms, then removes the selected computer. Claude Generated 2026
#endif

    Settings& m_settings;
    QTabWidget* m_tabs = nullptr;
    QWidget* m_pages[TabCount] = {};
    QVector<ProgramRow> m_rows;
    QLineEdit* m_name = nullptr;
    QLineEdit* m_orcid = nullptr;
    QLabel* m_orcidIcon = nullptr;
    QLabel* m_orcidStatus = nullptr;
    QLineEdit* m_institution = nullptr;
    QComboBox* m_license = nullptr;

    // General
    QString m_currentWorkDir;   ///< the directory in use when the dialog opened
    QString m_chosenWorkDir;    ///< the directory to switch to on OK (empty = keep the current one)
    QStringList m_removedWorkDirs;  ///< list entries removed in the dialog, removed from Settings on OK
    QLineEdit* m_workDirEdit = nullptr;
    QLabel* m_workDirNote = nullptr;
    QListWidget* m_workDirList = nullptr;
    QPushButton* m_useWorkDirBtn = nullptr;
    QPushButton* m_removeWorkDirBtn = nullptr;
    QCheckBox* m_useInvocationDirCheck = nullptr;
    QCheckBox* m_darkModeCheck = nullptr;
    QLabel* m_recentLabel = nullptr;
    QPushButton* m_clearRecentBtn = nullptr;
    bool m_clearRecent = false;  ///< clear the recent-files list on OK

    // Display
    QCheckBox* m_centerOnLoadCheck = nullptr;

#ifdef QURCUMA_REMOTE
    QTreeWidget* m_computerTree = nullptr;
    QPushButton* m_editComputerBtn = nullptr;
    QPushButton* m_removeComputerBtn = nullptr;
#endif
};
