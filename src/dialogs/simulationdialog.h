// simulationdialog.h - Full simulation configuration and monitoring dialog
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Interactive Simulation Integration

#pragma once

#include "../simulationworker.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QThread>

/**
 * @brief Full-featured dialog for configuring and monitoring molecular simulations.
 *
 * Claude Generated - Interactive Simulation Integration
 *
 * Provides complete control over MD and geometry optimization parameters.
 * Emits frameReady() for live viewer updates and logs simulation progress.
 * Can be opened from: Simulation menu or SimulationControlWidget "⚙ More..." button.
 */
class SimulationDialog : public QDialog {
    Q_OBJECT

public:
    explicit SimulationDialog(QWidget* parent = nullptr);
    ~SimulationDialog() override;

    /** @brief Provide the molecule to simulate (call before showing the dialog). */
    void setMolecule(const QVector<MoleculeViewer::Atom>& atoms);

    /** @brief Pre-select simulation mode (MD or Optimization). */
    void setMode(SimulationConfig::Mode mode);

    /** @brief Pre-populate all dialog controls from an existing config. */
    void setConfig(const SimulationConfig& cfg);

    /** @brief Return current dialog control state as SimulationConfig. */
    SimulationConfig currentConfig() const { return buildConfig(); }

signals:
    /**
     * @brief Forwarded from SimulationWorker::frameReady for live viewer updates.
     */
    void frameReady(QVector<MoleculeViewer::Atom> atoms, double energy, double ekin, int step);

private slots:
    void onStartClicked();
    void onPauseClicked();
    void onStopClicked();
    void onExportClicked();
    void onModeChanged(int index);
    void onFrameReady(QVector<MoleculeViewer::Atom> atoms, double energy, double ekin, int step);
    void onSimulationFinished();
    void onError(const QString& message);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUI();
    void setRunning(bool running);
    SimulationConfig buildConfig() const;

    // --- Connection group ---
    QComboBox* m_modeCombo = nullptr;
    QComboBox* m_methodCombo = nullptr;

    // --- MD group ---
    QGroupBox* m_mdGroup = nullptr;
    QDoubleSpinBox* m_tempSpin = nullptr;
    QDoubleSpinBox* m_timestepSpin = nullptr;
    QSpinBox* m_stepsSpin = nullptr;
    QSpinBox* m_dumpFreqSpin = nullptr;
    QSpinBox* m_fpsLimitSpin = nullptr;  // Claude Generated - FPS cap control
    QComboBox* m_gpuCombo = nullptr;     // Claude Generated - GPU acceleration selector
    QCheckBox* m_writeTrjCheck = nullptr;
    QCheckBox* m_perfCheck = nullptr;  // Claude Generated - Performance analysis toggle

    // --- Optimization group ---
    QGroupBox* m_optGroup = nullptr;
    QSpinBox* m_maxIterSpin = nullptr;
    QDoubleSpinBox* m_convergenceSpin = nullptr;

    // --- Live display ---
    QLabel* m_stepLabel = nullptr;
    QLabel* m_energyLabel = nullptr;
    QLabel* m_tempLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QTextEdit* m_logView = nullptr;

    // --- Buttons ---
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_pauseBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    // --- State ---
    QVector<MoleculeViewer::Atom> m_atoms;
    SimulationWorker* m_worker = nullptr;
    QThread* m_thread = nullptr;
    SimulationConfig m_activeConfig;
    bool m_paused = false;
    int m_frameCount = 0;
    QVector<QVector<MoleculeViewer::Atom>> m_trajectory; // For export
};
