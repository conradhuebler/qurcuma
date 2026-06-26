// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// AtomsSimulationDock implementation.
//
// Claude Generated 2026 - Dock system restructuring.

#include "atomssimulationdock.h"

#include "atomlistpanel.h"
#include "simulationcontrolwidget.h"
#include "snapshotswidget.h"

#include <QTabWidget>

AtomsSimulationDock::AtomsSimulationDock(QWidget* parent)
    : QDockWidget(DockConfig::AtomsSimulationDockTitle, parent)
{
    setObjectName(DockConfig::AtomsSimulationDockObjectName);
    setupUI();
}

void AtomsSimulationDock::setupUI()
{
    m_tabs = new QTabWidget(this);
    m_tabs->setTabPosition(QTabWidget::North);
    m_tabs->setDocumentMode(true);

    m_atomListPanel = new AtomListPanel(this);
    m_tabs->addTab(m_atomListPanel, tr("Atoms"));

    m_simulationControlWidget = new SimulationControlWidget(this);
    m_tabs->addTab(m_simulationControlWidget, tr("Simulation"));

    m_snapshotsWidget = new SnapshotsWidget(this);
    m_tabs->addTab(m_snapshotsWidget, tr("Snapshots"));

    setWidget(m_tabs);
}

QTabWidget* AtomsSimulationDock::tabs() const
{
    return m_tabs;
}

AtomListPanel* AtomsSimulationDock::atomListPanel() const
{
    return m_atomListPanel;
}

SimulationControlWidget* AtomsSimulationDock::simulationControlWidget() const
{
    return m_simulationControlWidget;
}

SnapshotsWidget* AtomsSimulationDock::snapshotsWidget() const
{
    return m_snapshotsWidget;
}

void AtomsSimulationDock::setCurrentTab(int index)
{
    if (m_tabs && index >= 0 && index < m_tabs->count())
        m_tabs->setCurrentIndex(index);
}
