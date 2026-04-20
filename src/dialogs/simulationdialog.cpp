// simulationdialog.cpp - Full simulation configuration and monitoring dialog
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Interactive Simulation Integration

#include "simulationdialog.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QTextStream>
#include <QVBoxLayout>

SimulationDialog::SimulationDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Molecular Simulation"));
    setMinimumSize(520, 600);
    setupUI();
}

SimulationDialog::~SimulationDialog()
{
    if (m_worker)
        m_worker->requestStop();
    if (m_thread && m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait(2000);
    }
}

void SimulationDialog::setMolecule(const QVector<MoleculeViewer::Atom>& atoms)
{
    m_atoms = atoms;
    m_logView->append(tr("Molecule loaded: %1 atoms").arg(atoms.size()));
}

void SimulationDialog::setMode(SimulationConfig::Mode mode)
{
    int idx = (mode == SimulationConfig::Mode::GeometryOptimization) ? 1 : 0;
    m_modeCombo->setCurrentIndex(idx);
}

// Claude Generated - Pre-populate all dialog controls from shared config for dock/dialog sync
void SimulationDialog::setConfig(const SimulationConfig& cfg)
{
    int modeIdx = m_modeCombo->findData(static_cast<int>(cfg.mode));
    if (modeIdx >= 0)
        m_modeCombo->setCurrentIndex(modeIdx);
    int methIdx = m_methodCombo->findData(cfg.method);
    if (methIdx >= 0)
        m_methodCombo->setCurrentIndex(methIdx);
    m_tempSpin->setValue(cfg.temperature);
    m_timestepSpin->setValue(cfg.timestep);
    m_stepsSpin->setValue(cfg.steps);
    m_dumpFreqSpin->setValue(cfg.dumpFrequency);
    m_fpsLimitSpin->setValue(cfg.fpsLimit);
    { int idx = m_gpuCombo->findData(cfg.gpu); if (idx >= 0) m_gpuCombo->setCurrentIndex(idx); }
    m_writeTrjCheck->setChecked(cfg.writeTrajectory);
    if (m_perfCheck)
        m_perfCheck->setChecked(cfg.performanceAnalysis);
    m_convergenceSpin->setValue(cfg.convergence);
    if (cfg.mode == SimulationConfig::Mode::GeometryOptimization)
        m_maxIterSpin->setValue(cfg.steps);
}

void SimulationDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // --- Type & Method ---
    auto* typeForm = new QFormLayout;

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("Molecular Dynamics (MD)"),
        static_cast<int>(SimulationConfig::Mode::MolecularDynamics));
    m_modeCombo->addItem(tr("Geometry Optimization"),
        static_cast<int>(SimulationConfig::Mode::GeometryOptimization));
    typeForm->addRow(tr("Simulation type:"), m_modeCombo);

    m_methodCombo = new QComboBox(this);
    m_methodCombo->addItem(tr("GFN-FF (default, no external deps)"), "gfnff");
    m_methodCombo->addItem(tr("UFF (Universal Force Field)"), "uff");
    m_methodCombo->addItem(tr("GFN2 (requires TBLite/XTB)"), "gfn2");
    m_methodCombo->addItem(tr("GFN1 (requires TBLite/XTB)"), "gfn1");
    typeForm->addRow(tr("Method:"), m_methodCombo);

    mainLayout->addLayout(typeForm);

    // --- MD parameters ---
    m_mdGroup = new QGroupBox(tr("Molecular Dynamics Parameters"), this);
    auto* mdForm = new QFormLayout(m_mdGroup);

    m_tempSpin = new QDoubleSpinBox(this);
    m_tempSpin->setRange(1, 5000);
    m_tempSpin->setValue(300);
    m_tempSpin->setSuffix(" K");
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

    m_dumpFreqSpin = new QSpinBox(this);
    m_dumpFreqSpin->setRange(1, 10000);
    m_dumpFreqSpin->setValue(1);
    m_dumpFreqSpin->setToolTip(tr("Invoke viewer callback every N steps (1 = every step; flood guard limits GUI rate)"));
    mdForm->addRow(tr("Dump frequency:"), m_dumpFreqSpin);

    m_fpsLimitSpin = new QSpinBox(this);
    m_fpsLimitSpin->setRange(1, 120);
    m_fpsLimitSpin->setValue(30);
    m_fpsLimitSpin->setSuffix(tr(" fps"));
    m_fpsLimitSpin->setToolTip(tr("Target display rate: fast steps wait to reach this fps; slow steps show immediately (accepts lower fps)"));
    mdForm->addRow(tr("Max display rate:"), m_fpsLimitSpin);

    m_gpuCombo = new QComboBox(this);
    m_gpuCombo->addItem(tr("CPU (none)"), "none");
    m_gpuCombo->addItem(tr("CUDA"), "cuda");
    m_gpuCombo->addItem(tr("Auto (GPU if available)"), "auto");
    m_gpuCombo->setToolTip(tr("GPU acceleration for GFN-FF (requires CUDA build: cmake -DUSE_CUDA=ON)"));
    mdForm->addRow(tr("GPU acceleration:"), m_gpuCombo);

    m_writeTrjCheck = new QCheckBox(tr("Write trajectory file (.trj.xyz)"), this);
    mdForm->addRow("", m_writeTrjCheck);

    m_perfCheck = new QCheckBox(tr("Performance analysis (every 100 steps)"), this);
    mdForm->addRow("", m_perfCheck);

    mainLayout->addWidget(m_mdGroup);

    // --- Optimization parameters ---
    m_optGroup = new QGroupBox(tr("Geometry Optimization Parameters"), this);
    auto* optForm = new QFormLayout(m_optGroup);
    m_optGroup->setVisible(false);

    m_maxIterSpin = new QSpinBox(this);
    m_maxIterSpin->setRange(1, 100000);
    m_maxIterSpin->setValue(5000);
    optForm->addRow(tr("Max iterations:"), m_maxIterSpin);

    m_convergenceSpin = new QDoubleSpinBox(this);
    m_convergenceSpin->setRange(1e-10, 1e-2);
    m_convergenceSpin->setValue(1e-6);
    m_convergenceSpin->setDecimals(10);
    m_convergenceSpin->setSingleStep(1e-7);
    optForm->addRow(tr("Convergence:"), m_convergenceSpin);

    mainLayout->addWidget(m_optGroup);

    // --- Live status ---
    auto* statusGroup = new QGroupBox(tr("Live Status"), this);
    auto* statusLayout = new QFormLayout(statusGroup);

    m_stepLabel = new QLabel("---", this);
    statusLayout->addRow(tr("Step:"), m_stepLabel);

    m_energyLabel = new QLabel("---", this);
    statusLayout->addRow(tr("Energy [Eh]:"), m_energyLabel);

    m_tempLabel = new QLabel("---", this);
    statusLayout->addRow(tr("Temperature [K]:"), m_tempLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0); // Indeterminate by default
    m_progressBar->setVisible(false);
    statusLayout->addRow(tr("Progress:"), m_progressBar);

    mainLayout->addWidget(statusGroup);

    // --- Log ---
    m_logView = new QTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setMaximumHeight(120);
    m_logView->setFontFamily("monospace");
    mainLayout->addWidget(m_logView);

    // --- Buttons ---
    auto* btnLayout = new QHBoxLayout;

    m_startBtn = new QPushButton(tr("▶ Start"), this);
    m_pauseBtn = new QPushButton(tr("⏸ Pause"), this);
    m_stopBtn = new QPushButton(tr("■ Stop"), this);
    m_exportBtn = new QPushButton(tr("Export Trajectory..."), this);
    m_closeBtn = new QPushButton(tr("Close"), this);

    m_pauseBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);
    m_exportBtn->setEnabled(false);

    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_pauseBtn);
    btnLayout->addWidget(m_stopBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_exportBtn);
    btnLayout->addWidget(m_closeBtn);

    mainLayout->addLayout(btnLayout);

    // --- Connections ---
    connect(m_startBtn, &QPushButton::clicked, this, &SimulationDialog::onStartClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &SimulationDialog::onPauseClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &SimulationDialog::onStopClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &SimulationDialog::onExportClicked);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::close);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &SimulationDialog::onModeChanged);
}

SimulationConfig SimulationDialog::buildConfig() const
{
    SimulationConfig cfg;
    cfg.mode = static_cast<SimulationConfig::Mode>(m_modeCombo->currentData().toInt());
    cfg.method = m_methodCombo->currentData().toString();
    cfg.temperature = m_tempSpin->value();
    cfg.timestep = m_timestepSpin->value();
    cfg.steps = m_stepsSpin->value();
    cfg.dumpFrequency = m_dumpFreqSpin->value();
    cfg.fpsLimit = m_fpsLimitSpin->value();
    cfg.gpu = m_gpuCombo->currentData().toString();
    cfg.writeTrajectory = m_writeTrjCheck->isChecked();
    cfg.performanceAnalysis = m_perfCheck ? m_perfCheck->isChecked() : false;
    cfg.convergence = m_convergenceSpin->value();
    return cfg;
}

void SimulationDialog::onStartClicked()
{
    if (m_atoms.isEmpty()) {
        QMessageBox::warning(this, tr("No Molecule"),
            tr("Please open a molecule file before starting a simulation."));
        return;
    }

    if (m_thread && m_thread->isRunning()) {
        if (m_worker)
            m_worker->requestStop();
        m_thread->quit();
        m_thread->wait(2000);
    }

    m_trajectory.clear();
    m_frameCount = 0;
    m_activeConfig = buildConfig();

    m_worker = new SimulationWorker;
    m_worker->setMolecule(m_atoms);
    m_worker->setConfig(m_activeConfig);

    m_thread = new QThread(this);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &SimulationWorker::run);
    connect(m_worker, &SimulationWorker::frameReady, this, &SimulationDialog::onFrameReady);
    connect(m_worker, &SimulationWorker::finished, this, &SimulationDialog::onSimulationFinished);
    connect(m_worker, &SimulationWorker::errorOccurred, this, &SimulationDialog::onError);
    connect(m_worker, &SimulationWorker::finished, m_thread, &QThread::quit);
    connect(m_worker, &SimulationWorker::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

    // Claude Generated - Expose worker so MainWindow can wire worker->view direct.
    emit workerStarted(m_worker);

    setRunning(true);
    m_progressBar->setVisible(true);
    m_logView->append(tr("Starting %1 with %2...")
                          .arg(m_modeCombo->currentText())
                          .arg(m_methodCombo->currentText()));
    m_thread->start();
}

void SimulationDialog::onPauseClicked()
{
    if (!m_worker)
        return;
    if (m_paused) {
        m_worker->requestResume();
        m_pauseBtn->setText(tr("⏸ Pause"));
        m_logView->append(tr("Resumed."));
        m_paused = false;
    } else {
        m_worker->requestPause();
        m_pauseBtn->setText(tr("▶ Resume"));
        m_logView->append(tr("Paused at step %1.").arg(m_stepLabel->text()));
        m_paused = true;
    }
}

void SimulationDialog::onStopClicked()
{
    if (m_worker) {
        m_worker->requestStop();
        m_logView->append(tr("Stop requested..."));
    }
}

void SimulationDialog::onExportClicked()
{
    if (m_trajectory.isEmpty())
        return;

    QString path = QFileDialog::getSaveFileName(this, tr("Export Trajectory"),
        QString(), tr("XYZ Trajectory (*.trj.xyz);;XYZ Files (*.xyz)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Error"),
            tr("Could not open file for writing: %1").arg(path));
        return;
    }

    // Claude Generated - Frames carry only positions; element names come from the
    // initial molecule (m_atoms). Topology-change during sim isn't supported here.
    QTextStream out(&file);
    for (int f = 0; f < m_trajectory.size(); ++f) {
        const SimulationFramePtr& frame = m_trajectory[f];
        if (!frame) continue;
        const int n = static_cast<int>(frame->positions.size());
        out << n << "\n";
        out << "Frame " << f << "\n";
        for (int i = 0; i < n; ++i) {
            const QString element = (i < m_atoms.size()) ? m_atoms[i].element : QStringLiteral("C");
            const QVector3D& p = frame->positions[i];
            out << element << "  " << p.x() << "  " << p.y() << "  " << p.z() << "\n";
        }
    }
    m_logView->append(tr("Exported %1 frames to %2.").arg(m_trajectory.size()).arg(path));
}

void SimulationDialog::onModeChanged(int /*index*/)
{
    bool isMD = (m_modeCombo->currentData().toInt()
        == static_cast<int>(SimulationConfig::Mode::MolecularDynamics));
    m_mdGroup->setVisible(isMD);
    m_optGroup->setVisible(!isMD);
    adjustSize();
}

void SimulationDialog::onFrameReady(SimulationFramePtr frame)
{
    if (!frame)
        return;

    // Store frame for export only when explicitly requested — avoids O(N×steps) memory growth
    if (m_activeConfig.writeTrajectory) {
        m_trajectory.append(frame);
    }
    ++m_frameCount;

    m_stepLabel->setText(QString::number(frame->step));
    m_energyLabel->setText(QString::number(frame->energy, 'f', 8));

    if (m_activeConfig.mode == SimulationConfig::Mode::MolecularDynamics && frame->ekin > 0) {
        // Rough temperature estimate: Ekin = 3/2 * N * kB * T  →  T = 2*Ekin/(3*N*kB)
        const double kB_Eh = 3.1668114e-6;
        double T = (2.0 * frame->ekin) / (3.0 * m_atoms.size() * kB_Eh);
        m_tempLabel->setText(QString::number(T, 'f', 1) + " K");
    }

    // Claude Generated - No forwarding: MainWindow wires worker directly to the viewer.
}

void SimulationDialog::onSimulationFinished()
{
    setRunning(false);
    m_worker = nullptr;
    m_thread = nullptr;
    m_paused = false;
    m_progressBar->setVisible(false);
    m_exportBtn->setEnabled(!m_trajectory.isEmpty());
    m_logView->append(tr("Simulation finished. %1 frames recorded.").arg(m_frameCount));
}

void SimulationDialog::onError(const QString& message)
{
    m_logView->append(tr("ERROR: %1").arg(message));
    QMessageBox::critical(this, tr("Simulation Error"), message);
    onSimulationFinished();
}

void SimulationDialog::setRunning(bool running)
{
    m_startBtn->setEnabled(!running);
    m_pauseBtn->setEnabled(running);
    m_stopBtn->setEnabled(running);
    m_modeCombo->setEnabled(!running);
    m_methodCombo->setEnabled(!running);
    m_mdGroup->setEnabled(!running);
    m_optGroup->setEnabled(!running);
}

void SimulationDialog::closeEvent(QCloseEvent* event)
{
    if (m_thread && m_thread->isRunning()) {
        if (m_worker)
            m_worker->requestStop();
        m_thread->quit();
        m_thread->wait(2000);
    }
    event->accept();
}
