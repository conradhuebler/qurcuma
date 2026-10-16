// settingsdialog.h - Settings dialog: external program paths and operator metadata
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#pragma once

#include "programpaths.h"

#include <QDialog>
#include <QVector>

class QComboBox;
class QLabel;
class QLineEdit;
class QTabWidget;
class Settings;

/**
 * @brief One dialog for the application settings, with a "Programs" and an "Operator" tab.
 *
 * Programs: one row per programpaths::specs() entry (path field, Browse, Detect, live status
 * from programpaths::validate()). Operator: name, ORCID (live format and check-digit status),
 * institution, default license. Nothing is written to Settings before OK; Cancel discards.
 * Claude Generated 2026
 */
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    enum Tab { ProgramsTab = 0, OperatorTab = 1 };

    /// Reads the current values from @p settings and opens on @p tab. Claude Generated 2026
    explicit SettingsDialog(Settings& settings, QWidget* parent = nullptr, Tab tab = ProgramsTab);

    /// Writes every field to Settings, then closes. Claude Generated 2026
    void accept() override;

private:
    struct ProgramRow {
        programpaths::ProgramSpec spec;
        QString detected;  ///< candidate found when the dialog opened (placeholder text)
        QLineEdit* edit = nullptr;
        QLabel* icon = nullptr;
        QLabel* status = nullptr;
    };

    QWidget* buildProgramsTab();             ///< one grid row per program. Claude Generated 2026
    QWidget* buildOperatorTab();             ///< name, ORCID, institution, license. Claude Generated 2026
    void browse(int row);                    ///< file or folder dialog into the row's field. Claude Generated 2026
    void detect(int row);                    ///< programpaths::detect() into the row's field. Claude Generated 2026
    void updateProgramStatus(int row);       ///< validate() result into the row's status. Claude Generated 2026
    void updateOrcidStatus();                ///< checkOrcid() result into the ORCID status. Claude Generated 2026

    Settings& m_settings;
    QTabWidget* m_tabs = nullptr;
    QVector<ProgramRow> m_rows;
    QLineEdit* m_name = nullptr;
    QLineEdit* m_orcid = nullptr;
    QLabel* m_orcidIcon = nullptr;
    QLabel* m_orcidStatus = nullptr;
    QLineEdit* m_institution = nullptr;
    QComboBox* m_license = nullptr;
};
