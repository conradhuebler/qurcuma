// hostprobe.h - Checks that a computer can be used for remote runs
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute): two ssh calls in the background. First "can ssh
// log in without a password prompt", then "where is qurcuma-server on that computer, and does
// it start". The answer carries a hint for the usual failures, so adding a computer does not
// end in a bare "connection failed" at the first run.

#pragma once

#include "hostprofiles.h"

#include <QObject>
#include <QProcess>
#include <QTimer>

namespace remote {

class HostProbe : public QObject {
    Q_OBJECT
public:
    explicit HostProbe(const HostProfile& profile, QObject* parent = nullptr);
    ~HostProbe() override;

    void start();
    void cancel();

    /// Hint for the usual ssh failures, from ssh's own message (also used by the tests).
    static QString explainSshFailure(const QString& stderrText, const HostProfile& profile);
    /// The script run on the remote side (`sh -s`); it takes no input from this program.
    static QByteArray findServerScript();

signals:
    /// What is being tried, for the status line.
    void progress(const QString& text);
    /// @p serverPath is empty when ssh works but no server was found (or ssh failed).
    void finished(bool sshOk, const QString& serverPath, bool serverRuns, const QString& message);

private:
    void runStep(const QStringList& command, const QByteArray& input, std::function<void(int, const QString&, const QString&)> done);
    void finishWith(bool sshOk, const QString& path, bool runs, const QString& message);

    HostProfile m_profile;
    QProcess* m_process = nullptr;
    QTimer m_timeout;
    bool m_done = false;
};

} // namespace remote
