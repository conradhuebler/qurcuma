// simulationbackend.cpp - LocalBackend: SimulationWorker in a QThread of this process
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute, stage R0); the thread handling moved here
// unchanged from SimulationControlWidget::startWithConfig.

#include "simulationbackend.h"

#include <QMetaObject>

LocalBackend::LocalBackend(QObject* parent)
    : SimulationBackend(parent)
{
    m_worker = new SimulationWorker;
    m_thread = new QThread(this);
}

LocalBackend::~LocalBackend()
{
    if (m_worker)
        m_worker->requestStop();
    if (m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait(2000);
    }
    // finished() normally deletes the worker from its thread; if the thread never got that
    // far, delete it here (the thread is stopped, so this is safe).
    if (m_worker && !m_thread->isRunning())
        delete m_worker.data();
}

void LocalBackend::setMolecule(const QVector<MolAtom>& atoms) { m_worker->setMolecule(atoms); }
void LocalBackend::setBonds(const QVector<MolBond>& bonds) { m_worker->setBonds(bonds); }
void LocalBackend::setConfig(const SimulationConfig& config) { m_worker->setConfig(config); }
void LocalBackend::setLiveNci(bool enabled) { m_worker->setLiveNci(enabled); }

void LocalBackend::start(bool singleStep)
{
    SimulationWorker* w = m_worker;
    w->moveToThread(m_thread);

    // Worker signals are relayed (queued, the worker lives in its own thread).
    connect(w, &SimulationWorker::frameReady, this, &SimulationBackend::frameReady);
    connect(w, &SimulationWorker::finished, this, &SimulationBackend::finished);
    connect(w, &SimulationWorker::errorOccurred, this, &SimulationBackend::errorOccurred);
    connect(w, &SimulationWorker::runParameters, this, &SimulationBackend::runParameters);
    connect(w, &SimulationWorker::paused, this, &SimulationBackend::paused);

    // Single-shot step: QThread::started -> stepOnce() (not run()).
    if (singleStep)
        connect(m_thread, &QThread::started, w, &SimulationWorker::stepOnce);
    else
        connect(m_thread, &QThread::started, w, &SimulationWorker::run);
    connect(w, &SimulationWorker::finished, m_thread, &QThread::quit);
    connect(w, &SimulationWorker::finished, w, &QObject::deleteLater);

    m_thread->start();
}

void LocalBackend::requestStop() { if (m_worker) m_worker->requestStop(); }
void LocalBackend::requestPause() { if (m_worker) m_worker->requestPause(); }
void LocalBackend::requestResume() { if (m_worker) m_worker->requestResume(); }

// The live controls are queued into the worker thread, as the direct connections were.
void LocalBackend::injectForce(int atomIndex, QVector3D force, double alpha, int maxShells)
{
    if (m_worker)
        QMetaObject::invokeMethod(m_worker, "injectForce", Qt::QueuedConnection,
            Q_ARG(int, atomIndex), Q_ARG(QVector3D, force), Q_ARG(double, alpha), Q_ARG(int, maxShells));
}

void LocalBackend::clearInjectedForce()
{
    if (m_worker)
        QMetaObject::invokeMethod(m_worker, "clearInjectedForce", Qt::QueuedConnection);
}

void LocalBackend::setTargetTemperature(double temperature)
{
    if (m_worker)
        QMetaObject::invokeMethod(m_worker, "setTargetTemperature", Qt::QueuedConnection, Q_ARG(double, temperature));
}

void LocalBackend::setWallTemp(double T)
{
    if (m_worker)
        QMetaObject::invokeMethod(m_worker, "setWallTemp", Qt::QueuedConnection, Q_ARG(double, T));
}

void LocalBackend::setWallBeta(double beta)
{
    if (m_worker)
        QMetaObject::invokeMethod(m_worker, "setWallBeta", Qt::QueuedConnection, Q_ARG(double, beta));
}
