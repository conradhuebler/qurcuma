// simulationcontrolwidget.h - Compact inline simulation controls for the viewer dock
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Interactive Simulation Integration

#pragma once

#include "simulationworker.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QSpinBox>
#include <QThread>
#include <QWidget>

/**
 * @brief Compact widget for inline simulation control in the viewer dock.
 *
 * Claude Generated - Interactive Simulation Integration
 *
 * Provides quick-access controls for MD and geometry optimization directly
 * in the viewer panel. A "More..." button opens the full SimulationDialog.
 *
 * Layout:
 *   [Mode ▼] [Method ▼] [Temp] K  [Steps]  ▶ Start  ⏸  ■   ⚙ More...
 *   Step 0 | E= --- Eh | T= --- K
 */
class SimulationControlWidget : public QWidget {
    Q_OBJECT

public:
    explicit SimulationControlWidget(QWidget* parent = nullptr);
    ~SimulationControlWidget() override;

    /** @brief Feed a new molecule to the worker before the next run. */
    void setMolecule(const QVector<MoleculeViewer::Atom>& atoms) { m_atoms = atoms; }

    /** @brief Get the currently stored atoms (for passing to SimulationDialog). */
    const QVector<MoleculeViewer::Atom>& currentAtoms() const { return m_atoms; }

    /** @brief Return current UI state as SimulationConfig without starting. */
    SimulationConfig currentConfig() const { return buildConfig(); }

    /** @brief Populate UI controls from an existing config. Uses QSignalBlocker to avoid feedback. */
    void applyConfig(const SimulationConfig& cfg);

signals:
    /**
     * @brief Forwarded from SimulationWorker::frameReady.
     * Connected by MainWindow to MoleculeViewer for live position updates.
     */
    void frameReady(QVector<MoleculeViewer::Atom> atoms, double energy, double ekin, int step);

    /** @brief Opens the full SimulationDialog (handled by MainWindow). */
    void openFullDialog();

    /** @brief Simulation finished (for UI state reset). */
    void simulationFinished();

    /** @brief Emitted when any control value changes so MainWindow can sync shared config. */
    void configChanged(SimulationConfig);

private slots:
    void onStartClicked();
    void onPauseClicked();
    void onStopClicked();
    void onFrameReady(QVector<MoleculeViewer::Atom> atoms, double energy, double ekin, int step);
    void onSimulationFinished();
    void onModeChanged(int index);

private:
    void setupUI();
    void setRunning(bool running);
    SimulationConfig buildConfig() const;

    // Controls
    QComboBox* m_modeCombo = nullptr;
    QComboBox* m_methodCombo = nullptr;
    QDoubleSpinBox* m_tempSpin = nullptr;
    QSpinBox* m_stepsSpin = nullptr;
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_pauseBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QPushButton* m_moreBtn = nullptr;
    QLabel* m_statusLabel = nullptr;

    // Simulation state
    QVector<MoleculeViewer::Atom> m_atoms;
    SimulationConfig m_config;        // Cached at start for display purposes
    SimulationWorker* m_worker = nullptr;
    QThread* m_thread = nullptr;
    bool m_paused = false;
};
