// hostprobe.h - Checks that a computer can be used for remote runs
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute): three steps, each reported on its own. 1. Can ssh
// log in without a password prompt. 2. Where is qurcuma-server on that computer, and does it
// start. 3. A real session: the server is started through the tunnel the way a run does it,
// the hello is answered with `welcome`, and a ping measures the round trip. The answer carries
// a hint for the usual failures, so adding a computer does not end in a bare "connection failed"
// at the first run.

#pragma once

#include "hostprofiles.h"

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

#include <functional>

class QWebSocket;

namespace remote {

class SshTunnel;

/// What the test found out; the fields of a step that was not reached keep their defaults.
struct ProbeResult {
    bool sshOk = false;
    QString serverPath;          ///< where qurcuma-server was found (empty: nowhere)
    bool serverRuns = false;     ///< `--help` ran
    bool sessionOk = false;      ///< a session was opened and answered
    int threads = 0;             ///< hardware threads reported by the server
    QStringList gpuBackends;     ///< GPU backends the server was built with and can load
    int rttMs = -1;              ///< round trip of a ping through the tunnel
    QString message;             ///< one line for the whole result
};

class HostProbe : public QObject {
    Q_OBJECT
public:
    enum Step { Login = 0, Server = 1, Session = 2 };
    enum State { Pending = 0, Running, Ok, Failed, Skipped };
    Q_ENUM(Step)
    Q_ENUM(State)

    explicit HostProbe(const HostProfile& profile, QObject* parent = nullptr);
    ~HostProbe() override;

    void start();
    void cancel();

    /// Hint for the usual ssh failures, from ssh's own message (also used by the tests).
    static QString explainSshFailure(const QString& stderrText, const HostProfile& profile);
    /// The script run on the remote side (`sh -s`); it takes no input from this program.
    static QByteArray findServerScript();

signals:
    /// State and one line of detail for a step (the dialog shows them as a list).
    void stepChanged(int step, int state, const QString& text);
    void finished(const remote::ProbeResult& result);

private:
    void runStep(const QStringList& command, const QByteArray& input,
        std::function<void(int, const QString&, const QString&)> done);
    void runSession(const QString& serverCommand);
    void connectSocket();
    void onSocketText(const QString& text);
    void setStep(Step step, State state, const QString& text);
    void finishWith(const QString& message);
    void failSession(const QString& why);

    HostProfile m_profile;
    ProbeResult m_result;
    QProcess* m_process = nullptr;
    SshTunnel* m_tunnel = nullptr;
    QWebSocket* m_socket = nullptr;
    QTimer m_timeout;
    QString m_token;
    quint16 m_port = 0;
    int m_attempts = 0;
    qint64 m_pingSentMs = 0;
    bool m_welcomed = false;
    bool m_done = false;
};

} // namespace remote

Q_DECLARE_METATYPE(remote::ProbeResult)
