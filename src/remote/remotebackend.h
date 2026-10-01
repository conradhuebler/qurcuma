// remotebackend.h - SimulationBackend that runs on another machine (qurcuma-server)
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 (WP remote compute R2, docs/WP-remote-compute-vr.md).
// Connects to ws://127.0.0.1:<port> (the local end of an ssh tunnel, or a server started
// by hand), authenticates with the token, uploads the files the configuration refers to,
// sends the start message, and relays frames and run state through the
// SimulationBackend signals. The trajectory file (writeTrajectory) is written HERE from
// the received frames; the server never writes one (frames dropped on a slow link are
// missing from it, see "Dateizugriff" in the plan).
//
// There is no fallback to a local run: a failed connection ends the run with
// errorOccurred().

#pragma once

#include "../simulationbackend.h"

#include <QElapsedTimer>
#include <QFile>
#include <QTimer>
#include <QUrl>
#include <QVector>

class QWebSocket;

namespace remote {

class SshTunnel;

class RemoteBackend : public SimulationBackend {
    Q_OBJECT
public:
    /// Connect through ssh: starts qurcuma-server on @p host with @p serverCommand.
    RemoteBackend(const QString& host, const QString& serverCommand, QObject* parent = nullptr);
    /// Connect to a server that is already listening (tests, manual tunnels).
    RemoteBackend(const QUrl& url, const QString& token, QObject* parent = nullptr);
    ~RemoteBackend() override;

    void setMolecule(const QVector<MolAtom>& atoms) override { m_atoms = atoms; }
    void setBonds(const QVector<MolBond>& bonds) override { m_bonds = bonds; }
    void setConfig(const SimulationConfig& config) override { m_config = config; }
    void setLiveNci(bool enabled) override { m_liveNci = enabled; }
    void start(bool singleStep = false) override;
    void requestStop() override;
    void requestPause() override;
    void requestResume() override;

    /// Also let the server write the complete trajectory into its session directory (kept
    /// there; fetch it with RemoteFiles / the Remote Files dialog). Default off.
    void setServerTrajectory(bool on) { m_serverTrajectory = on; }

    /// Where the trajectory is written (default: remote-<timestamp>.trj.xyz in the working directory).
    void setTrajectoryPath(const QString& path) { m_trajectoryPath = path; }
    QString trajectoryPath() const { return m_trajectoryPath; }

public slots:
    void injectForce(int atomIndex, QVector3D force, double alpha, int maxShells) override;
    void clearInjectedForce() override;
    void setTargetTemperature(double temperature) override;
    void setWallTemp(double T) override;
    void setWallBeta(double beta) override;

signals:
    /// Diagnostic text (also appended to remote::logFilePath()).
    void logMessage(const QString& text);
    /// What the server reported at connect: gpuBackends (array), threads.
    void capabilities(const QJsonObject& welcome);
    /// Short link description for a status line ("Remote host: 12 frames, 0 dropped ...").
    void statusText(const QString& text);

private:
    struct Upload { QString name; QByteArray data; };

    void connectSocket();
    void onConnected();
    void onText(const QString& message);
    void onBinary(const QByteArray& message);
    void onSocketError();
    void beginReconnect();
    void tryReconnect();
    void sendStart();
    void log(const QString& text);
    void sendJson(const QJsonObject& obj);
    bool prepareUploads(QJsonObject& configJson, QString* error);
    void endRun(const QString& reason, bool aborted, const QJsonObject& stats = {});
    void writeTrajectoryFrame(const SimulationFrame& f);

    QString m_host, m_serverCommand;
    QUrl m_url;
    QString m_token;
    SshTunnel* m_tunnel = nullptr;
    QWebSocket* m_socket = nullptr;
    QTimer m_retry;
    QTimer m_reconnectTimer;
    QElapsedTimer m_lostTimer;
    bool m_reconnecting = false;
    QString m_sessionName;
    QElapsedTimer m_connectTimer;

    QVector<MolAtom> m_atoms;
    QVector<MolBond> m_bonds;
    SimulationConfig m_config;
    bool m_liveNci = false;
    bool m_singleStep = false;
    bool m_serverTrajectory = false;

    QJsonObject m_startConfig;  // config JSON with file parameters rewritten to upload names
    QVector<Upload> m_uploads;
    int m_pendingStored = 0;
    bool m_welcomed = false;
    bool m_started = false;
    bool m_finished = false;
    bool m_stopRequested = false;

    QString m_trajectoryPath;
    QFile m_trajectory;
    quint64 m_framesReceived = 0;
    int m_serverThreads = 0;
    QElapsedTimer m_statusTimer;
};

} // namespace remote
