// simulationcontrolwidget.cpp - Full inline simulation controls
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Interactive Simulation Integration (Phase 6)

#include "simulationcontrolwidget.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QVBoxLayout>

SimulationControlWidget::SimulationControlWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

SimulationControlWidget::~SimulationControlWidget()
{
    if (m_worker)
        m_worker->requestStop();
    if (m_thread && m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait(2000);
    }
}

void SimulationControlWidget::setMolecule(
    const QVector<MoleculeViewer::Atom>& atoms,
    const QVector<MoleculeViewer::Bond>& bonds)
{
    m_atoms = atoms;
    m_bonds = bonds;
}

void SimulationControlWidget::setupUI()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(4, 4, 4, 4);
    outer->setSpacing(4);

    // Scroll container so the dock stays usable at small heights.
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* inner = new QWidget(scroll);
    auto* innerLayout = new QVBoxLayout(inner);
    innerLayout->setContentsMargins(0, 0, 0, 0);
    innerLayout->setSpacing(6);

    // ---- Mode + method row ----
    auto* typeForm = new QFormLayout;
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("Molecular Dynamics"),
        static_cast<int>(SimulationConfig::Mode::MolecularDynamics));
    m_modeCombo->addItem(tr("Geometry Optimization"),
        static_cast<int>(SimulationConfig::Mode::GeometryOptimization));
    typeForm->addRow(tr("Mode:"), m_modeCombo);

    m_methodCombo = new QComboBox(this);
    m_methodCombo->addItem("GFN-FF", "gfnff");
    m_methodCombo->addItem("UFF", "uff");
    m_methodCombo->addItem("GFN2", "gfn2");
    m_methodCombo->addItem("GFN1", "gfn1");
    typeForm->addRow(tr("Method:"), m_methodCombo);

    // Claude Generated 2026 - optimizer-algorithm picker (opt mode only)
    m_optimizerCombo = new QComboBox(this);
    m_optimizerCombo->addItem(tr("Auto"), "auto");
    m_optimizerCombo->addItem(tr("LBFGS++"), "lbfgspp");
    m_optimizerCombo->addItem(tr("Native L-BFGS"), "native_lbfgs");
    m_optimizerCombo->addItem(tr("DIIS"), "native_diis");
    m_optimizerCombo->addItem(tr("RFO"), "native_rfo");
    m_optimizerCombo->addItem(tr("ANCOpt"), "ancopt");
    m_optimizerCombo->setToolTip(tr("Optimization algorithm (geometry optimization only)"));
    typeForm->addRow(tr("Optimizer:"), m_optimizerCombo);

    innerLayout->addLayout(typeForm);

    // ---- Run buttons (moved to top) ----
    auto* btnRow = new QHBoxLayout;
    m_startBtn = new QPushButton(tr("▶ Start"), this);
    m_pauseBtn = new QPushButton(tr("⏸"), this);
    m_stopBtn = new QPushButton(tr("■"), this);
    m_pauseBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_pauseBtn);
    btnRow->addWidget(m_stopBtn);
    innerLayout->addLayout(btnRow);

    // ---- Status (directly under buttons) ----
    m_statusLabel = new QLabel(tr("Ready"), this);
    m_statusLabel->setStyleSheet("color: gray; font-size: 11px;");
    m_statusLabel->setWordWrap(true);
    innerLayout->addWidget(m_statusLabel);

    // ---- MD parameters ----
    auto* mdGroup = new QGroupBox(tr("MD Parameters"), this);
    auto* mdForm = new QFormLayout(mdGroup);

    m_tempSpin = new QDoubleSpinBox(this);
    m_tempSpin->setRange(1.0, 5000.0);
    m_tempSpin->setValue(300.0);
    m_tempSpin->setSuffix(" K");
    m_tempSpin->setDecimals(0);
    mdForm->addRow(tr("Temperature:"), m_tempSpin);

    m_timestepSpin = new QDoubleSpinBox(this);
    m_timestepSpin->setRange(0.1, 10.0);
    m_timestepSpin->setValue(1.0);
    m_timestepSpin->setSuffix(" fs");
    m_timestepSpin->setDecimals(1);
    mdForm->addRow(tr("Time step:"), m_timestepSpin);

    m_stepsSpin = new QSpinBox(this);
    m_stepsSpin->setRange(10, 10000000);
    m_stepsSpin->setValue(10000);
    m_stepsSpin->setSuffix(tr(" steps"));
    mdForm->addRow(tr("Total steps:"), m_stepsSpin);

    m_fpsLimitSpin = new QSpinBox(this);
    m_fpsLimitSpin->setRange(1, 240);
    m_fpsLimitSpin->setValue(30);
    m_fpsLimitSpin->setSuffix(tr(" fps"));
    m_fpsLimitSpin->setToolTip(tr("Max GUI update rate"));
    mdForm->addRow(tr("Display rate:"), m_fpsLimitSpin);

    m_gpuCombo = new QComboBox(this);
    m_gpuCombo->addItem(tr("CPU (none)"), "none");
    m_gpuCombo->addItem(tr("CUDA"), "cuda");
    m_gpuCombo->addItem(tr("Auto"), "auto");
    mdForm->addRow(tr("GPU:"), m_gpuCombo);

    m_writeTrjCheck = new QCheckBox(tr("Write .trj.xyz"), this);
    mdForm->addRow("", m_writeTrjCheck);

    m_perfCheck = new QCheckBox(tr("Performance analysis"), this);
    mdForm->addRow("", m_perfCheck);

    innerLayout->addWidget(mdGroup);

    // ---- Opt parameters ----
    auto* optGroup = new QGroupBox(tr("Optimization"), this);
    auto* optForm = new QFormLayout(optGroup);
    m_convergenceSpin = new QDoubleSpinBox(this);
    m_convergenceSpin->setRange(1e-10, 1e-2);
    m_convergenceSpin->setDecimals(10);
    m_convergenceSpin->setSingleStep(1e-7);
    m_convergenceSpin->setValue(1e-6);
    optForm->addRow(tr("Gradient tol:"), m_convergenceSpin);
    innerLayout->addWidget(optGroup);

    // ---- Interactive grab ----
    auto* grabGroup = new QGroupBox(tr("Interactive Grab"), this);
    auto* grabForm = new QFormLayout(grabGroup);

    m_grabStrengthSpin = new QDoubleSpinBox(this);
    m_grabStrengthSpin->setRange(1e-4, 1.0);
    m_grabStrengthSpin->setDecimals(4);
    m_grabStrengthSpin->setSingleStep(1e-3);
    m_grabStrengthSpin->setValue(0.01);
    m_grabStrengthSpin->setToolTip(tr("World-space force per screen pixel (Eh/Bohr)"));
    grabForm->addRow(tr("Strength:"), m_grabStrengthSpin);

    m_grabAlphaSpin = new QDoubleSpinBox(this);
    m_grabAlphaSpin->setRange(0.0, 1.0);
    m_grabAlphaSpin->setDecimals(2);
    m_grabAlphaSpin->setSingleStep(0.05);
    m_grabAlphaSpin->setValue(0.4);
    m_grabAlphaSpin->setToolTip(tr("Shell decay α^depth"));
    grabForm->addRow(tr("α decay:"), m_grabAlphaSpin);

    m_grabMaxShellsSpin = new QSpinBox(this);
    m_grabMaxShellsSpin->setRange(0, 10);
    m_grabMaxShellsSpin->setValue(3);
    m_grabMaxShellsSpin->setToolTip(tr("Max BFS depth for force propagation"));
    grabForm->addRow(tr("Max shells:"), m_grabMaxShellsSpin);

    innerLayout->addWidget(grabGroup);

    innerLayout->addStretch();

    inner->setLayout(innerLayout);
    scroll->setWidget(inner);
    outer->addWidget(scroll);

    // ---- Connections ----
    connect(m_startBtn, &QPushButton::clicked, this, &SimulationControlWidget::onStartClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &SimulationControlWidget::onPauseClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &SimulationControlWidget::onStopClicked);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &SimulationControlWidget::onModeChanged);

    auto notifyConfig = [this]() { emit configChanged(buildConfig()); };
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    connect(m_methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    connect(m_optimizerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    connect(m_tempSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_timestepSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_stepsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notifyConfig);
    connect(m_fpsLimitSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notifyConfig);
    connect(m_gpuCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    connect(m_writeTrjCheck, &QCheckBox::toggled, this, notifyConfig);
    connect(m_perfCheck, &QCheckBox::toggled, this, notifyConfig);
    connect(m_convergenceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);

    auto notifyGrab = [this]() {
        emit grabSettingsChanged(m_grabStrengthSpin->value(),
            m_grabAlphaSpin->value(), m_grabMaxShellsSpin->value());
    };
    connect(m_grabStrengthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyGrab);
    connect(m_grabAlphaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyGrab);
    connect(m_grabMaxShellsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notifyGrab);

    onModeChanged(0);
}

SimulationConfig SimulationControlWidget::buildConfig() const
{
    SimulationConfig cfg;
    cfg.mode = static_cast<SimulationConfig::Mode>(m_modeCombo->currentData().toInt());
    cfg.method = m_methodCombo->currentData().toString();
    cfg.optimizer = m_optimizerCombo->currentData().toString();
    cfg.temperature = m_tempSpin->value();
    cfg.timestep = m_timestepSpin->value();
    cfg.steps = m_stepsSpin->value();
    cfg.fpsLimit = m_fpsLimitSpin->value();
    cfg.gpu = m_gpuCombo->currentData().toString();
    cfg.writeTrajectory = m_writeTrjCheck->isChecked();
    cfg.performanceAnalysis = m_perfCheck->isChecked();
    cfg.convergence = m_convergenceSpin->value();
    return cfg;
}

void SimulationControlWidget::onStartClicked()
{
    if (m_atoms.isEmpty()) {
        m_statusLabel->setText(tr("No molecule loaded."));
        return;
    }

    if (m_thread && m_thread->isRunning()) {
        if (m_worker)
            m_worker->requestStop();
        m_thread->quit();
        m_thread->wait(2000);
    }

    m_worker = new SimulationWorker;
    m_worker->setMolecule(m_atoms);
    m_worker->setBonds(m_bonds);
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

    m_config = buildConfig();

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

    if (m_config.mode == SimulationConfig::Mode::MolecularDynamics) {
        const double kB_Eh = 3.1668114e-6;
        double tempK = (frame->ekin > 0 && !m_atoms.isEmpty())
            ? (2.0 * frame->ekin) / (3.0 * m_atoms.size() * kB_Eh)
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
    m_timestepSpin->setEnabled(isMD);
    m_fpsLimitSpin->setEnabled(isMD);
    m_gpuCombo->setEnabled(isMD);
    m_perfCheck->setEnabled(isMD);
    m_convergenceSpin->setEnabled(!isMD);
    m_optimizerCombo->setEnabled(!isMD);
}

void SimulationControlWidget::setRunning(bool running)
{
    m_startBtn->setEnabled(!running);
    m_pauseBtn->setEnabled(running);
    m_stopBtn->setEnabled(running);
    m_modeCombo->setEnabled(!running);
    m_methodCombo->setEnabled(!running);
    m_optimizerCombo->setEnabled(!running);
    m_tempSpin->setEnabled(!running);
    m_timestepSpin->setEnabled(!running);
    m_stepsSpin->setEnabled(!running);
    m_fpsLimitSpin->setEnabled(!running);
    m_gpuCombo->setEnabled(!running);
    m_writeTrjCheck->setEnabled(!running);
    m_perfCheck->setEnabled(!running);
    m_convergenceSpin->setEnabled(!running);

    emit simulationRunningChanged(running);
}
