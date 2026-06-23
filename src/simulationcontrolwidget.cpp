// simulationcontrolwidget.cpp - Full inline simulation controls
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Interactive Simulation Integration (Phase 6)

#include "simulationcontrolwidget.h"

#include "widgets/temperatureslider.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QTableWidgetItem>
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

    // ---- Compact icon button bar + state pill (Claude Generated 2026) ----
    // Replaces the old text+icon row: 5 QToolButtons (Start/Pause/Step/Stop/Save)
    // take ~32px instead of ~36+text. State pill next to the buttons replaces
    // the separate "● Modified" / status rows.
    auto* ctrlRow = new QHBoxLayout;
    ctrlRow->setSpacing(2);

    auto makeToolButton = [this](const QString& text, const QString& tooltip) {
        auto* b = new QToolButton(this);
        b->setText(text);
        b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        b->setAutoRaise(false);
        b->setFixedWidth(34);
        b->setFixedHeight(28);
        b->setToolTip(tooltip);
        return b;
    };
    m_startBtn = makeToolButton(QStringLiteral("▶"), tr("Start auto-run (Space)"));
    m_pauseBtn = makeToolButton(QStringLiteral("⏸"), tr("Pause / resume auto-run"));
    m_stepBtn = makeToolButton(QStringLiteral("⏭"), tr("Step once (one MD step / one Opt iteration)"));
    m_stopBtn = makeToolButton(QStringLiteral("■"), tr("Stop"));
    m_saveBtn = makeToolButton(QStringLiteral("💾"), tr("Save the modified structure"));
    m_saveBtn->setEnabled(false);  // enabled only when structure is modified
    m_resetBtn = makeToolButton(QStringLiteral("↩"), tr("Reset to the original structure"));
    m_resetBtn->setEnabled(false); // enabled when at least one snapshot is available
    m_pauseBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);
    m_stepBtn->setEnabled(false);  // enabled only when no run is in progress

    ctrlRow->addWidget(m_startBtn);
    ctrlRow->addWidget(m_pauseBtn);
    ctrlRow->addWidget(m_stepBtn);
    ctrlRow->addWidget(m_stopBtn);
    ctrlRow->addWidget(m_saveBtn);
    ctrlRow->addWidget(m_resetBtn);

    ctrlRow->addSpacing(8);

    // State pill: shows ● Running / ⏸ Paused / ● Finished / ○ Ready
    m_stateLabel = new QLabel(this);
    m_stateLabel->setTextFormat(Qt::RichText);
    m_stateLabel->setStyleSheet("color: gray; font-weight: 600;");
    ctrlRow->addWidget(m_stateLabel);

    ctrlRow->addStretch();

    innerLayout->addLayout(ctrlRow);

    // ---- Modified / Finished / State pill on second row ----
    auto* statusRow = new QHBoxLayout;
    statusRow->setSpacing(4);

    m_modifiedLabel = new QLabel(this);
    m_modifiedLabel->setTextFormat(Qt::RichText);
    m_modifiedLabel->setText(
        QStringLiteral("<span style='color:#d35400; font-weight:600;'>● Modified</span>"));
    m_modifiedLabel->setVisible(false);
    m_modifiedLabel->setToolTip(tr("On-screen structure differs from the source file"));
    statusRow->addWidget(m_modifiedLabel);

    statusRow->addSpacing(8);

    statusRow->addStretch();

    innerLayout->addLayout(statusRow);

    // ---- Speed (FPS) — visible in both modes (Claude Generated 2026) ----
    // For MD: throttles the auto-run emit cadence. For Opt: throttles the per-
    // iteration emit cadence AND the Step button's re-arm rate (clickable only
    // every 1/fpsLimit seconds so "max XXX FPS" is honoured on click-driven
    // single-step optimisation).
    auto* speedForm = new QFormLayout;
    m_fpsLimitSpin = new QSpinBox(this);
    m_fpsLimitSpin->setRange(1, 240);
    m_fpsLimitSpin->setValue(30);
    m_fpsLimitSpin->setSuffix(tr(" fps"));
    m_fpsLimitSpin->setToolTip(tr("Maximum steps/iterations emitted per second "
                                  "(throttles the auto-run cadence and the Step button rate)"));
    speedForm->addRow(tr("Speed:"), m_fpsLimitSpin);
    innerLayout->addLayout(speedForm);

    // Claude Generated 2026 - Auto-snapshot stride: 0 = off, N > 0 = every N steps.
    // Lives in the Simulation tab (not the Snapshots tab) because it controls
    // how the simulation produces snapshots, not how the user manages them.
    auto* strideRow = new QHBoxLayout;
    strideRow->addWidget(new QLabel(tr("Snapshot every"), this));
    m_strideSpin = new QSpinBox(this);
    m_strideSpin->setRange(0, 10000);
    m_strideSpin->setValue(0);
    m_strideSpin->setSuffix(tr(" steps"));
    m_strideSpin->setToolTip(tr("Automatically take a snapshot every N simulation steps/iterations. 0 = off."));
    strideRow->addWidget(m_strideSpin);
    strideRow->addStretch();
    innerLayout->addLayout(strideRow);

    // ---- Per-frame status (separate from state pill) ----
    m_statusLabel = new QLabel(tr("Ready"), this);
    m_statusLabel->setTextFormat(Qt::RichText);
    m_statusLabel->setStyleSheet("color: gray; font-size: 11px;");
    m_statusLabel->setWordWrap(true);
    innerLayout->addWidget(m_statusLabel);

    // Initial state pill
    setState(tr("○ Ready"), "gray");

    // Claude Generated 2026 - wire the in-dock save button to the public signal.
    connect(m_saveBtn, &QToolButton::clicked,
            this, &SimulationControlWidget::saveStructureRequested);
    // Claude Generated 2026 - wire the in-dock reset button to the public signal.
    // Reset always restores the first snapshot (index 0), which is captured
    // automatically when a molecule is loaded.
    connect(m_resetBtn, &QToolButton::clicked,
            this, [this]() { emit resetStructureRequested(0); });

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
#if defined(USE_CUDA)
    m_gpuCombo->addItem(tr("CUDA"), "cuda");
#endif
#if defined(USE_ROCM)
    m_gpuCombo->addItem(tr("ROCm"), "rocm");
#endif
#if defined(USE_VULKAN)
    m_gpuCombo->addItem(tr("Vulkan"), "vulkan");
#endif
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
    auto* mdOuter = new QHBoxLayout(m_mdGroup);
    mdOuter->setSpacing(8);

    // Vertical temperature-colored slider, live-adjustable during a run. Claude Generated 2026.
    m_tempSlider = new TemperatureSlider(this);
    m_tempSlider->setRange(1.0, 1000.0);
    m_tempSlider->setValue(300.0);
    m_tempSlider->setToolTip(tr("Thermostat target temperature. Editable min/max; the handle stays\n"
                                "live during a run — drag it to change the temperature on the fly\n"
                                "(a drag cancels an active global ramp)."));
    auto* tempCol = new QVBoxLayout;
    tempCol->setSpacing(2);
    auto* tempCaption = new QLabel(tr("Temperature"), this);
    tempCaption->setAlignment(Qt::AlignHCenter);
    tempCol->addWidget(tempCaption);
    tempCol->addWidget(m_tempSlider, 1);
    mdOuter->addLayout(tempCol);

    auto* mdForm = new QFormLayout;
    mdForm->setContentsMargins(0, 0, 0, 0);
    mdOuter->addLayout(mdForm, 1);

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

    // Claude Generated 2026 - Speed (fpsLimit) is now a top-level control visible
    // in both MD and Opt modes; the in-MD-group copy has been removed.

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

    // ---- Temperature Ramp (global setpoint schedule, curcuma temp_ramp/temp_schedule) ----
    // Claude Generated 2026 - drive the global setpoint through a multi-stage schedule.
    m_tempRampGroup = new QGroupBox(tr("Temperature Ramp"), this);
    auto* rampOuter = new QVBoxLayout(m_tempRampGroup);
    rampOuter->setSpacing(4);
    rampOuter->setContentsMargins(4, 4, 4, 4);

    m_tempRampEnableCheck = new QCheckBox(tr("Enable temperature ramp"), this);
    m_tempRampEnableCheck->setToolTip(tr("Drive the global thermostat setpoint through a multi-stage "
        "schedule. Each segment ramps to a target either over N steps or until the measured temperature "
        "reaches it. Dragging the temperature slider during a run overrides the ramp."));
    rampOuter->addWidget(m_tempRampEnableCheck);

    m_tempRampDetails = new QWidget(m_tempRampGroup);
    auto* rampLay = new QVBoxLayout(m_tempRampDetails);
    rampLay->setContentsMargins(0, 0, 0, 0);

    m_tempRampTable = new QTableWidget(0, 3, m_tempRampDetails);
    m_tempRampTable->setHorizontalHeaderLabels({ tr("Target (K)"), tr("Mode"), tr("Value") });
    m_tempRampTable->horizontalHeader()->setStretchLastSection(true);
    m_tempRampTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tempRampTable->verticalHeader()->setVisible(false);
    m_tempRampTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tempRampTable->setMaximumHeight(150);
    m_tempRampTable->setToolTip(tr("Mode 'steps': ramp the setpoint to Target over <Value> integration steps.\n"
                                   "Mode 'reach': hold the setpoint at Target, advance once |<T>-Target| < Value K."));
    rampLay->addWidget(m_tempRampTable);

    auto* rampBtnRow = new QHBoxLayout;
    auto* rampAddBtn = new QPushButton(tr("+ Segment"), m_tempRampDetails);
    auto* rampDelBtn = new QPushButton(tr("− Segment"), m_tempRampDetails);
    rampBtnRow->addWidget(rampAddBtn);
    rampBtnRow->addWidget(rampDelBtn);
    rampBtnRow->addStretch(1);
    rampLay->addLayout(rampBtnRow);

    m_tempOverrideLabel = new QLabel(tr("⚠ ramp overridden by manual temperature"), m_tempRampDetails);
    m_tempOverrideLabel->setStyleSheet(QStringLiteral("color:#c47f00;"));
    m_tempOverrideLabel->setVisible(false);
    rampLay->addWidget(m_tempOverrideLabel);

    rampOuter->addWidget(m_tempRampDetails);
    m_tempRampDetails->setVisible(false);  // hidden until enabled
    innerLayout->addWidget(m_tempRampGroup);

    connect(rampAddBtn, &QPushButton::clicked, this, [this]() {
        addRampSegmentRow(500.0, QStringLiteral("steps"), 5000.0);
        emit configChanged(buildConfig());
    });
    connect(rampDelBtn, &QPushButton::clicked, this, [this]() {
        const int row = m_tempRampTable->currentRow() >= 0
            ? m_tempRampTable->currentRow() : m_tempRampTable->rowCount() - 1;
        if (row >= 0)
            m_tempRampTable->removeRow(row);
        emit configChanged(buildConfig());
    });
    connect(m_tempRampEnableCheck, &QCheckBox::toggled, this,
        [this](bool on) {
            m_tempRampDetails->setVisible(on);
            if (on && m_tempRampTable->rowCount() == 0)
                addRampSegmentRow(500.0, QStringLiteral("steps"), 5000.0);
        });
    connect(m_tempRampTable, &QTableWidget::cellChanged, this,
        [this](int, int) { emit configChanged(buildConfig()); });

    // ---- Temperature Regions (per-atom-subset thermostats, curcuma temp_regions) ----
    // Claude Generated 2026 - each region thermostats an atom subset to its own target/ramp;
    // atoms in no region follow the global temperature above.
    m_tempRegionGroup = new QGroupBox(tr("Temperature Regions"), this);
    auto* regOuter = new QVBoxLayout(m_tempRegionGroup);
    regOuter->setSpacing(4);
    regOuter->setContentsMargins(4, 4, 4, 4);

    auto* regHelp = new QLabel(tr("Atom subsets with their own temperature. Atoms in no region "
        "follow the global temperature."), m_tempRegionGroup);
    regHelp->setWordWrap(true);
    regOuter->addWidget(regHelp);

    m_tempRegionTable = new QTableWidget(0, 3, m_tempRegionGroup);
    m_tempRegionTable->setHorizontalHeaderLabels({ tr("Atoms"), tr("Start T (K)"), tr("Schedule") });
    m_tempRegionTable->horizontalHeader()->setStretchLastSection(true);
    m_tempRegionTable->verticalHeader()->setVisible(false);
    m_tempRegionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tempRegionTable->setMaximumHeight(150);
    m_tempRegionTable->setToolTip(tr("Atoms: selection like \"1:10,15\", \"F2\" (fragment), or \"-1\" (all).\n"
                                     "Schedule (optional): same grammar as the global ramp, e.g. \"800:steps:5000;300:reach:10\"."));
    regOuter->addWidget(m_tempRegionTable);

    auto* regBtnRow = new QHBoxLayout;
    auto* regAddBtn = new QPushButton(tr("+ Region"), m_tempRegionGroup);
    auto* regDelBtn = new QPushButton(tr("− Region"), m_tempRegionGroup);
    regBtnRow->addWidget(regAddBtn);
    regBtnRow->addWidget(regDelBtn);
    regBtnRow->addStretch(1);
    regOuter->addLayout(regBtnRow);

    innerLayout->addWidget(m_tempRegionGroup);

    connect(regAddBtn, &QPushButton::clicked, this, [this]() {
        addRegionRow(QStringLiteral("-1"), 300.0, QString());
        emit configChanged(buildConfig());
    });
    connect(regDelBtn, &QPushButton::clicked, this, [this]() {
        const int row = m_tempRegionTable->currentRow() >= 0
            ? m_tempRegionTable->currentRow() : m_tempRegionTable->rowCount() - 1;
        if (row >= 0)
            m_tempRegionTable->removeRow(row);
        emit configChanged(buildConfig());
    });
    connect(m_tempRegionTable, &QTableWidget::cellChanged, this,
        [this](int, int) { emit configChanged(buildConfig()); });

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

    // ---- RMSD Metadynamics (MD bias, curcuma SimpleMD rmsd_mtd) ----
    // Claude Generated 2026 - exposes curcuma's RMSD-MTD bias option with all
    // relevant parameters (see external/curcuma/src/capabilities/simplemd.h,
    // "RMSD-MTD" PARAM category). Shown only in MD mode; details reveal on enable.
    m_rmsdMtdGroup = new QGroupBox(tr("RMSD Metadynamics"), this);
    auto* rmsdOuterLayout = new QVBoxLayout(m_rmsdMtdGroup);
    rmsdOuterLayout->setSpacing(4);
    rmsdOuterLayout->setContentsMargins(4, 4, 4, 4);

    m_rmsdMtdEnableCheck = new QCheckBox(tr("Enable RMSD-MTD bias"), this);
    m_rmsdMtdEnableCheck->setToolTip(tr("Add a bias potential in RMSD-to-reference space "
        "during the MD run, driving exploration away from already-sampled geometries "
        "(curcuma SimpleMD rmsd_mtd)."));
    rmsdOuterLayout->addWidget(m_rmsdMtdEnableCheck);

    m_rmsdMtdDetails = new QWidget(m_rmsdMtdGroup);
    auto* rmsdForm = new QFormLayout(m_rmsdMtdDetails);
    rmsdForm->setContentsMargins(0, 0, 0, 0);

    m_rmsdMtdKSpin = new QDoubleSpinBox(this);
    m_rmsdMtdKSpin->setRange(0.0, 1000.0);
    m_rmsdMtdKSpin->setDecimals(4);
    m_rmsdMtdKSpin->setSingleStep(0.01);
    m_rmsdMtdKSpin->setValue(0.01);
    m_rmsdMtdKSpin->setSuffix(" Eh");
    m_rmsdMtdKSpin->setToolTip(tr("Hill height constant: W_i = k * counter_i (Eh). "
        "Force is the exact gradient of the bias, so k is ~100x smaller than the "
        "pre-2026 value."));
    rmsdForm->addRow(tr("k (height):"), m_rmsdMtdKSpin);

    m_rmsdMtdAlphaSpin = new QDoubleSpinBox(this);
    m_rmsdMtdAlphaSpin->setRange(0.0, 1e6);
    m_rmsdMtdAlphaSpin->setDecimals(2);
    m_rmsdMtdAlphaSpin->setSingleStep(1.0);
    m_rmsdMtdAlphaSpin->setValue(10.0);
    m_rmsdMtdAlphaSpin->setToolTip(tr("Width parameter for the RMSD Gaussians."));
    rmsdForm->addRow(tr("α (width):"), m_rmsdMtdAlphaSpin);

    m_rmsdMtdAtomsEdit = new QLineEdit(QStringLiteral("-1"), this);
    m_rmsdMtdAtomsEdit->setToolTip(tr("Atom indices used for the RMSD calculation, "
        "e.g. \"1-50\". \"-1\" = all atoms."));
    rmsdForm->addRow(tr("RMSD atoms:"), m_rmsdMtdAtomsEdit);

    // Reference structures file: line edit + browse button in a row.
    auto* refRow = new QWidget(this);
    auto* refRowLayout = new QHBoxLayout(refRow);
    refRowLayout->setContentsMargins(0, 0, 0, 0);
    m_rmsdMtdRefFileEdit = new QLineEdit(QStringLiteral("none"), refRow);
    m_rmsdMtdRefFileEdit->setToolTip(tr("File with reference structures for RMSD-MTD. "
        "\"none\" = derive bias structures from the running geometry."));
    auto* refBrowseBtn = new QPushButton(tr("…"), refRow);
    refBrowseBtn->setMaximumWidth(30);
    refBrowseBtn->setToolTip(tr("Browse for a reference structures file"));
    refRowLayout->addWidget(m_rmsdMtdRefFileEdit, 1);
    refRowLayout->addWidget(refBrowseBtn);
    rmsdForm->addRow(tr("Ref. file:"), refRow);
    connect(refBrowseBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this,
            tr("RMSD-MTD reference structures"),
            QString(), tr("Molecular structures (*.xyz *.pdb *.mol2 *.vtf);;All files (*)"));
        if (!path.isEmpty())
            m_rmsdMtdRefFileEdit->setText(path);
    });

    m_rmsdMtdMaxGaussiansSpin = new QSpinBox(this);
    m_rmsdMtdMaxGaussiansSpin->setRange(-1, 1000000);
    m_rmsdMtdMaxGaussiansSpin->setValue(-1);
    m_rmsdMtdMaxGaussiansSpin->setToolTip(tr("Maximum number of stored bias structures. "
        "-1 = unlimited."));
    rmsdForm->addRow(tr("Max Gaussians:"), m_rmsdMtdMaxGaussiansSpin);

    m_rmsdMtdMaxHeightSpin = new QSpinBox(this);
    m_rmsdMtdMaxHeightSpin->setRange(0, 1000000);
    m_rmsdMtdMaxHeightSpin->setValue(0);
    m_rmsdMtdMaxHeightSpin->setToolTip(tr("Cap on per-structure hill counter: "
        "W_i = k * min(counter_i, cap). 0 = unbounded (legacy)."));
    rmsdForm->addRow(tr("Max height cap:"), m_rmsdMtdMaxHeightSpin);

    m_rmsdMtdEconvSpin = new QDoubleSpinBox(this);
    m_rmsdMtdEconvSpin->setRange(0.0, 1e12);
    m_rmsdMtdEconvSpin->setDecimals(0);
    m_rmsdMtdEconvSpin->setSingleStep(1e7);
    m_rmsdMtdEconvSpin->setValue(1e8);
    m_rmsdMtdEconvSpin->setToolTip(tr("Bias-deposition convergence threshold (rmsd_econv). "
        "Gates when a region is considered biased enough to stop depositing hills; "
        "passed to curcuma via setEnergyConv()."));
    rmsdForm->addRow(tr("Conv. threshold:"), m_rmsdMtdEconvSpin);

    m_rmsdMtdPaceSpin = new QSpinBox(this);
    m_rmsdMtdPaceSpin->setRange(1, 1000000);
    m_rmsdMtdPaceSpin->setValue(1);
    m_rmsdMtdPaceSpin->setToolTip(tr("Deposition pace. UNUSED in the counter-based "
        "scheme (kept for compatibility) — deposition is gated by bias level."));
    rmsdForm->addRow(tr("Pace (unused):"), m_rmsdMtdPaceSpin);

    m_rmsdMtdWtmtdCheck = new QCheckBox(tr("Well-tempered reporting"), this);
    m_rmsdMtdWtmtdCheck->setToolTip(tr("Switch on well-tempered reporting. Only then "
        "does ΔT below take effect (it only affects the reported well-tempered "
        "energy, not the force or exploration)."));
    rmsdForm->addRow(QString(), m_rmsdMtdWtmtdCheck);

    m_rmsdMtdDtSpin = new QDoubleSpinBox(this);
    m_rmsdMtdDtSpin->setRange(0.0, 1e9);
    m_rmsdMtdDtSpin->setDecimals(1);
    m_rmsdMtdDtSpin->setSingleStep(100.0);
    m_rmsdMtdDtSpin->setValue(2000.0);
    m_rmsdMtdDtSpin->setSuffix(" K");
    m_rmsdMtdDtSpin->setEnabled(false);
    m_rmsdMtdDtSpin->setToolTip(tr("Well-tempered bias temperature ΔT (K). "
        "Only used when well-tempered reporting is on."));
    rmsdForm->addRow(tr("ΔT (wtmtd):"), m_rmsdMtdDtSpin);

    m_rmsdMtdFreezeCheck = new QCheckBox(tr("Freeze inherited hills"), this);
    m_rmsdMtdFreezeCheck->setToolTip(tr("Freeze hill heights of bias structures present "
        "at MD run start; only structures deposited during this run gain height. "
        "Bounds cumulative bias force across successive shared-pool runs."));
    rmsdForm->addRow(QString(), m_rmsdMtdFreezeCheck);

    rmsdOuterLayout->addWidget(m_rmsdMtdDetails);
    m_rmsdMtdDetails->setVisible(false);  // hidden until enabled
    innerLayout->addWidget(m_rmsdMtdGroup);

    // ---- Confinement Walls (curcuma SimpleMD wall_* params) ----
    // Claude Generated 2026 - exposes curcuma's harmonic confinement walls
    // (wall_type/wall_potential/wall_x|y|z_min|max/wall_radius, "Walls" PARAM
    // category in external/curcuma/src/capabilities/simplemd.h). MD only; details
    // reveal on enable. The box wireframe is drawn live in the 3D viewer as the
    // bounds are typed (auto-show when enabled) — see MainWindow forwarding
    // configChanged to MoleculeViewer::setConfinementBox.
    m_wallGroup = new QGroupBox(tr("Confinement Walls"), this);
    auto* wallOuterLayout = new QVBoxLayout(m_wallGroup);
    wallOuterLayout->setSpacing(4);
    wallOuterLayout->setContentsMargins(4, 4, 4, 4);

    m_wallEnableCheck = new QCheckBox(tr("Enable confinement walls"), this);
    m_wallEnableCheck->setToolTip(tr("Add a harmonic/logfermi confinement potential "
        "that pushes atoms back inside the defined region during MD. The wall "
        "geometry is drawn live in the 3D viewer; explicit bounds are required for "
        "the preview (zeros = curcuma auto-size, not previewable)."));
    wallOuterLayout->addWidget(m_wallEnableCheck);

    m_wallDetails = new QWidget(m_wallGroup);
    auto* wallForm = new QFormLayout(m_wallDetails);

    m_wallTypeCombo = new QComboBox(this);
    m_wallTypeCombo->addItem(tr("None"), 0);
    m_wallTypeCombo->addItem(tr("Spherical"), 1);
    m_wallTypeCombo->addItem(tr("Rectangular"), 2);
    m_wallTypeCombo->setToolTip(tr("Wall geometry. Spherical is origin-centred; "
        "rectangular is the axis-aligned cuboid [x/y/z min .. max]."));
    wallForm->addRow(tr("Geometry:"), m_wallTypeCombo);

    m_wallPotentialCombo = new QComboBox(this);
    m_wallPotentialCombo->addItem(tr("Harmonic"), 0);
    m_wallPotentialCombo->addItem(tr("LogFermi"), 1);
    m_wallPotentialCombo->setToolTip(tr("Wall potential function. Harmonic: "
        "V = ½k·d² (unbounded force); LogFermi: soft, temperature-dependent wall."));
    wallForm->addRow(tr("Potential:"), m_wallPotentialCombo);

    auto makeBoundSpin = [this](const QString& suffix) {
        auto* s = new QDoubleSpinBox(this);
        s->setRange(-1e5, 1e5);
        s->setDecimals(3);
        s->setSingleStep(0.1);
        s->setSuffix(suffix);
        s->setValue(0.0);
        return s;
    };
    // Pack two spin boxes side by side into a single QWidget for a QFormLayout row.
    auto makeRowPair = [](QWidget* a, QWidget* b) {
        auto* row = new QWidget;
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(a);
        h->addWidget(b);
        return row;
    };
    const QString angstrom = QStringLiteral(" Å");
    m_wallXminSpin = makeBoundSpin(angstrom);
    m_wallXmaxSpin = makeBoundSpin(angstrom);
    m_wallYminSpin = makeBoundSpin(angstrom);
    m_wallYmaxSpin = makeBoundSpin(angstrom);
    m_wallZminSpin = makeBoundSpin(angstrom);
    m_wallZmaxSpin = makeBoundSpin(angstrom);
    // Sane symmetric ±10 Å default so the rectangular preview is visible immediately.
    m_wallXminSpin->setValue(-10.0);  m_wallXmaxSpin->setValue(10.0);
    m_wallYminSpin->setValue(-10.0);  m_wallYmaxSpin->setValue(10.0);
    m_wallZminSpin->setValue(-10.0);  m_wallZmaxSpin->setValue(10.0);
    wallForm->addRow(tr("x min / max:"), makeRowPair(m_wallXminSpin, m_wallXmaxSpin));
    wallForm->addRow(tr("y min / max:"), makeRowPair(m_wallYminSpin, m_wallYmaxSpin));
    wallForm->addRow(tr("z min / max:"), makeRowPair(m_wallZminSpin, m_wallZmaxSpin));

    m_wallRadiusSpin = new QDoubleSpinBox(this);
    m_wallRadiusSpin->setRange(0.0, 1e5);
    m_wallRadiusSpin->setDecimals(3);
    m_wallRadiusSpin->setSingleStep(0.1);
    m_wallRadiusSpin->setSuffix(angstrom);
    m_wallRadiusSpin->setValue(8.0);
    m_wallRadiusSpin->setToolTip(tr("Spherical wall radius (origin-centred). "
        "0 = curcuma auto-size from molecule geometry (not previewable)."));
    wallForm->addRow(tr("Sphere radius:"), m_wallRadiusSpin);

    wallOuterLayout->addWidget(m_wallDetails);
    m_wallDetails->setVisible(false);  // hidden until enabled

    // Claude Generated 2026 - Live boundary-violation feedback: shown when walls
    // are enabled; updated from MoleculeViewer::wallViolationChanged via
    // MainWindow. The 3D wireframe also turns red when any atom is outside.
    m_wallStatusLabel = new QLabel(m_wallGroup);
    m_wallStatusLabel->setWordWrap(true);
    m_wallStatusLabel->setVisible(false);
    wallOuterLayout->addWidget(m_wallStatusLabel);

    innerLayout->addWidget(m_wallGroup);

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

    // Claude Generated 2026 - Opt-in: keep the force-field parameters/topology
    // fixed while interactively dragging atoms during a geometry optimization.
    // When on (default), keep-alive restarts reuse the existing FF and only move
    // atoms (no rebuild from the grab-distorted geometry — faster, crash-free).
    // When off, each restart rebuilds the FF from the current geometry (adaptive).
    m_optKeepParamsCheck = new QCheckBox(tr("Keep parameters while dragging"), this);
    m_optKeepParamsCheck->setChecked(true);
    m_optKeepParamsCheck->setToolTip(tr("Interactive Opt: keep the force-field parameters/topology fixed "
                                        "across keep-alive restarts instead of rebuilding them from the "
                                        "(grab-distorted) geometry. Recommended on."));
    optForm->addRow("", m_optKeepParamsCheck);

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
    auto* grabOuter = new QVBoxLayout(grabGroup);
    grabOuter->setContentsMargins(4, 4, 4, 4);
    grabOuter->setSpacing(4);

    m_grabStrengthSpin = new QDoubleSpinBox(this);
    m_grabStrengthSpin->setRange(1e-4, 10.0);
    m_grabStrengthSpin->setDecimals(4);
    m_grabStrengthSpin->setSingleStep(0.01);
    m_grabStrengthSpin->setValue(0.1);
    m_grabStrengthSpin->setToolTip(tr("World-space force per screen pixel (Eh/Bohr), Angstrom-to-Bohr corrected"));

    auto* grabForm = new QFormLayout;
    grabForm->addRow(tr("Strength:"), m_grabStrengthSpin);
    grabOuter->addLayout(grabForm);

    // Stiffness presets — map coupled α + maxShells to physical behaviour
    m_grabPresetCombo = new QComboBox(this);
    m_grabPresetCombo->addItem(tr("Soft (local drag)"), 0);   // α=0.2, shells=5
    m_grabPresetCombo->addItem(tr("Balanced"), 1);           // α=0.4, shells=3
    m_grabPresetCombo->addItem(tr("Stiff (rigid pull)"), 2);  // α=0.8, shells=1
    m_grabPresetCombo->setCurrentIndex(1);  // Balanced default
    m_grabPresetCombo->setToolTip(tr("Force propagation: Soft affects only nearest neighbours, "
                                     "Stiff pulls the whole fragment as a unit"));
    grabForm->addRow(tr("Stiffness:"), m_grabPresetCombo);

    // Advanced controls — hidden by default
    m_grabAdvancedCheck = new QCheckBox(tr("Advanced"), this);
    grabForm->addRow("", m_grabAdvancedCheck);

    m_grabAdvancedWidget = new QWidget(this);
    auto* advLayout = new QFormLayout(m_grabAdvancedWidget);
    advLayout->setContentsMargins(0, 0, 0, 0);

    m_grabAlphaSpin = new QDoubleSpinBox(this);
    m_grabAlphaSpin->setRange(0.0, 1.0);
    m_grabAlphaSpin->setDecimals(2);
    m_grabAlphaSpin->setSingleStep(0.05);
    m_grabAlphaSpin->setValue(0.4);
    m_grabAlphaSpin->setToolTip(tr("Shell decay factor α^depth (0 = only grabbed atom, 1 = uniform)"));
    advLayout->addRow(tr("α decay:"), m_grabAlphaSpin);

    m_grabMaxShellsSpin = new QSpinBox(this);
    m_grabMaxShellsSpin->setRange(-1, 20);
    m_grabMaxShellsSpin->setValue(3);
    m_grabMaxShellsSpin->setSpecialValueText(tr("∞"));
    m_grabMaxShellsSpin->setToolTip(tr("Max BFS depth for force propagation (-1 = unlimited)"));
    advLayout->addRow(tr("Max shells:"), m_grabMaxShellsSpin);

    m_grabAdvancedWidget->setVisible(false);
    grabOuter->addWidget(m_grabAdvancedWidget);

    innerLayout->addWidget(grabGroup);

    innerLayout->addStretch();

    inner->setLayout(innerLayout);
    scroll->setWidget(inner);
    outer->addWidget(scroll);

    // ---- Connections ----
    connect(m_startBtn, &QToolButton::clicked, this, &SimulationControlWidget::onStartClicked);
    connect(m_pauseBtn, &QToolButton::clicked, this, &SimulationControlWidget::onPauseClicked);
    connect(m_stepBtn, &QToolButton::clicked, this, &SimulationControlWidget::onStepClicked);
    connect(m_stopBtn, &QToolButton::clicked, this, &SimulationControlWidget::onStopClicked);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &SimulationControlWidget::onModeChanged);

    auto notifyConfig = [this]() { emit configChanged(buildConfig()); };
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    connect(m_methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    connect(m_topologyModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    connect(m_optimizerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    // Temperature slider: stays live during a run. A drag emits temperatureChanged() (forwarded
    // to the worker) and, while running, flags the ramp as overridden. Claude Generated 2026.
    connect(m_tempSlider, &TemperatureSlider::valueChanged, this, [this](double t) {
        emit temperatureChanged(t);
        if (m_running && m_tempOverrideLabel && m_tempRampEnableCheck->isChecked())
            m_tempOverrideLabel->setVisible(true);
        emit configChanged(buildConfig());
    });
    connect(m_timestepSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_stepsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notifyConfig);
    connect(m_fpsLimitSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notifyConfig);
    connect(m_hmassSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_gpuCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    connect(m_writeTrjCheck, &QCheckBox::toggled, this, notifyConfig);
    connect(m_perfCheck, &QCheckBox::toggled, this, notifyConfig);
    connect(m_convergenceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_optKeepParamsCheck, &QCheckBox::toggled, this, notifyConfig);
    connect(m_rattleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    connect(m_rattle12Check, &QCheckBox::toggled, this, notifyConfig);
    connect(m_rattle13Check, &QCheckBox::toggled, this, notifyConfig);
    connect(m_rattleTol12Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_rattleTol13Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_rattleMaxIterSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notifyConfig);

    // Claude Generated 2026 - RMSD-MTD: notify on every parameter change.
    connect(m_rmsdMtdEnableCheck, &QCheckBox::toggled, this, notifyConfig);
    connect(m_rmsdMtdKSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_rmsdMtdAlphaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_rmsdMtdAtomsEdit, &QLineEdit::textChanged, this, notifyConfig);
    connect(m_rmsdMtdRefFileEdit, &QLineEdit::textChanged, this, notifyConfig);
    connect(m_rmsdMtdMaxGaussiansSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notifyConfig);
    connect(m_rmsdMtdMaxHeightSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notifyConfig);
    connect(m_rmsdMtdEconvSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_rmsdMtdPaceSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notifyConfig);
    connect(m_rmsdMtdWtmtdCheck, &QCheckBox::toggled, this, notifyConfig);
    connect(m_rmsdMtdDtSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_rmsdMtdFreezeCheck, &QCheckBox::toggled, this, notifyConfig);

    // Claude Generated 2026 - Confinement walls: notify on every parameter change.
    connect(m_wallEnableCheck, &QCheckBox::toggled, this, notifyConfig);
    connect(m_wallTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    connect(m_wallPotentialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notifyConfig);
    connect(m_wallRadiusSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_wallXminSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_wallXmaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_wallYminSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_wallYminSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_wallYmaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_wallZminSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);
    connect(m_wallZmaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyConfig);

    // Show/hide groups based on mode and RATTLE selection
    connect(m_rattleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int index) { m_rattleDetails->setVisible(index > 0); });

    // Claude Generated 2026 - RMSD-MTD: reveal details on enable; gate ΔT by wtmtd.
    connect(m_rmsdMtdEnableCheck, &QCheckBox::toggled, this,
        [this](bool on) { m_rmsdMtdDetails->setVisible(on); });
    connect(m_rmsdMtdWtmtdCheck, &QCheckBox::toggled, this,
        [this](bool on) { m_rmsdMtdDtSpin->setEnabled(on); });

    // Claude Generated 2026 - Confinement walls: reveal details on enable.
    // All rows stay visible regardless of geometry — the geometry combo decides
    // which curcuma actually uses (rect bounds vs spheric radius) and which the
    // 3D viewer draws; greying/visibility swaps here would leave half-empty rows.
    connect(m_wallEnableCheck, &QCheckBox::toggled, this,
        [this](bool on) {
            m_wallDetails->setVisible(on);
            m_wallStatusLabel->setVisible(on);
            if (!on) m_wallStatusLabel->clear();
        });

    auto notifyGrab = [this]() {
        emit grabSettingsChanged(m_grabStrengthSpin->value(),
            m_grabAlphaSpin->value(), m_grabMaxShellsSpin->value());
    };
    connect(m_grabStrengthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyGrab);
    connect(m_grabAlphaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, notifyGrab);
    connect(m_grabMaxShellsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notifyGrab);

    // Preset → alpha + maxShells
    connect(m_grabPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int index) {
            static constexpr double alphas[] = { 0.2, 0.4, 0.8 };
            static constexpr int shells[] = { -1, 3, 1 };  // -1 = unlimited
            if (index >= 0 && index < 3) {
                m_grabAlphaSpin->setValue(alphas[index]);
                m_grabMaxShellsSpin->setValue(shells[index]);
            }
        });

    // Advanced toggle
    connect(m_grabAdvancedCheck, &QCheckBox::toggled, this,
        [this](bool on) { m_grabAdvancedWidget->setVisible(on); });

    // Spinbox manual change → switch preset to "custom" (deselect)
    auto markCustom = [this]() {
        m_grabPresetCombo->blockSignals(true);
        m_grabPresetCombo->setCurrentIndex(-1);
        m_grabPresetCombo->blockSignals(false);
    };
    connect(m_grabAlphaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, markCustom);
    connect(m_grabMaxShellsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, markCustom);

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
    cfg.temperature = m_tempSlider->value();
    cfg.timestep = m_timestepSpin->value();
    cfg.steps = m_stepsSpin->value();
    cfg.fpsLimit = m_fpsLimitSpin->value();
    cfg.hmass = m_hmassSpin->value();
    cfg.gpu = m_gpuCombo->currentData().toString();
    cfg.writeTrajectory = m_writeTrjCheck->isChecked();
    cfg.performanceAnalysis = m_perfCheck->isChecked();
    cfg.convergence = m_convergenceSpin->value();
    cfg.optKeepParameters = m_optKeepParamsCheck->isChecked();
    cfg.rattleMode    = m_rattleCombo->currentData().toInt();
    cfg.rattle12      = m_rattle12Check->isChecked();
    cfg.rattle13      = m_rattle13Check->isChecked();
    cfg.rattleTol12   = m_rattleTol12Spin->value();
    cfg.rattleTol13   = m_rattleTol13Spin->value();
    cfg.rattleMaxIter = m_rattleMaxIterSpin->value();

    // GFN-FF topology mode
    cfg.topologyMode = m_topologyModeCombo->currentData().toString();

    // Claude Generated 2026 - RMSD metadynamics (MD bias, curcuma SimpleMD rmsd_mtd).
    cfg.rmsdMtd              = m_rmsdMtdEnableCheck->isChecked();
    cfg.rmsdMtdK             = m_rmsdMtdKSpin->value();
    cfg.rmsdMtdAlpha         = m_rmsdMtdAlphaSpin->value();
    cfg.rmsdMtdAtoms         = m_rmsdMtdAtomsEdit->text().trimmed();
    cfg.rmsdMtdRefFile       = m_rmsdMtdRefFileEdit->text().trimmed();
    cfg.rmsdMtdMaxGaussians  = m_rmsdMtdMaxGaussiansSpin->value();
    cfg.rmsdMtdMaxHeight     = m_rmsdMtdMaxHeightSpin->value();
    cfg.rmsdMtdEconv         = m_rmsdMtdEconvSpin->value();
    cfg.rmsdMtdPace          = m_rmsdMtdPaceSpin->value();
    cfg.rmsdMtdWtmtd         = m_rmsdMtdWtmtdCheck->isChecked();
    cfg.rmsdMtdDt            = m_rmsdMtdDtSpin->value();
    cfg.rmsdMtdFreezeInherited = m_rmsdMtdFreezeCheck->isChecked();

    // Claude Generated 2026 - Confinement walls (curcuma SimpleMD wall_* params).
    cfg.wallEnabled  = m_wallEnableCheck->isChecked();
    cfg.wallType      = m_wallTypeCombo->currentData().toInt();
    cfg.wallHarmonic  = (m_wallPotentialCombo->currentData().toInt() == 0);
    cfg.wallXmin = m_wallXminSpin->value();  cfg.wallXmax = m_wallXmaxSpin->value();
    cfg.wallYmin = m_wallYminSpin->value();  cfg.wallYmax = m_wallYmaxSpin->value();
    cfg.wallZmin = m_wallZminSpin->value();  cfg.wallZmax = m_wallZmaxSpin->value();
    cfg.wallRadius   = m_wallRadiusSpin->value();

    // Claude Generated 2026 - Temperature ramp (global) + regions (curcuma SimpleMD temp_* params).
    cfg.tempRamp = m_tempRampEnableCheck->isChecked();
    cfg.tempSchedule.clear();
    if (cfg.tempRamp) {
        QStringList segs;
        for (int r = 0; r < m_tempRampTable->rowCount(); ++r) {
            const QTableWidgetItem* targetItem = m_tempRampTable->item(r, 0);
            const auto* modeCombo = qobject_cast<QComboBox*>(m_tempRampTable->cellWidget(r, 1));
            const QTableWidgetItem* valueItem = m_tempRampTable->item(r, 2);
            const QString target = targetItem ? targetItem->text().trimmed() : QString();
            const QString mode = modeCombo ? modeCombo->currentText() : QStringLiteral("steps");
            const QString value = valueItem ? valueItem->text().trimmed() : QString();
            if (target.isEmpty() || value.isEmpty())
                continue;
            segs << QStringLiteral("%1:%2:%3").arg(target, mode, value);
        }
        cfg.tempSchedule = segs.join(QLatin1Char(';'));
    }

    cfg.tempRegions.clear();
    for (int r = 0; r < m_tempRegionTable->rowCount(); ++r) {
        const QTableWidgetItem* atomsItem = m_tempRegionTable->item(r, 0);
        const QTableWidgetItem* tItem = m_tempRegionTable->item(r, 1);
        const QTableWidgetItem* schedItem = m_tempRegionTable->item(r, 2);
        const QString atoms = atomsItem ? atomsItem->text().trimmed() : QString();
        if (atoms.isEmpty())
            continue;
        TempRegion reg;
        reg.atoms = atoms;
        reg.temperature = tItem ? tItem->text().toDouble() : 300.0;
        reg.schedule = schedItem ? schedItem->text().trimmed() : QString();
        cfg.tempRegions.push_back(reg);
    }

    return cfg;
}

// Claude Generated 2026 - append a row to the global ramp table (Target | Mode combo | Value).
void SimulationControlWidget::addRampSegmentRow(double target, const QString& mode, double value)
{
    const QSignalBlocker blocker(m_tempRampTable);  // suppress cellChanged during the build
    const int row = m_tempRampTable->rowCount();
    m_tempRampTable->insertRow(row);
    m_tempRampTable->setItem(row, 0, new QTableWidgetItem(QString::number(target, 'f', 0)));
    auto* modeCombo = new QComboBox(m_tempRampTable);
    modeCombo->addItems({ QStringLiteral("steps"), QStringLiteral("reach") });
    modeCombo->setCurrentText(mode);
    m_tempRampTable->setCellWidget(row, 1, modeCombo);
    m_tempRampTable->setItem(row, 2, new QTableWidgetItem(QString::number(value, 'f', 0)));
    connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int) { emit configChanged(buildConfig()); });
}

// Claude Generated 2026 - append a row to the regions table (Atoms | Start T | Schedule).
void SimulationControlWidget::addRegionRow(const QString& atoms, double temperature, const QString& schedule)
{
    const QSignalBlocker blocker(m_tempRegionTable);
    const int row = m_tempRegionTable->rowCount();
    m_tempRegionTable->insertRow(row);
    m_tempRegionTable->setItem(row, 0, new QTableWidgetItem(atoms));
    m_tempRegionTable->setItem(row, 1, new QTableWidgetItem(QString::number(temperature, 'f', 0)));
    m_tempRegionTable->setItem(row, 2, new QTableWidgetItem(schedule));
}

void SimulationControlWidget::onStartClicked()
{
    if (m_atoms.isEmpty()) {
        m_statusLabel->setText(tr("No molecule loaded."));
        return;
    }

    // Tear down any prior worker
    if (m_thread && m_thread->isRunning()) {
        if (m_worker)
            m_worker->requestStop();
        m_thread->quit();
        m_thread->wait(2000);
    }

    m_worker = new SimulationWorker;
    // Claude Generated 2026 - Emit workerStarted BEFORE setMolecule so the
    // MainWindow can re-sync m_atoms with the viewer's *current* geometry
    // (e.g. the final coordinates of the previous MD/Opt run) and re-arm
    // the viewer's throttled moleculeUpdated emit. After this signal,
    // m_atoms holds the live viewer state and is what the worker should
    // start from.
    emit workerStarted(m_worker);
    m_worker->setMolecule(m_atoms);
    m_worker->setBonds(m_bonds);
    m_worker->setConfig(buildConfig());

    m_thread = new QThread(this);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &SimulationWorker::run);
    connect(m_worker, &SimulationWorker::frameReady, this, &SimulationControlWidget::onFrameReady);
    connect(m_worker, &SimulationWorker::finished, this, &SimulationControlWidget::onSimulationFinished);
    connect(m_worker, &SimulationWorker::paused, this, [this]() {
        m_pauseBtn->setText(tr("▶"));
        m_paused = true;
        setState(tr("⏸ Paused"), "#d4a017");  // amber
    });
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

    setRunning(true);
    setState(tr("● Running"), "#27ae60");  // green
    m_statusLabel->setText(tr("Starting..."));
    m_thread->start();
}

// Claude Generated 2026 - Step button: spawn a fresh worker that runs exactly
// one MD step / one Opt iteration and emits one frame, then finishes. We
// re-use the dock's running-state plumbing so the toolbar looks identical to
// the auto-run path (Start greyed, Pause/Stop/Step shown-but-Stop-only-meaningful).
// The Speed knob throttles the click rate so "max XXX FPS" is honoured.
void SimulationControlWidget::onStepClicked()
{
    if (m_atoms.isEmpty()) {
        m_statusLabel->setText(tr("No molecule loaded."));
        return;
    }

    // Claude Generated 2026 - Throttle: only allow one Step click per 1000/fpsLimit ms.
    // This is the dock-side enforcement of "max XXX FPS" the user asked for.
    if (m_stepThrottleTimer.isValid() && m_stepThrottleTimer.elapsed() < (1000 / qMax(1, m_fpsLimitSpin->value()))) {
        return;
    }
    m_stepThrottleTimer.restart();

    // Tear down any prior worker (shouldn't exist if Step is only enabled when idle)
    if (m_thread && m_thread->isRunning()) {
        if (m_worker)
            m_worker->requestStop();
        m_thread->quit();
        m_thread->wait(2000);
    }

    m_worker = new SimulationWorker;
    emit workerStarted(m_worker);
    m_worker->setMolecule(m_atoms);
    m_worker->setBonds(m_bonds);
    m_worker->setConfig(buildConfig());

    m_thread = new QThread(this);
    m_worker->moveToThread(m_thread);

    // Single-shot step: QThread::started → stepOnce() (NOT run()).
    connect(m_thread, &QThread::started, m_worker, &SimulationWorker::stepOnce);
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

    // Reset FPS measurement
    m_frameCount = 0;
    m_actualFps = 0.0;
    m_fpsTimer.invalidate();

    setRunning(true);
    setState(tr("⏭ Stepping"), "#2980b9");  // blue
    m_statusLabel->setText(tr("Stepping once..."));
    m_thread->start();
}

void SimulationControlWidget::onPauseClicked()
{
    if (!m_worker)
        return;
    if (m_paused) {
        m_worker->requestResume();
        m_pauseBtn->setText(tr("⏸"));
        setState(tr("● Running"), "#27ae60");  // green
        m_paused = false;
    } else {
        m_worker->requestPause();
        m_pauseBtn->setText(tr("▶"));
        setState(tr("⏸ Paused"), "#d4a017");  // amber
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
    setState(tr("● Finished"), "#7f8c8d");  // grey
    m_statusLabel->setText(tr("Finished."));
    emit simulationFinished();
}

// Claude Generated 2026 - Mirror MainWindow's modified-state in the dock UI.
void SimulationControlWidget::setStructureModified(bool modified)
{
    m_structureModifiedState = modified;
    if (m_modifiedLabel)
        m_modifiedLabel->setVisible(modified);
    if (m_saveBtn)
        m_saveBtn->setEnabled(!m_running && m_structureModifiedState);
    // Reset availability is driven by setResetEnabled(); do not disable it here.
}

void SimulationControlWidget::setResetEnabled(bool enabled)
{
    if (m_resetBtn)
        m_resetBtn->setEnabled(enabled);
}

void SimulationControlWidget::onModeChanged(int /*index*/)
{
    bool isMD = (m_modeCombo->currentData().toInt()
        == static_cast<int>(SimulationConfig::Mode::MolecularDynamics));

    // Show/hide mode-specific groups
    m_mdGroup->setVisible(isMD);
    m_rattleGroup->setVisible(isMD);
    m_rmsdMtdGroup->setVisible(isMD);
    m_wallGroup->setVisible(isMD);  // walls are MD-only (curcuma SimpleMD)
    m_tempRampGroup->setVisible(isMD);    // temperature ramp is MD-only
    m_tempRegionGroup->setVisible(isMD);  // temperature regions are MD-only
    m_optGroup->setVisible(!isMD);

    // Speed is visible in both modes (single-step optimisation uses it as a
    // click-rate cap; MD uses it as the auto-run emit cadence cap).
    // m_fpsLimitSpin is already a top-level control — no per-mode visibility needed.
}

// Claude Generated 2026 - Live confinement-wall boundary feedback. count == 0
// => all atoms inside (green check); count > 0 => N atoms outside the wall
// (warning). The 3D wireframe is recoloured red independently by the viewer.
void SimulationControlWidget::setWallViolationCount(int count)
{
    if (!m_wallStatusLabel)
        return;
    if (count <= 0) {
        m_wallStatusLabel->setText(tr("✓ all atoms inside the wall"));
        m_wallStatusLabel->setStyleSheet(QStringLiteral("color: #4caf50;"));
    } else {
        m_wallStatusLabel->setText(tr("⚠ %1 atom(s) outside the wall").arg(count));
        m_wallStatusLabel->setStyleSheet(QStringLiteral("color: #e05757; font-weight: bold;"));
    }
}

// Claude Generated 2026 - Programmatic mode setter for the CLI auto-start
// (-md / -opt). Drives the combo via findData + setCurrentIndex so the existing
// currentIndexChanged -> onModeChanged connection toggles the MD/Opt group
// visibility and buildConfig() picks up the requested mode.
void SimulationControlWidget::setMode(SimulationConfig::Mode mode)
{
    if (!m_modeCombo)
        return;
    const int idx = m_modeCombo->findData(static_cast<int>(mode));
    if (idx < 0) {
        qWarning() << "setMode: unknown mode" << static_cast<int>(mode);
        return;
    }
    m_modeCombo->setCurrentIndex(idx);
}

void SimulationControlWidget::setRunning(bool running)
{
    m_running = running;
    m_startBtn->setEnabled(!running);
    m_pauseBtn->setEnabled(running);
    m_stopBtn->setEnabled(running);
    // Step is the inverse: enabled when idle so the user can iterate manually.
    m_stepBtn->setEnabled(!running);
    m_saveBtn->setEnabled(!m_running && m_structureModifiedState);
    // Reset stays enabled while running so the user can abort and revert at any
    // time. Its own availability is controlled by setResetEnabled().
    m_modeCombo->setEnabled(!running);
    m_methodCombo->setEnabled(!running);
    m_optimizerCombo->setEnabled(!running);
    // Temperature slider stays editable during a run — that is the whole point of the live
    // setpoint; a drag is forwarded to the worker (temperatureChanged). Claude Generated 2026.
    // The ramp/region configuration is fixed at start, so those lock while running.
    m_tempRampEnableCheck->setEnabled(!running);
    m_tempRampTable->setEnabled(!running);
    m_tempRegionTable->setEnabled(!running);
    if (!running && m_tempOverrideLabel)
        m_tempOverrideLabel->setVisible(false);  // clear the override badge on (re)start
    m_timestepSpin->setEnabled(!running);
    m_stepsSpin->setEnabled(!running);
    // Speed stays editable during a run — the user often wants to slow down
    // or speed up a live MD/Opt without stopping it.
    // m_fpsLimitSpin->setEnabled(!running);
    m_hmassSpin->setEnabled(!running);
    m_gpuCombo->setEnabled(!running);
    m_writeTrjCheck->setEnabled(!running);
    m_perfCheck->setEnabled(!running);
    m_convergenceSpin->setEnabled(!running);
    if (m_optKeepParamsCheck)
        m_optKeepParamsCheck->setEnabled(!running);
    m_rattleCombo->setEnabled(!running);
    m_rattle12Check->setEnabled(!running);
    m_rattle13Check->setEnabled(!running);
    m_rattleTol12Spin->setEnabled(!running);
    m_rattleTol13Spin->setEnabled(!running);
    m_rattleMaxIterSpin->setEnabled(!running);

    // Claude Generated 2026 - RMSD-MTD controls lock during a run.
    m_rmsdMtdEnableCheck->setEnabled(!running);
    m_rmsdMtdKSpin->setEnabled(!running);
    m_rmsdMtdAlphaSpin->setEnabled(!running);
    m_rmsdMtdAtomsEdit->setEnabled(!running);
    m_rmsdMtdRefFileEdit->setEnabled(!running);
    m_rmsdMtdMaxGaussiansSpin->setEnabled(!running);
    m_rmsdMtdMaxHeightSpin->setEnabled(!running);
    m_rmsdMtdEconvSpin->setEnabled(!running);
    m_rmsdMtdPaceSpin->setEnabled(!running);
    m_rmsdMtdWtmtdCheck->setEnabled(!running);
    m_rmsdMtdFreezeCheck->setEnabled(!running);
    // ΔT stays gated by wtmtd; re-apply that constraint after the run-state pass.
    m_rmsdMtdDtSpin->setEnabled(!running && m_rmsdMtdWtmtdCheck->isChecked());

    emit simulationRunningChanged(running);
}

// Claude Generated 2026 - Update the state pill (color-coded run-state indicator
// next to the button bar). Pass a plain text + a CSS color name.
void SimulationControlWidget::setState(const QString& label, const QString& color)
{
    if (!m_stateLabel)
        return;
    m_stateLabel->setText(QStringLiteral("<span style='color:%1; font-weight:600;'>%2</span>")
                              .arg(color, label.toHtmlEscaped()));
}
