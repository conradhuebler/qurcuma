// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// SimulationDock — right-side dock with internal tabs for the interactive
// simulation/optimization controls, snapshot history, RMSD / Align tool and
// input editor.
//
// Claude Generated 2026 - Dock system restructuring.

#pragma once

#include "dockconfig.h"

#include <QDockWidget>

class ModifiableTextEdit;
class MoleculeViewer;
class QCheckBox;
class QLabel;
class QLineEdit;
class QShowEvent;
class QSlider;
class QSpinBox;
class RMSDWidget;
class SimulationControlWidget;
class SnapshotsWidget;
class SimulationParametersWidget;
class QTabWidget;

class SimulationDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit SimulationDock(QWidget* parent = nullptr);

    QTabWidget* tabs() const;
    SimulationControlWidget* simulationControlWidget() const;
    SnapshotsWidget* snapshotsWidget() const;
    RMSDWidget* rmsdWidget() const;

    // Input editor, moved here from the former EditorsDock
    ModifiableTextEdit* inputView() const;
    QLineEdit* inputFileEdit() const;
    QLineEdit* inputFileEditExtension() const;

    void setCurrentTab(int index);

    /// Claude Generated 2026 - Place the viewer options of a simulation (SimulationViewOptions)
    /// in a collapsible "Show in viewer" section below the simulation controls.
    void setViewOptions(QWidget* options);

signals:
    // Claude Generated 2026 - Re-emitted SnapshotsWidget signals so MainWindow wires
    // the dock instead of the internal widget (same pattern as ProjectDock). Only the
    // snapshot signals are forwarded: the RMSD overlay signals carry MoleculeViewer
    // types (forwarding them would pull view.h into this header) and the simulation
    // control signals interleave with per-run worker wiring, so those stay connected
    // directly via the getters pending a deeper logic move.
    void takeSnapshotRequested();
    void restoreSnapshotRequested(int index);
    void deleteSnapshotRequested(int index);

private:
    void setupUI();

    QTabWidget* m_tabs = nullptr;
    SimulationControlWidget* m_simulationControlWidget = nullptr;
    SnapshotsWidget* m_snapshotsWidget = nullptr;
    RMSDWidget* m_rmsdWidget = nullptr;
    SimulationParametersWidget* m_parametersWidget = nullptr;  // "All parameters" tab

    ModifiableTextEdit* m_inputView = nullptr;
    QLineEdit* m_inputFileEdit = nullptr;
    QLineEdit* m_inputFileEditExtension = nullptr;
    class CollapsibleSection* m_viewOptionsSection = nullptr;
};

// Claude Generated 2026 - UX stage 4: what a simulation shows in the 3D view (confinement
// walls, wall potential shells and force field, grab force vectors, dynamic bonds). These
// were in the Display panel's former Tools section; they belong next to the simulation.
// Writes go straight to the viewer setters; syncFromViewer() re-reads on every show.
class SimulationViewOptions : public QWidget
{
    Q_OBJECT
public:
    explicit SimulationViewOptions(MoleculeViewer* viewer, QWidget* parent = nullptr);
    void syncFromViewer();

protected:
    void showEvent(QShowEvent* event) override;

private:
    MoleculeViewer* m_viewer = nullptr;
    QCheckBox* m_forceVectorsCheck = nullptr;
    QCheckBox* m_dynamicBondsCheck = nullptr;
    QCheckBox* m_wallCheck = nullptr;
    QSlider* m_wallOpacitySlider = nullptr;
    QLabel* m_wallOpacityLabel = nullptr;
    QCheckBox* m_potGradientCheck = nullptr;
    QCheckBox* m_potArrowCheck = nullptr;
    QSpinBox* m_potArrowResSpin = nullptr;
};
