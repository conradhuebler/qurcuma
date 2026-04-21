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

    // ---- Simulationssteuerung ----
    auto* simForm = new QFormLayout;
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("Molecular Dynamics"),
        static_cast<int>(SimulationConfig::Mode::MolecularDynamics));
    m_modeCombo->addItem(tr("Geometry Optimization"),
        static_cast<int>(SimulationConfig::Mode::GeometryOptimization));
    simForm->addRow(tr("Mode:"), m_modeCombo);
    innerLayout->addLayout(simForm);

    // ---- Run buttons ----
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

    // ---- Status ----
    m_statusLabel = new QLabel(tr("Ready"), this);
    m_statusLabel->setTextFormat(Qt::RichText);
    m_statusLabel->setStyleSheet("color: gray; font-size: 11px;");
    m_statusLabel->setWordWrap(true);
    innerLayout->addWidget(m_statusLabel);

    // ---- Potential / Methode ----
    auto* potentialGroup = new QGroupBox(tr("Potential / Method"), this);
    auto* potentialForm = new QFormLayout(potentialGroup);

    m_methodCombo = new QComboBox(this);
    m_methodCombo->addItem("GFN-FF", "gfnff");
    m_methodCombo->addItem("UFF", "uff");
    m_methodCombo->addItem("GFN2", "gfn2");
    m_methodCombo->addItem("GFN1", "gfn1");
    potentialForm->addRow(tr("Method:"), m_methodCombo);

    m_gpuCombo = new QComboBox(this);
    m_gpuCombo->addItem(tr("CPU (none)"), "none");
    m_gpuCombo->addItem(tr("CUDA"), "cuda");
    m_gpuCombo->addItem(tr("Auto"), "auto");
    m_gpuCombo->setToolTip(tr("GPU acceleration for force field calculations"));
    potentialForm->addRow(tr("GPU:"), m_gpuCombo);

    // GFN-FF topology mode selector
    m_topologyModeCombo = new QComboBox(this);
    m_topologyModeCombo->addItem(tr("Auto (adaptive)"), "auto");
    m_topologyModeCombo->addItem(tr("Constant (fixed)"), "constant");
    m_topologyModeCombo->setToolTip(tr("GFN-FF topology mode: Auto recalculates topology when needed, "
                                       "Constant keeps initial topology fixed (faster for MD)"));
    potentialForm->addRow(tr("Topology:"), m_topologyModeCombo);

    innerLayout->addWidget(potentialGroup);

    // ---- MD Parameters ----
    m_mdGroup = new QGroupBox(tr("MD Parameters"), this);
    auto* mdForm = new QFormLayout(m_mdGroup);

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
    m_fpsLimitSpin->setToolTip(tr("Steps displayed per second — controls simulation speed"));
    mdForm->addRow(tr("Speed:"), m_fpsLimitSpin);

    m_hmassSpin = new QDoubleSpinBox(this);
    m_hmassSpin->setRange(1.0, 5.0);
    m_hmassSpin->setValue(1.0);
    m_hmassSpin->setDecimals(1);
    m_hmassSpin->setSingleStep(0.5);
    m_hmassSpin->setSuffix(" amu");
    m_hmassSpin->setToolTip(tr("Hydrogen mass scaling: increases H mass to allow larger time steps\n"
                               "1.0 = normal mass, 2.0-3.0 = common values for faster MD"));
    mdForm->addRow(tr("H mass:"), m_hmassSpin);

    innerLayout->addWidget(m_mdGroup);

    // ---- RATTLE constraints (MD only) ----
    m_rattleGroup = new QGroupBox(tr("RATTLE Constraints"), this);
    auto* rattleOuterLayout = new QVBoxLayout(m_rattleGroup);
    rattleOuterLayout->setSpacing(4);
    rattleOuterLayout->setContentsMargins(4, 4, 4, 4);

    auto* rattleModeForm = new QFormLayout;
    m_rattleCombo = new QComboBox(this);
    m_rattleCombo->addItem(tr("Off"), 0);
    m_rattleCombo->addItem(tr("RATTLE"), 1);
    m_rattleCombo->addItem(tr("RATTLE (H-only)"), 2);
    m_rattleCombo->setToolTip(tr("Bond-length constraint algorithm (RATTLE)"));
    rattleModeForm->addRow(tr("Mode:"), m_rattleCombo);
    rattleOuterLayout->addLayout(rattleModeForm);

    // Detail controls — shown only when RATTLE is active
    m_rattleDetails = new QWidget(m_rattleGroup);
    auto* rattleForm = new QFormLayout(m_rattleDetails);
    rattleForm->setContentsMargins(0, 0, 0, 0);

    m_rattle12Check = new QCheckBox(tr("Constrain 1-2 bonds"), this);
    m_rattle12Check->setChecked(true);
    rattleForm->addRow("", m_rattle12Check);

    m_rattle13Check = new QCheckBox(tr("Constrain 1-3 angles"), this);
    m_rattle13Check->setChecked(false);
    rattleForm->addRow("", m_rattle13Check);

    m_rattleTol12Spin = new QDoubleSpinBox(this);
    m_rattleTol12Spin->setRange(1e-10, 1e-1);
    m_rattleTol12Spin->setDecimals(8);
    m_rattleTol12Spin->setSingleStep(1e-5);
    m_rattleTol12Spin->setValue(1e-4);
    m_rattleTol12Spin->setToolTip(tr("Tolerance for 1-2 bond constraints (Bohr²)"));
    rattleForm->addRow(tr("Tol 1-2:"), m_rattleTol12Spin);

    m_rattleTol13Spin = new QDoubleSpinBox(this);
    m_rattleTol13Spin->setRange(1e-10, 1e-1);
    m_rattleTol13Spin->setDecimals(8);
    m_rattleTol13Spin->setSingleStep(1e-4);
    m_rattleTol13Spin->setValue(1e-3);
    m_rattleTol13Spin->setToolTip(tr("Tolerance for 1-3 angle constraints (Bohr²)"));
    rattleForm->addRow(tr("Tol 1-3:"), m_rattleTol13Spin);

    m_rattleMaxIterSpin = new QSpinBox(this);
    m_rattleMaxIterSpin->setRange(1, 1000);
    m_rattleMaxIterSpin->setValue(100);
    m_rattleMaxIterSpin->setToolTip(tr("Maximum RATTLE iterations per MD step"));
    rattleForm->addRow(tr("Max iter:"), m_rattleMaxIterSpin);

    rattleOuterLayout->addWidget(m_rattleDetails);
    m_rattleDetails->setVisible(false);  // hidden until mode != off
    innerLayout->addWidget(m_rattleGroup);

    // ---- Optimization Parameters ----
    m_optGroup = new QGroupBox(tr("Optimization"), this);
    auto* optForm = new QFormLayout(m_optGroup);

    m_optimizerCombo = new QComboBox(this);
    m_optimizerCombo->addItem(tr("Auto"), "auto");
    m_optimizerCombo->addItem(tr("LBFGS++"), "lbfgspp");
    m_optimizerCombo->addItem(tr("Native L-BFGS"), "native_lbfgs");
    m_optimizerCombo->addItem(tr("DIIS"), "native_diis");
    m_optimizerCombo->addItem(tr("RFO"), "native_rfo");
    m_optimizerCombo->addItem(tr("ANCOpt"), "ancopt");
    m_optimizerCombo->setToolTip(tr("Optimization algorithm (geometry optimization only)"));
    optForm->addRow(tr("Algorithm:"), m_optimizerCombo);

    m_convergenceSpin = new QDoubleSpinBox(this);
    m_convergenceSpin->setRange(1e-10, 1e-2);
    m_convergenceSpin->setDecimals(10);
    m_convergenceSpin->setSingleStep(1e-7);
    m_convergenceSpin->setValue(1e-6);
    optForm->addRow(tr("Gradient tol:"), m_convergenceSpin);

    innerLayout->addWidget(m_optGroup);

    // ---- Output Options ----
    auto* outputGroup = new QGroupBox(tr("Output"), this);
    auto* outputLayout = new QVBoxLayout(outputGroup);
    outputLayout->setSpacing(4);
    outputLayout->setContentsMargins(4, 4, 4, 4);

    m_writeTrjCheck = new QCheckBox(tr("Write .trj.xyz"), this);
    outputLayout->addWidget(m_writeTrjCheck);

    m_perfCheck = new QCheckBox(tr("Performance analysis"), this);
    outputLayout->addWidget(m_perfCheck);

    innerLayout->addWidget(outputGroup);

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
    connect(m_topologyModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    connect(m_optimizerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    connect(m_tempSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_timestepSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_stepsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notifyConfig);
    connect(m_fpsLimitSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notifyConfig);
    connect(m_hmassSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_gpuCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    connect(m_writeTrjCheck, &QCheckBox::toggled, this, notifyConfig);
    connect(m_perfCheck, &QCheckBox::toggled, this, notifyConfig);
    connect(m_convergenceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_rattleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    connect(m_rattle12Check, &QCheckBox::toggled, this, notifyConfig);
    connect(m_rattle13Check, &QCheckBox::toggled, this, notifyConfig);
    connect(m_rattleTol12Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_rattleTol13Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_rattleMaxIterSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notifyConfig);

    // Show/hide groups based on mode and RATTLE selection
    connect(m_rattleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int index) { m_rattleDetails->setVisible(index > 0); });

    auto notifyGrab = [this]() {
        emit grabSettingsChanged(m_grabStrengthSpin->value(),
            m_grabAlphaSpin->value(), m_grabMaxShellsSpin->value());
    };
    connect(m_grabStrengthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyGrab);
    connect(m_grabAlphaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyGrab);
    connect(m_grabMaxShellsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notifyGrab);

    // Show/hide topology mode only for GFN-FF
    connect(m_methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int /*index*/) {
            bool isGFNFF = (m_methodCombo->currentData().toString() == "gfnff");
            // Find the topology row and hide/show it
            // The topology combo is the 3rd row in the potential group
            if (m_topologyModeCombo && m_topologyModeCombo->parentWidget()) {
                QWidget* label = nullptr;
                // Find the label associated with topology combo
                auto* form = qobject_cast<QFormLayout*>(m_topologyModeCombo->parentWidget()->layout());
                if (form) {
                    int row = 2; // 3rd row (0-indexed)
                    QLayoutItem* labelItem = form->itemAt(row, QFormLayout::LabelRole);
                    if (labelItem) label = labelItem->widget();
                }
                m_topologyModeCombo->setVisible(isGFNFF);
                if (label) label->setVisible(isGFNFF);
            }
        });

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
    cfg.hmass = m_hmassSpin->value();
    cfg.gpu = m_gpuCombo->currentData().toString();
    cfg.writeTrajectory = m_writeTrjCheck->isChecked();
    cfg.performanceAnalysis = m_perfCheck->isChecked();
    cfg.convergence = m_convergenceSpin->value();
    cfg.rattleMode    = m_rattleCombo->currentData().toInt();
    cfg.rattle12      = m_rattle12Check->isChecked();
    cfg.rattle13      = m_rattle13Check->isChecked();
    cfg.rattleTol12   = m_rattleTol12Spin->value();
    cfg.rattleTol13   = m_rattleTol13Spin->value();
    cfg.rattleMaxIter = m_rattleMaxIterSpin->value();

    // GFN-FF topology mode
    cfg.topologyMode = m_topologyModeCombo->currentData().toString();

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

    // Reset FPS measurement for the new simulation run
    m_frameCount = 0;
    m_actualFps = 0.0;
    m_fpsTimer.invalidate();

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

    // Measure actual FPS (update every second)
    m_frameCount++;
    if (!m_fpsTimer.isValid() || m_fpsTimer.elapsed() >= 1000) {
        qint64 elapsed = m_fpsTimer.isValid() ? m_fpsTimer.elapsed() : 1000;
        m_actualFps = 1000.0 * m_frameCount / elapsed;
        m_frameCount = 0;
        m_fpsTimer.restart();
    }

    // Format FPS string: red when actual < configured limit
    QString fpsText;
    if (m_actualFps < m_config.fpsLimit) {
        fpsText = tr("<span style='color:red'>%1 fps</span>").arg(m_actualFps, 0, 'f', 1);
    } else {
        fpsText = tr("%1 fps").arg(m_actualFps, 0, 'f', 1);
    }

    if (m_config.mode == SimulationConfig::Mode::MolecularDynamics) {
        const double kB_Eh = 3.1668114e-6;
        double tempK = (frame->ekin > 0 && !m_atoms.isEmpty())
            ? (2.0 * frame->ekin) / (3.0 * m_atoms.size() * kB_Eh)
            : 0.0;
        m_statusLabel->setText(
            tr("Step %1 | E= %2 Eh | T≈ %3 K | %4")
                .arg(frame->step)
                .arg(frame->energy, 0, 'f', 6)
                .arg(tempK, 0, 'f', 0)
                .arg(fpsText));
    } else {
        m_statusLabel->setText(
            tr("Iter %1 | E= %2 Eh | %3")
                .arg(frame->step)
                .arg(frame->energy, 0, 'f', 8)
                .arg(fpsText));
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

    // Show/hide mode-specific groups
    m_mdGroup->setVisible(isMD);
    m_rattleGroup->setVisible(isMD);
    m_optGroup->setVisible(!isMD);
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
    m_hmassSpin->setEnabled(!running);
    m_gpuCombo->setEnabled(!running);
    m_writeTrjCheck->setEnabled(!running);
    m_perfCheck->setEnabled(!running);
    m_convergenceSpin->setEnabled(!running);
    m_rattleCombo->setEnabled(!running);
    m_rattle12Check->setEnabled(!running);
    m_rattle13Check->setEnabled(!running);
    m_rattleTol12Spin->setEnabled(!running);
    m_rattleTol13Spin->setEnabled(!running);
    m_rattleMaxIterSpin->setEnabled(!running);

    emit simulationRunningChanged(running);
}
