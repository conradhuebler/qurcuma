// simulationcontrolwidget.h - Full inline simulation controls for the viewer dock
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Interactive Simulation Integration (Phase 6: single source of truth)

#pragma once

#include "simulationworker.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QSpinBox>
#include <QThread>
#include <QToolButton>
#include <QWidget>

class QTableWidget;
class QFormLayout;
class CollapsibleSection;
class TemperatureSlider;  // Claude Generated 2026 - vertical temperature-colored slider

/**
 * @brief Dock widget holding every MD/Opt parameter + interactive-grab controls.
 *
 * Claude Generated - Phase 6 replaces the old SimulationDialog; all knobs (mode,
 * method, temperature, timestep, steps, FPS, GPU, trajectory output,
 * optimization tolerances, and the grab-force α/max-shells/strength triple)
 * live here. MainWindow feeds it atoms+bonds on every molecule change and
 * wires the freshly-spawned worker to the viewer on start.
 */
class SimulationControlWidget : public QWidget {
    Q_OBJECT

public:
    explicit SimulationControlWidget(QWidget* parent = nullptr);
    ~SimulationControlWidget() override;

    /** @brief Feed the current molecule + bond graph to the worker before start. */
    void setMolecule(const QVector<MoleculeViewer::Atom>& atoms,
        const QVector<MoleculeViewer::Bond>& bonds = {});

    const QVector<MoleculeViewer::Atom>& currentAtoms() const { return m_atoms; }

    SimulationConfig currentConfig() const { return buildConfig(); }

    /** @brief Drive every widget from a config (inverse of buildConfig). Used to
     *  restore a lesson structure's stored simulation conditions. Signals are
     *  suppressed during the apply, then configChanged() is emitted once.
     *  Claude Generated 2026. */
    void applyConfig(const SimulationConfig& cfg);

    /** @brief Set the MD/Opt mode programmatically (drives the combo so that
     *  buildConfig() reflects the requested mode). Used by the CLI auto-start
     *  (-md / -opt). Claude Generated 2026. */
    void setMode(SimulationConfig::Mode mode);

    /** @brief Bounded geometry optimization with the current method — the
     *  builder's "Clean up" (refuses while a run is active). Claude Generated 2026. */
    void startQuickOptimization(int maxSteps = 50);

    /** @brief Grab strength (world Å/Bohr per screen pixel) for the viewer. */
    double grabStrength() const { return m_grabStrengthSpin ? m_grabStrengthSpin->value() : 0.1; }
    double grabAlpha() const { return m_grabAlphaSpin ? m_grabAlphaSpin->value() : 0.4; }
    int grabMaxShells() const { return m_grabMaxShellsSpin ? m_grabMaxShellsSpin->value() : 3; }

    /** @brief Auto-snapshot stride. 0 = disabled, N > 0 = snapshot every N steps/iterations. */
    int autoStride() const { return m_strideSpin ? m_strideSpin->value() : 0; }

signals:
    void simulationFinished();
    void configChanged(SimulationConfig);
    void simulationRunningChanged(bool running);
    void workerStarted(SimulationWorker* worker);

    /** @brief Emitted whenever the temperature slider moves. During a run MainWindow
     *  forwards it live to the worker (SimulationWorker::setTargetTemperature).
     *  Claude Generated 2026. */
    void temperatureChanged(double temperature);

    /** @brief Emitted when the wall_temp slider moves — live during a run. */
    void wallTempChanged(double T);
    /** @brief Emitted when the wall_beta slider moves — live during a run. */
    void wallBetaChanged(double beta);

    /** @brief Emitted whenever grab sliders change, so the viewer can update
     *  its live scaling. */
    void grabSettingsChanged(double strength, double alpha, int maxShells);

    // Claude Generated 2026 - User clicked the in-dock "Save" button. MainWindow
    // routes this to the central saveStructure() implementation.
    void saveStructureRequested();

    // Claude Generated 2026 - User clicked the in-dock "Reset" button. The int
    // argument is the snapshot index to restore (0 = first snapshot, set
    // automatically at load time). MainWindow resolves it to the matching snapshot.
    void resetStructureRequested(int index);

public slots:
    void onStartClicked();
    void onPauseClicked();
    void onStopClicked();
    void onStepClicked();
    void onFrameReady(SimulationFramePtr frame);
    void onSimulationFinished();
    void onModeChanged(int index);

    // Claude Generated 2026 - Receive modified-state from MainWindow and reflect
    // it in the dock UI (label visibility + save-button enabled). Public so
    // MainWindow can drive it from both the moleculeUpdated lambda and the
    // save/load paths.
    void setStructureModified(bool modified);

    // Claude Generated 2026 - MainWindow tells the dock whether at least one
    // snapshot is available, so the Reset button can stay enabled independently
    // of the modified flag.
    void setResetEnabled(bool enabled);

    // Claude Generated 2026 - Live boundary-violation feedback from the viewer:
    // updates the "N atoms outside / all inside" label in the Confinement Walls
    // group. count == 0 means the current structure is fully inside the wall.
    void setWallViolationCount(int count);

private:
    void setupUI();
    // Claude Generated 2026 - WP T1 rest: signal wiring extracted from setupUI() so
    // the latter stays at composition altitude. All connect() targets are m_* members.
    void setupConnections();
    // Claude Generated 2026 - builders extracted from setupUI(). Each creates its group
    // or section (populating the m_* members it owns) and returns it for setupUI() to
    // add to the scroll layout. UX stage 6 S1: the basic parameters (curcuma tier
    // "primary") sit in the Method and MD groups; every optional feature is a
    // CollapsibleSection with a switch in its header; the rest is in "Advanced".
    QGroupBox* createMethodGroup();
    QGroupBox* createMdGroup();
    CollapsibleSection* createTempRampSection();
    CollapsibleSection* createTempRegionSection();
    CollapsibleSection* createRattleSection();
    CollapsibleSection* createRmsdMtdSection();
    CollapsibleSection* createWallSection();
    CollapsibleSection* createAdvancedSection();
    CollapsibleSection* createGrabSection();
    /// Show only the thermostat fields the chosen thermostat reads.
    void updateThermostatRows();
    /// Show the GFN-FF topology row only for GFN-FF.
    void updateMethodRows();
    void setRunning(bool running);
    void setState(const QString& label, const QString& color);  // Claude Generated 2026 - state pill
    SimulationConfig buildConfig() const;
    /// Shared worker spawn for Start and the quick optimization. Claude Generated 2026.
    void startWithConfig(const SimulationConfig& cfg);

    // Claude Generated 2026 - temperature ramp / region table row helpers
    void addRampSegmentRow(double target, const QString& mode, double value);
    void addRegionRow(const QString& atoms, double temperature, const QString& schedule);

    // --- Mode / method ---
    QComboBox* m_modeCombo = nullptr;
    QFormLayout* m_methodForm = nullptr;    // Method group (method, optimizer, charge, spin)
    QFormLayout* m_mdForm = nullptr;        // MD group (thermostat rows, time step, steps)
    QFormLayout* m_advancedForm = nullptr;  // Advanced section (mode-specific rows)
    QComboBox* m_methodCombo = nullptr;
    QSpinBox* m_chargeSpin = nullptr;   // curcuma "charge" (Claude Generated 2026)
    QSpinBox* m_spinSpin = nullptr;     // curcuma "spin": unpaired electrons
    QComboBox* m_optimizerCombo = nullptr;  // Claude Generated 2026 - opt algorithm picker

    // --- Common (visible in both modes) ---
    QSpinBox* m_fpsLimitSpin = nullptr;  // Speed: max emits per second
    QSpinBox* m_strideSpin = nullptr;    // Claude Generated 2026 - auto-snapshot stride
    QLabel* m_stateLabel = nullptr;      // Claude Generated 2026 - "○ Ready / ● Running" pill

    // --- MD parameters ---
    TemperatureSlider* m_tempSlider = nullptr;  // Claude Generated 2026 - vertical, live during runs
    // Thermostat selection (Claude Generated 2026 - curcuma SimpleMD "Thermostat" PARAMs).
    QComboBox*      m_thermostatCombo = nullptr;   // csvr/berendsen/andersen/nosehover/none
    QDoubleSpinBox* m_couplingSpin = nullptr;      // coupling time (fs)
    QDoubleSpinBox* m_andersenProbSpin = nullptr;  // Andersen collision probability
    QSpinBox*       m_noseChainSpin = nullptr;     // Nosé-Hoover chain length
    QDoubleSpinBox* m_timestepSpin = nullptr;
    QSpinBox* m_stepsSpin = nullptr;
    QDoubleSpinBox* m_hmassSpin = nullptr;  // Hydrogen mass scaling
    QComboBox* m_gpuCombo = nullptr;
    QCheckBox* m_writeTrjCheck = nullptr;
    QCheckBox* m_perfCheck = nullptr;

    // --- GFN-FF topology mode ---
    QComboBox* m_topologyModeCombo = nullptr;  // "auto" or "constant"

    // --- Optimization parameters ---
    QDoubleSpinBox* m_convergenceSpin = nullptr;
    QCheckBox* m_optKeepParamsCheck = nullptr;  // Claude Generated 2026 - keep FF params across interactive Opt restarts

    // --- MD/Opt specific groups (shown/hidden based on mode) ---
    QGroupBox*      m_mdGroup = nullptr;       // MD parameters
    CollapsibleSection* m_rattleSection = nullptr;  // RATTLE constraints (switch = on/off)
    QCheckBox*      m_rattleEnableCheck = nullptr;  // the section's switch
    QComboBox*      m_rattleCombo = nullptr;        // 1 = all bonds, 2 = bonds to H only
    QCheckBox*      m_rattle12Check = nullptr;
    QCheckBox*      m_rattle13Check = nullptr;
    QDoubleSpinBox* m_rattleTol12Spin = nullptr;
    QDoubleSpinBox* m_rattleTol13Spin = nullptr;
    QSpinBox*       m_rattleMaxIterSpin = nullptr;

    // --- RMSD metadynamics (MD bias, curcuma SimpleMD rmsd_mtd) ---
    CollapsibleSection* m_rmsdMtdSection = nullptr;
    QCheckBox*      m_rmsdMtdEnableCheck = nullptr;  // the section's switch
    QDoubleSpinBox* m_rmsdMtdKSpin = nullptr;
    QDoubleSpinBox* m_rmsdMtdAlphaSpin = nullptr;
    QLineEdit*      m_rmsdMtdAtomsEdit = nullptr;
    QLineEdit*      m_rmsdMtdRefFileEdit = nullptr;
    QSpinBox*       m_rmsdMtdMaxGaussiansSpin = nullptr;
    QSpinBox*       m_rmsdMtdMaxHeightSpin = nullptr;
    QDoubleSpinBox* m_rmsdMtdStrideSpin = nullptr;   // rmsd_mtd_deposit_stride (fs)
    QDoubleSpinBox* m_rmsdMtdRdepSpin = nullptr;     // rmsd_mtd_r_dep (Å, -1 = auto)
    QCheckBox*      m_rmsdMtdWtmtdCheck = nullptr;
    QDoubleSpinBox* m_rmsdMtdDtSpin = nullptr;
    QCheckBox*      m_rmsdMtdFreezeCheck = nullptr;

    // --- Confinement walls (curcuma SimpleMD wall_* params) ---
    CollapsibleSection* m_wallSection = nullptr;
    QCheckBox*       m_wallEnableCheck = nullptr;  // the section's switch
    QComboBox*       m_wallTypeCombo = nullptr;   // 0=None, 1=Spheric, 2=Rectangular
    QComboBox*       m_wallPotentialCombo = nullptr; // 0=Harmonic, 1=LogFermi
    QDoubleSpinBox*  m_wallRadiusSpin = nullptr;
    QDoubleSpinBox*  m_wallXminSpin = nullptr;
    QDoubleSpinBox*  m_wallXmaxSpin = nullptr;
    QDoubleSpinBox*  m_wallYminSpin = nullptr;
    QDoubleSpinBox*  m_wallYmaxSpin = nullptr;
    QDoubleSpinBox*  m_wallZminSpin = nullptr;
    QDoubleSpinBox*  m_wallZmaxSpin = nullptr;
    QLabel*          m_wallStatusLabel = nullptr;  // live boundary-violation feedback
    TemperatureSlider* m_wallTempSlider = nullptr; // wall_temp: energy scale (K), live during run
    TemperatureSlider* m_wallBetaSlider = nullptr; // wall_beta: steepness (β), live during run

    // --- Temperature ramp (global) + regions (curcuma SimpleMD temp_* params) ---
    CollapsibleSection* m_tempRampSection = nullptr;
    QCheckBox*    m_tempRampEnableCheck = nullptr;  // the section's switch
    QTableWidget* m_tempRampTable = nullptr;     // columns: Target(K) | Mode | Value
    CollapsibleSection* m_tempRegionSection = nullptr;
    QCheckBox*    m_tempRegionEnableCheck = nullptr;  // the section's switch (off = no regions sent)
    QTableWidget* m_tempRegionTable = nullptr;   // columns: Atoms | Start T(K) | Schedule
    QLabel*       m_tempOverrideLabel = nullptr; // "ramp overridden" badge after a live drag

    // --- Interactive grab ---
    QDoubleSpinBox* m_grabStrengthSpin = nullptr;
    QDoubleSpinBox* m_grabAlphaSpin = nullptr;
    QSpinBox* m_grabMaxShellsSpin = nullptr;
    QComboBox* m_grabPresetCombo = nullptr;

    // --- Buttons / status ---
    QToolButton* m_startBtn = nullptr;
    QToolButton* m_pauseBtn = nullptr;
    QToolButton* m_stepBtn = nullptr;       // Claude Generated 2026 - one-shot step (works in MD + Opt)
    QToolButton* m_stopBtn = nullptr;
    QToolButton* m_saveBtn = nullptr;       // Claude Generated 2026 - in-dock save
    QToolButton* m_resetBtn = nullptr;      // Claude Generated 2026 - in-dock reset
    QLabel* m_modifiedLabel = nullptr;      // Claude Generated 2026 - "● Modified" hint
    QLabel* m_statusLabel = nullptr;

    // --- State ---
    bool m_structureModifiedState = false;  // Claude Generated 2026 - tracked separately from button state
    bool m_running = false;  // Claude Generated 2026 - tracked for derived button enable logic
    QElapsedTimer m_fpsTimer;
    int m_frameCount = 0;
    double m_actualFps = 0.0;
    QVector<MoleculeViewer::Atom> m_atoms;
    QVector<MoleculeViewer::Bond> m_bonds;
    SimulationConfig m_config;
    SimulationWorker* m_worker = nullptr;
    QThread* m_thread = nullptr;
    bool m_paused = false;
    // Claude Generated 2026 - throttle for the Step button: re-enabled after 1000/fpsLimit ms
    // so the user can click at the configured "max XXX FPS" but not faster.
    QElapsedTimer m_stepThrottleTimer;
};
