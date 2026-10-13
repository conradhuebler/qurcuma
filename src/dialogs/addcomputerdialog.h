// addcomputerdialog.h - Add a computer for remote runs
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute): name, address, port and key of a computer, a test
// that checks ssh and finds qurcuma-server, and the host list shared by the Simulation dock and
// the Remote Files dialog. Saved computers live in the qurcuma settings (remote/hostprofiles.h);
// ~/.ssh/config is only read.

#pragma once

#include "remote/hostprofiles.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace remote {
class HostProbe;
}

class AddComputerDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddComputerDialog(QWidget* parent = nullptr);

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

private:
    void runTest();
    void onProbeFinished(bool sshOk, const QString& serverPath, bool serverRuns, const QString& message);
    void updateButtons();
    void tryAccept();

    QLineEdit *m_name, *m_address, *m_key, *m_serverCommand;
    QSpinBox* m_port;
    QLabel* m_status;
    QPushButton *m_test, *m_ok;
    remote::HostProbe* m_probe = nullptr;
};
