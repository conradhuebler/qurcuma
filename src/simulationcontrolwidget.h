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
#include <QObject>
#include <QPushButton>
#include <QSpinBox>
#include <QThread>
#include <QWidget>

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

    /** @brief Grab strength (world Å/Bohr per screen pixel) for the viewer. */
    double grabStrength() const { return m_grabStrengthSpin ? m_grabStrengthSpin->value() : 0.01; }
    double grabAlpha() const { return m_grabAlphaSpin ? m_grabAlphaSpin->value() : 0.4; }
    int grabMaxShells() const { return m_grabMaxShellsSpin ? m_grabMaxShellsSpin->value() : 3; }

signals:
    void simulationFinished();
    void configChanged(SimulationConfig);
    void simulationRunningChanged(bool running);
    void workerStarted(SimulationWorker* worker);

    /** @brief Emitted whenever grab sliders change, so the viewer can update
     *  its live scaling. */
    void grabSettingsChanged(double strength, double alpha, int maxShells);

private slots:
    void onStartClicked();
    void onPauseClicked();
    void onStopClicked();
    void onFrameReady(SimulationFramePtr frame);
    void onSimulationFinished();
    void onModeChanged(int index);

private:
    void setupUI();
    void setRunning(bool running);
    SimulationConfig buildConfig() const;

    // --- Mode / method ---
    QComboBox* m_modeCombo = nullptr;
    QComboBox* m_methodCombo = nullptr;
    QComboBox* m_optimizerCombo = nullptr;  // Claude Generated 2026 - opt algorithm picker

    // --- MD parameters ---
    QDoubleSpinBox* m_tempSpin = nullptr;
    QDoubleSpinBox* m_timestepSpin = nullptr;
    QSpinBox* m_stepsSpin = nullptr;
    QSpinBox* m_fpsLimitSpin = nullptr;
    QComboBox* m_gpuCombo = nullptr;
    QCheckBox* m_writeTrjCheck = nullptr;
    QCheckBox* m_perfCheck = nullptr;

    // --- Optimization parameters ---
    QDoubleSpinBox* m_convergenceSpin = nullptr;

    // --- RATTLE constraints (MD only) ---
    QGroupBox*      m_rattleGroup = nullptr;
    QWidget*        m_rattleDetails = nullptr;  // hidden when mode=off
    QComboBox*      m_rattleCombo = nullptr;
    QCheckBox*      m_rattle12Check = nullptr;
    QCheckBox*      m_rattle13Check = nullptr;
    QDoubleSpinBox* m_rattleTol12Spin = nullptr;
    QDoubleSpinBox* m_rattleTol13Spin = nullptr;
    QSpinBox*       m_rattleMaxIterSpin = nullptr;

    // --- Interactive grab ---
    QDoubleSpinBox* m_grabStrengthSpin = nullptr;
    QDoubleSpinBox* m_grabAlphaSpin = nullptr;
    QSpinBox* m_grabMaxShellsSpin = nullptr;

    // --- Buttons / status ---
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_pauseBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QLabel* m_statusLabel = nullptr;

    // --- State ---
    QElapsedTimer m_fpsTimer;
    int m_frameCount = 0;
    double m_actualFps = 0.0;
    QVector<MoleculeViewer::Atom> m_atoms;
    QVector<MoleculeViewer::Bond> m_bonds;
    SimulationConfig m_config;
    SimulationWorker* m_worker = nullptr;
    QThread* m_thread = nullptr;
    bool m_paused = false;
};
