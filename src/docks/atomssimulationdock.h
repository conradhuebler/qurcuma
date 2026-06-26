// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// AtomsSimulationDock — right-side dock with internal tabs for the atom list,
// interactive simulation/optimization controls, and snapshot history.
//
// Claude Generated 2026 - Dock system restructuring.

#pragma once

#include "dockconfig.h"

#include <QDockWidget>

class AtomListPanel;
class SimulationControlWidget;
class SnapshotsWidget;
class QTabWidget;

class AtomsSimulationDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit AtomsSimulationDock(QWidget* parent = nullptr);

    QTabWidget* tabs() const;
    AtomListPanel* atomListPanel() const;
    SimulationControlWidget* simulationControlWidget() const;
    SnapshotsWidget* snapshotsWidget() const;

    void setCurrentTab(int index);

private:
    void setupUI();

    QTabWidget* m_tabs = nullptr;
    AtomListPanel* m_atomListPanel = nullptr;
    SimulationControlWidget* m_simulationControlWidget = nullptr;
    SnapshotsWidget* m_snapshotsWidget = nullptr;
};
