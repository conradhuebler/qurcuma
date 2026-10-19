// addcomputerdialog.h - Add or edit a computer for remote runs
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute): name, address, port and key of a computer, a test
// in three visible steps (ssh login, qurcuma-server found, a session opened), and the host list
// of the Simulation dock. Saved computers live in the qurcuma
// settings (remote/hostprofiles.h); ~/.ssh/config is not read.

#pragma once

#include "remote/hostprobe.h"
#include "remote/hostprofiles.h"

#include <QDialog>

#include <optional>

class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;

class AddComputerDialog : public QDialog {
    Q_OBJECT
public:
    /// With @p edit the dialog changes that saved computer instead of adding a new one.
    explicit AddComputerDialog(QWidget* parent = nullptr, const std::optional<remote::HostProfile>& edit = std::nullopt);
    ~AddComputerDialog() override;

    /// The profile as entered (valid after accept()).
    remote::HostProfile profile() const;

    /// Fills @p combo with "This computer" (when @p withLocal) and the computers saved in qurcuma,
    /// and selects @p select when it is in the list. The item data is the name that SshTunnel
    /// takes; the tool tip is the address. ~/.ssh/config is not read.
    static void fillHostCombo(QComboBox* combo, bool withLocal, const QString& select = QString());
    static bool isSavedComputer(const QString& name);
    /// Server command to show for @p host when the user has not typed one for it before.
    static QString defaultServerCommand(const QString& host);

private:
    void toggleTest();
    void startTest();
    void stopTest();
    void onStep(int step, int state, const QString& text);
    void onProbeFinished(const remote::ProbeResult& result);
    void setBusy(bool busy);
    void updateButtons();
    void tryAccept();

    QLineEdit *m_name, *m_address, *m_key, *m_serverCommand;
    QSpinBox* m_port;
    QLabel* m_stepLabels[3];
    QLabel* m_stepIcons[3];
    QLabel *m_summary, *m_problem;
    QProgressBar* m_progress;
    QPushButton *m_test, *m_browse, *m_ok;
    remote::HostProbe* m_probe = nullptr;
    QString m_originalName;  ///< set when editing
    bool m_tested = false, m_testedOk = false;
    bool m_filling = false;  ///< the test itself fills in a field
};
