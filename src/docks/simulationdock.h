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
class QLineEdit;
class RMSDWidget;
class SimulationControlWidget;
class SnapshotsWidget;
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

private:
    void setupUI();

    QTabWidget* m_tabs = nullptr;
    SimulationControlWidget* m_simulationControlWidget = nullptr;
    SnapshotsWidget* m_snapshotsWidget = nullptr;
    RMSDWidget* m_rmsdWidget = nullptr;

    ModifiableTextEdit* m_inputView = nullptr;
    QLineEdit* m_inputFileEdit = nullptr;
    QLineEdit* m_inputFileEditExtension = nullptr;
};
