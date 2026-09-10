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

class QTableWidget;
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
    /// What a run is doing right now, for a caller that is not the dock itself
    /// (the tool layer). A value type on purpose: it crosses to the agent loop's
    /// thread, which must not touch widgets. Claude Generated 2026.
    struct LiveState {
        bool running = false;
        bool paused = false;
        QString mode;               ///< "md" | "opt", empty when idle
        QString method;
        int step = 0;
        int totalSteps = 0;
        double energy = 0.0;        ///< Eh
        double kineticEnergy = 0.0; ///< Eh, MD only
        double temperature = 0.0;   ///< K, from the kinetic energy; MD only
        double targetTemperature = 0.0;
        double timeFs = 0.0;        ///< MD only
        /// Volume of the confinement container in A^3, 0 when no wall is set.
        /// Carried here so a density can be worked out off the GUI thread.
        /// Claude Generated 2026.
        double containerVolume = 0.0;
    };

    explicit SimulationControlWidget(QWidget* parent = nullptr);
    ~SimulationControlWidget() override;

    /// Is a worker thread up? Claude Generated 2026.
    bool isRunning() const;
    /// Latest live state; step and energies are those of the last frame.
    LiveState liveState() const { return m_liveState; }

    /// Start a run from @p cfg. The dock's own controls are driven from it first,
    /// so what runs is what the operator can see, and the run then starts from the
    /// controls exactly as pressing Start would. Returns false and fills @p error
    /// when there is no structure or a run is already going. Claude Generated 2026.
    bool startRun(const SimulationConfig& cfg, QString* error = nullptr);

    /// Pause and resume, each idempotent -- unlike the button, which toggles.
    /// Claude Generated 2026.
    void pauseRun();
    void resumeRun();

    /// Move the live thermostat setpoint, i.e. the slider, which is what reaches
    /// the worker. Claude Generated 2026.
    void setLiveTemperature(double kelvin);

    /// The values the dock's own combos accept. Read out of the combos rather than
    /// written down a second time: a tool schema that listed them by hand would be
    /// the third copy of the same list and the first to go stale. Claude Generated 2026.
    QStringList methodValues() const;
    QStringList optimizerValues() const;
    QStringList thermostatValues() const;

    /// Pull on atoms while the run continues: @p forces[i] (Eh/Bohr, model space)
    /// acts on @p atoms[i], each spread through the bond graph with exponential
    /// decay. Sticky -- it is re-applied at every step until clearExternalForces().
    /// Reaches the worker over the same queued connection the mouse grab uses.
    /// Claude Generated 2026.
    void requestExternalForces(const QVector<int>& atoms, const QVector<QVector3D>& forces,
                               double alpha, int maxShells);
    void clearExternalForces();

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

    /** @brief Reaction-events group: take a snapshot at every bond event (reactive GFN-FF). */
    bool snapshotOnEvent() const { return m_snapshotOnEventCheck && m_snapshotOnEventCheck->isChecked(); }

signals:
    void simulationFinished();
    void configChanged(SimulationConfig);
    /** @brief One reaction event line (reactive GFN-FF): "step N: N1-H4 formed (dE ...)".
     *  Emitted from onFrameReady on the GUI thread, after the viewer adopted the
     *  frame. Claude Generated 2026. */
    void reactionEvent(int step, const QString& text);

    /** @brief The run ended; @p reason is one plain sentence and @p aborted says
     *  whether the engine stopped it. Mirrored into the output dock. Claude Generated 2026. */
    void runEnded(const QString& reason, bool aborted);
    void simulationRunningChanged(bool running);
    void workerStarted(SimulationWorker* worker);

    /** @brief Emitted whenever the temperature slider moves. During a run MainWindow
     *  forwards it live to the worker (SimulationWorker::setTargetTemperature).
     *  Claude Generated 2026. */
    void temperatureChanged(double temperature);
    /// Live progress for consumers outside the dock, on every frame and on every
    /// change of the running/paused state. Claude Generated 2026.
    void liveStateChanged(SimulationControlWidget::LiveState state);
    /// Every frame, for consumers outside the dock that need the geometry itself
    /// rather than the summary -- the tool layer evaluates watched quantities on
    /// it as it arrives. Claude Generated 2026.
    void frameObserved(SimulationFramePtr frame);
    /// Forwarded to the worker in wireSimulationWorker(). Claude Generated 2026.
    void externalForcesRequested(QVector<int> atoms, QVector<QVector3D> forces,
                                 double alpha, int maxShells);
    void externalForcesCleared();

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
    void onSimulationFinished(const QString& reason = QString(), bool aborted = false);
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
    // Claude Generated 2026 - per-QGroupBox builders extracted from setupUI().
    // Each creates its group (populating the m_* members it owns) and returns it
    // for setupUI() to add to the scroll layout.
    QGroupBox* createPotentialGroup();
    QGroupBox* createReactEventsGroup();   // Claude Generated 2026 - reactive GFN-FF event log
    QGroupBox* createReactiveGroup();      // Claude Generated 2026 - reactive GFN-FF parameters
    /// Show/hide everything that only applies to a reactive GFN-FF MD run, and
    /// lock RATTLE while it is selected. Claude Generated 2026.
    void updateReactEventsVisibility();
    QGroupBox* createMdGroup();
    QGroupBox* createTempRampGroup();
    QGroupBox* createTempRegionGroup();
    QGroupBox* createRattleGroup();
    QGroupBox* createRmsdMtdGroup();
    QGroupBox* createWallGroup();
    QGroupBox* createOptGroup();
    QGroupBox* createOutputGroup();
    QGroupBox* createGrabGroup();
    /// Stop and join the worker thread, without ever leaving a running QThread to
    /// be destroyed. Claude Generated 2026.
    void stopWorkerThread();
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
    QComboBox* m_methodCombo = nullptr;
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
    QComboBox* m_topologyModeCombo = nullptr;  // "auto", "constant" or "react"

    // --- Reactive GFN-FF parameters (Claude Generated 2026) ---
    QGroupBox* m_reactGroup = nullptr;
    QDoubleSpinBox* m_reactFormSpin = nullptr;
    QDoubleSpinBox* m_reactBreakSpin = nullptr;
    QSpinBox* m_reactCheckEverySpin = nullptr;
    QSpinBox* m_reactRefractorySpin = nullptr;
    QCheckBox* m_reactValenceCapCheck = nullptr;
    QSpinBox* m_reactExchangeSpin = nullptr;
    QWidget* m_topologyLabel = nullptr;   // label of the Topology row (visibility)

    // --- Reactive GFN-FF event log (Claude Generated 2026) ---
    QGroupBox* m_reactEventsGroup = nullptr;
    QTableWidget* m_reactEventTable = nullptr;
    QCheckBox* m_snapshotOnEventCheck = nullptr;

    // --- Optimization parameters ---
    QDoubleSpinBox* m_convergenceSpin = nullptr;
    QDoubleSpinBox* m_energyConvergenceSpin = nullptr;   // Claude Generated 2026
    QCheckBox* m_optKeepParamsCheck = nullptr;  // Claude Generated 2026 - keep FF params across interactive Opt restarts

    // --- MD/Opt specific groups (shown/hidden based on mode) ---
    QGroupBox*      m_mdGroup = nullptr;       // MD parameters
    QGroupBox*      m_rattleGroup = nullptr;   // RATTLE constraints
    QGroupBox*      m_optGroup = nullptr;      // Optimization parameters
    QWidget*        m_rattleDetails = nullptr;  // hidden when mode=off
    QComboBox*      m_rattleCombo = nullptr;
    QCheckBox*      m_rattle12Check = nullptr;
    QCheckBox*      m_rattle13Check = nullptr;
    QDoubleSpinBox* m_rattleTol12Spin = nullptr;
    QDoubleSpinBox* m_rattleTol13Spin = nullptr;
    QSpinBox*       m_rattleMaxIterSpin = nullptr;

    // --- RMSD metadynamics (MD bias, curcuma SimpleMD rmsd_mtd) ---
    QGroupBox*      m_rmsdMtdGroup = nullptr;
    QCheckBox*      m_rmsdMtdEnableCheck = nullptr;
    QWidget*        m_rmsdMtdDetails = nullptr;
    QDoubleSpinBox* m_rmsdMtdKSpin = nullptr;
    QDoubleSpinBox* m_rmsdMtdAlphaSpin = nullptr;
    QLineEdit*      m_rmsdMtdAtomsEdit = nullptr;
    QLineEdit*      m_rmsdMtdRefFileEdit = nullptr;
    QSpinBox*       m_rmsdMtdMaxGaussiansSpin = nullptr;
    QSpinBox*       m_rmsdMtdMaxHeightSpin = nullptr;
    QDoubleSpinBox* m_rmsdMtdEconvSpin = nullptr;
    QSpinBox*       m_rmsdMtdPaceSpin = nullptr;
    QCheckBox*      m_rmsdMtdWtmtdCheck = nullptr;
    QDoubleSpinBox* m_rmsdMtdDtSpin = nullptr;
    QCheckBox*      m_rmsdMtdFreezeCheck = nullptr;

    // --- Confinement walls (curcuma SimpleMD wall_* params) ---
    QGroupBox*       m_wallGroup = nullptr;
    QCheckBox*       m_wallEnableCheck = nullptr;
    QWidget*         m_wallDetails = nullptr;
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
    QGroupBox*    m_tempRampGroup = nullptr;
    QCheckBox*    m_tempRampEnableCheck = nullptr;
    QWidget*      m_tempRampDetails = nullptr;
    QTableWidget* m_tempRampTable = nullptr;     // columns: Target(K) | Mode | Value
    QGroupBox*    m_tempRegionGroup = nullptr;
    QTableWidget* m_tempRegionTable = nullptr;   // columns: Atoms | Start T(K) | Schedule
    QLabel*       m_tempOverrideLabel = nullptr; // "ramp overridden" badge after a live drag

    // --- Interactive grab ---
    QDoubleSpinBox* m_grabStrengthSpin = nullptr;
    QDoubleSpinBox* m_grabAlphaSpin = nullptr;
    QSpinBox* m_grabMaxShellsSpin = nullptr;
    QComboBox* m_grabPresetCombo = nullptr;
    QCheckBox* m_grabAdvancedCheck = nullptr;
    QWidget* m_grabAdvancedWidget = nullptr;

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
    LiveState m_liveState;                  ///< Claude Generated 2026
    void publishLiveState();                ///< refresh m_liveState and emit
    // Claude Generated 2026 - throttle for the Step button: re-enabled after 1000/fpsLimit ms
    // so the user can click at the configured "max XXX FPS" but not faster.
    QElapsedTimer m_stepThrottleTimer;
};
