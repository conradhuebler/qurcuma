// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// ChartDock implementation.
//
// Claude Generated 2026.

#include "chartdock.h"

#include "../widgets/simulationchart.h"

ChartDock::ChartDock(QWidget* parent)
    : QDockWidget(DockConfig::ChartDockTitle, parent)
{
    setObjectName(DockConfig::ChartDockObjectName);
    // Bottom next to the output log, or right with the other panels — the plots
    // are wide rather than tall, so both make sense depending on the layout.
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);

    m_charts = new SimulationChartWidget(this);
    // Three stacked plots plus the control bar and the measurement table need room;
    // without a floor the bottom dock area would squeeze them to unreadable strips.
    // Well below the dialog's former 560 px, so it does not dominate the window.
    m_charts->setMinimumHeight(300);
    setWidget(m_charts);
}

void ChartDock::appendFrame(SimulationFramePtr frame)
{
    m_charts->appendFrame(frame);
}

void ChartDock::reset()
{
    m_charts->reset();
}

void ChartDock::setTimestepFs(double fs)
{
    m_charts->setTimestepFs(fs);
}

void ChartDock::setElements(const QVector<QString>& elements)
{
    m_charts->setElements(elements);
}

void ChartDock::setSelection(const QVector<int>& atoms)
{
    m_charts->setSelection(atoms);
}
