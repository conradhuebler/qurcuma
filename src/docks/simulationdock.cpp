// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// SimulationDock implementation.
//
// Claude Generated 2026 - Dock system restructuring.

#include "simulationdock.h"

#include "modifiabletextedit.h"
#include "rmsdwidget.h"
#include "simulationcontrolwidget.h"
#include "snapshotswidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QVBoxLayout>

SimulationDock::SimulationDock(QWidget* parent)
    : QDockWidget(DockConfig::SimulationDockTitle, parent)
{
    setObjectName(DockConfig::SimulationDockObjectName);
    setupUI();
}

void SimulationDock::setupUI()
{
    m_tabs = new QTabWidget(this);
    m_tabs->setTabPosition(QTabWidget::North);
    m_tabs->setDocumentMode(true);

    m_simulationControlWidget = new SimulationControlWidget(this);
    m_tabs->addTab(m_simulationControlWidget, tr("Simulation"));

    m_snapshotsWidget = new SnapshotsWidget(this);
    m_tabs->addTab(m_snapshotsWidget, tr("Snapshots"));

    m_rmsdWidget = new RMSDWidget(this);
    m_tabs->addTab(m_rmsdWidget, tr("RMSD / Align"));

    QWidget* inputPage = new QWidget;
    QVBoxLayout* inputLayout = new QVBoxLayout(inputPage);
    inputLayout->setContentsMargins(4, 4, 4, 4);
    QHBoxLayout* inputFileLayout = new QHBoxLayout;
    inputFileLayout->addWidget(new QLabel(tr("Input file:")));
    m_inputFileEdit = new QLineEdit(tr("input"));
    m_inputFileEdit->setToolTip(tr("Base name for input file"));
    m_inputFileEditExtension = new QLineEdit;
    m_inputFileEditExtension->setMaximumWidth(60);
    inputFileLayout->addWidget(m_inputFileEdit);
    inputFileLayout->addWidget(m_inputFileEditExtension);
    inputLayout->addLayout(inputFileLayout);
    m_inputView = new ModifiableTextEdit;
    m_inputView->setPlaceholderText(tr("Input data"));
    inputLayout->addWidget(m_inputView);
    m_tabs->addTab(inputPage, tr("Input"));

    setWidget(m_tabs);
}

QTabWidget* SimulationDock::tabs() const { return m_tabs; }
SimulationControlWidget* SimulationDock::simulationControlWidget() const { return m_simulationControlWidget; }
SnapshotsWidget* SimulationDock::snapshotsWidget() const { return m_snapshotsWidget; }
RMSDWidget* SimulationDock::rmsdWidget() const { return m_rmsdWidget; }
ModifiableTextEdit* SimulationDock::inputView() const { return m_inputView; }
QLineEdit* SimulationDock::inputFileEdit() const { return m_inputFileEdit; }
QLineEdit* SimulationDock::inputFileEditExtension() const { return m_inputFileEditExtension; }

void SimulationDock::setCurrentTab(int index)
{
    if (m_tabs && index >= 0 && index < m_tabs->count())
        m_tabs->setCurrentIndex(index);
}
