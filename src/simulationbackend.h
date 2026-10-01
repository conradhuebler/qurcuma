// simulationbackend.h - Where a simulation runs: this machine or a remote one
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 (WP remote compute, stage R0). The Simulation dock and
// MainWindow talk to a SimulationBackend, not to SimulationWorker directly. LocalBackend
// runs the worker in a QThread of this process (behaviour as before); a RemoteBackend
// (stage R2, docs/WP-remote-compute-vr.md) will run it on another machine. The interface
// is exactly the worker's public surface: inputs before start(), live controls while
// running, and the frame/finish signals.

#pragma once

#include "moleculetypes.h"
#include "simulationframe.h"
#include "simulationworker.h"  // SimulationConfig

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QVector>
#include <QVector3D>

class SimulationBackend : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

    /// Inputs; call before start().
    virtual void setMolecule(const QVector<MolAtom>& atoms) = 0;
    virtual void setBonds(const QVector<MolBond>& bonds) = 0;
    virtual void setConfig(const SimulationConfig& config) = 0;
    virtual void setLiveNci(bool enabled) = 0;

    /// Start the run (MD or optimisation per the config), or exactly one step.
    virtual void start(bool singleStep = false) = 0;

    /// Thread-safe run control.
    virtual void requestStop() = 0;
    virtual void requestPause() = 0;
    virtual void requestResume() = 0;

public slots:
    /// Live controls, same meaning as the SimulationWorker slots of the same name.
    virtual void injectForce(int atomIndex, QVector3D force, double alpha, int maxShells) = 0;
    virtual void clearInjectedForce() = 0;
    virtual void setTargetTemperature(double temperature) = 0;
    virtual void setWallTemp(double T) = 0;
    virtual void setWallBeta(double beta) = 0;

signals:
    void frameReady(SimulationFramePtr frame);
    void finished(const QString& reason = QString(), bool aborted = false);
    void errorOccurred(QString message);
    void runParameters(const QJsonObject& record);
    void paused();
};

/// Runs a SimulationWorker in a QThread of this process.
class LocalBackend : public SimulationBackend {
    Q_OBJECT
public:
    explicit LocalBackend(QObject* parent = nullptr);
    ~LocalBackend() override;

    void setMolecule(const QVector<MolAtom>& atoms) override;
    void setBonds(const QVector<MolBond>& bonds) override;
    void setConfig(const SimulationConfig& config) override;
    void setLiveNci(bool enabled) override;
    void start(bool singleStep = false) override;
    void requestStop() override;
    void requestPause() override;
    void requestResume() override;

public slots:
    void injectForce(int atomIndex, QVector3D force, double alpha, int maxShells) override;
    void clearInjectedForce() override;
    void setTargetTemperature(double temperature) override;
    void setWallTemp(double T) override;
    void setWallBeta(double beta) override;

private:
    QPointer<SimulationWorker> m_worker;  // deleteLater()'d by the worker thread after finished
    QThread* m_thread = nullptr;          // child of this backend
};
