// simulationcontrolwidget.cpp - Compact inline simulation controls
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Interactive Simulation Integration

#include "simulationcontrolwidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

SimulationControlWidget::SimulationControlWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

SimulationControlWidget::~SimulationControlWidget()
{
    // Stop any running simulation cleanly
    if (m_worker)
        m_worker->requestStop();
    if (m_thread && m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait(2000);
    }
}

void SimulationControlWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(3);

    // --- Top row: controls ---
    auto* topRow = new QHBoxLayout;

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("MD"), static_cast<int>(SimulationConfig::Mode::MolecularDynamics));
    m_modeCombo->addItem(tr("Optimization"), static_cast<int>(SimulationConfig::Mode::GeometryOptimization));
    m_modeCombo->setToolTip(tr("Simulation type"));
    topRow->addWidget(m_modeCombo);

    m_methodCombo = new QComboBox(this);
    m_methodCombo->addItem("GFN-FF", "gfnff");   // Default - always available
    m_methodCombo->addItem("UFF", "uff");
    m_methodCombo->addItem("GFN2", "gfn2");
    m_methodCombo->addItem("GFN1", "gfn1");
    m_methodCombo->setToolTip(tr("Force field / method"));
    topRow->addWidget(m_methodCombo);

    m_tempSpin = new QDoubleSpinBox(this);
    m_tempSpin->setRange(1.0, 5000.0);
    m_tempSpin->setValue(300.0);
    m_tempSpin->setSuffix(" K");
    m_tempSpin->setDecimals(0);
    m_tempSpin->setToolTip(tr("Temperature (MD only)"));
    m_tempSpin->setMaximumWidth(80);
    topRow->addWidget(m_tempSpin);

    m_stepsSpin = new QSpinBox(this);
    m_stepsSpin->setRange(10, 1000000);
    m_stepsSpin->setValue(1000);
    m_stepsSpin->setSuffix(tr(" steps"));
    m_stepsSpin->setToolTip(tr("Total steps (MD) or max iterations (optimization)"));
    m_stepsSpin->setMaximumWidth(100);
    topRow->addWidget(m_stepsSpin);

    m_startBtn = new QPushButton(tr("▶"), this);
    m_startBtn->setToolTip(tr("Start simulation"));
    m_startBtn->setMaximumWidth(32);
    topRow->addWidget(m_startBtn);

    m_pauseBtn = new QPushButton(tr("⏸"), this);
    m_pauseBtn->setToolTip(tr("Pause / Resume"));
    m_pauseBtn->setMaximumWidth(32);
    m_pauseBtn->setEnabled(false);
    topRow->addWidget(m_pauseBtn);

    m_stopBtn = new QPushButton(tr("■"), this);
    m_stopBtn->setToolTip(tr("Stop simulation"));
    m_stopBtn->setMaximumWidth(32);
    m_stopBtn->setEnabled(false);
    topRow->addWidget(m_stopBtn);

    m_moreBtn = new QPushButton(tr("⚙"), this);
    m_moreBtn->setToolTip(tr("Open full simulation dialog"));
    m_moreBtn->setMaximumWidth(32);
    topRow->addWidget(m_moreBtn);

    mainLayout->addLayout(topRow);

    // --- Status row ---
    m_statusLabel = new QLabel(tr("Ready"), this);
    m_statusLabel->setStyleSheet("color: gray; font-size: 11px;");
    mainLayout->addWidget(m_statusLabel);

    // --- Connections ---
    connect(m_startBtn, &QPushButton::clicked, this, &SimulationControlWidget::onStartClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &SimulationControlWidget::onPauseClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &SimulationControlWidget::onStopClicked);
    connect(m_moreBtn, &QPushButton::clicked, this, &SimulationControlWidget::openFullDialog);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &SimulationControlWidget::onModeChanged);

    // Claude Generated - emit configChanged so MainWindow can keep shared config in sync
    auto notifyConfig = [this]() { emit configChanged(buildConfig()); };
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    connect(m_methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    connect(m_tempSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_stepsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notifyConfig);
}

SimulationConfig SimulationControlWidget::buildConfig() const
{
    SimulationConfig cfg;
    cfg.mode = static_cast<SimulationConfig::Mode>(m_modeCombo->currentData().toInt());
    cfg.method = m_methodCombo->currentData().toString();
    cfg.temperature = m_tempSpin->value();
    cfg.steps = m_stepsSpin->value();
    // dumpFrequency=1: pass every step to callback; flood guard (8ms) prevents sub-ms overload
    cfg.dumpFrequency = 1;
    cfg.fpsLimit = 30;  // Normal: cap GUI updates at 30 fps
    return cfg;
}

void SimulationControlWidget::onStartClicked()
{
    if (m_atoms.isEmpty()) {
        m_statusLabel->setText(tr("No molecule loaded."));
        return;
    }

    // Clean up previous worker/thread if any
    if (m_thread && m_thread->isRunning()) {
        if (m_worker)
            m_worker->requestStop();
        m_thread->quit();
        m_thread->wait(2000);
    }

    m_worker = new SimulationWorker;
    m_worker->setMolecule(m_atoms);
    m_worker->setConfig(buildConfig());

    m_thread = new QThread(this);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &SimulationWorker::run);
    connect(m_worker, &SimulationWorker::frameReady, this, &SimulationControlWidget::onFrameReady);
    connect(m_worker, &SimulationWorker::finished, this, &SimulationControlWidget::onSimulationFinished);
    connect(m_worker, &SimulationWorker::errorOccurred, this, [this](const QString& msg) {
        m_statusLabel->setText(tr("Error: %1").arg(msg));
        onSimulationFinished();
    });
    connect(m_worker, &SimulationWorker::finished, m_thread, &QThread::quit);
    connect(m_worker, &SimulationWorker::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

    m_config = buildConfig(); // Cache for status display

    // Claude Generated - Expose worker to MainWindow so it can wire worker->view directly.
    // Must happen BEFORE setRunning emits running=true (so listeners can connect in response).
    emit workerStarted(m_worker);

    setRunning(true);
    m_statusLabel->setText(tr("Starting..."));
    m_thread->start();
}

void SimulationControlWidget::onPauseClicked()
{
    if (!m_worker)
        return;
    if (m_paused) {
        m_worker->requestResume();
        m_pauseBtn->setText(tr("⏸"));
        m_statusLabel->setText(tr("Running..."));
        m_paused = false;
    } else {
        m_worker->requestPause();
        m_pauseBtn->setText(tr("▶"));
        m_statusLabel->setText(tr("Paused"));
        m_paused = true;
    }
}

void SimulationControlWidget::onStopClicked()
{
    if (m_worker)
        m_worker->requestStop();
}

void SimulationControlWidget::onFrameReady(SimulationFramePtr frame)
{
    if (!frame)
        return;

    // Claude Generated - No forwarding: widget only updates its own status label.
    // View + MainWindow receive the frame directly from the worker via MainWindow's wiring.
    if (m_config.mode == SimulationConfig::Mode::MolecularDynamics) {
        double tempK = frame->ekin > 0 && !m_atoms.isEmpty()
            ? frame->ekin * 315775.0 / m_atoms.size()
            : 0.0;
        m_statusLabel->setText(
            tr("Step %1 | E= %2 Eh | T≈ %3 K")
                .arg(frame->step)
                .arg(frame->energy, 0, 'f', 6)
                .arg(tempK, 0, 'f', 0));
    } else {
        m_statusLabel->setText(
            tr("Iter %1 | E= %2 Eh")
                .arg(frame->step)
                .arg(frame->energy, 0, 'f', 8));
    }
}

void SimulationControlWidget::onSimulationFinished()
{
    setRunning(false);
    m_worker = nullptr;
    m_thread = nullptr;
    m_paused = false;
    m_statusLabel->setText(tr("Finished."));
    emit simulationFinished();
}

void SimulationControlWidget::onModeChanged(int /*index*/)
{
    bool isMD = (m_modeCombo->currentData().toInt()
        == static_cast<int>(SimulationConfig::Mode::MolecularDynamics));
    m_tempSpin->setEnabled(isMD);
}

// Claude Generated - Sync UI controls from shared config; QSignalBlocker prevents configChanged loop
void SimulationControlWidget::applyConfig(const SimulationConfig& cfg)
{
    QSignalBlocker b1(m_modeCombo), b2(m_methodCombo), b3(m_tempSpin), b4(m_stepsSpin);
    int modeIdx = m_modeCombo->findData(static_cast<int>(cfg.mode));
    if (modeIdx >= 0)
        m_modeCombo->setCurrentIndex(modeIdx);
    int methIdx = m_methodCombo->findData(cfg.method);
    if (methIdx >= 0)
        m_methodCombo->setCurrentIndex(methIdx);
    m_tempSpin->setValue(cfg.temperature);
    m_stepsSpin->setValue(cfg.steps);
    bool isMD = (cfg.mode == SimulationConfig::Mode::MolecularDynamics);
    m_tempSpin->setEnabled(isMD);
}

void SimulationControlWidget::setRunning(bool running)
{
    m_startBtn->setEnabled(!running);
    m_pauseBtn->setEnabled(running);
    m_stopBtn->setEnabled(running);
    m_modeCombo->setEnabled(!running);
    m_methodCombo->setEnabled(!running);
    m_tempSpin->setEnabled(!running);
    m_stepsSpin->setEnabled(!running);

    // Claude Generated - Notify view so per-atom pickers can be disabled during sim.
    emit simulationRunningChanged(running);
}

