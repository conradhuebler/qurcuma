// addcomputerdialog.h - Add or edit a computer for remote runs
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute): name, address, port and key of a computer, a test
// in three visible steps (ssh login, qurcuma-server found, a session opened), and the host list
// shared by the Simulation dock and the Remote Files dialog. Saved computers live in the qurcuma
// settings (remote/hostprofiles.h); ~/.ssh/config is only read.

#pragma once

#include "remote/hostprobe.h"
#include "remote/hostprofiles.h"

#include <QDialog>
#include <QIcon>
#include <QStyle>

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
    /// With @p edit the dialog changes that saved computer instead of adding a new one. With
    /// @p testOnly it tests @p edit as it is (address, port and key are fixed, also for an entry
    /// of ~/.ssh/config) and only the server command can be changed; nothing is saved but that.
    explicit AddComputerDialog(QWidget* parent = nullptr, const std::optional<remote::HostProfile>& edit = std::nullopt,
        bool testOnly = false);
    ~AddComputerDialog() override;

    /// The profile as entered (valid after accept()).
    remote::HostProfile profile() const;

    /// Fills @p combo with "This computer" (when @p withLocal), the computers of ~/.ssh/config
    /// (patterns left out) and the saved ones, and selects @p select when it is in the list.
    /// The item data is the host name that SshTunnel takes; saved computers are marked in the
    /// tool tip and by `isSavedComputer()`.
    static void fillHostCombo(QComboBox* combo, bool withLocal, const QString& select = QString());
    static bool isSavedComputer(const QString& name);
    /// Server command to show for @p host when the user has not typed one for it before.
    static QString defaultServerCommand(const QString& host);
    /// An icon of the desktop theme, or the style's standard icon when the theme has none.
    static QIcon icon(const QString& themeName, QStyle::StandardPixmap fallback);

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
    bool m_testOnly = false;  ///< address, port and key are fixed
};
