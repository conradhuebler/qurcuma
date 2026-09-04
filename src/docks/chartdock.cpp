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
    // One plot at a time (each chart is its own tab) plus the control bar and the
    // measurement table; without a floor the bottom dock area would squeeze the
    // plot to an unreadable strip.
    m_charts->setMinimumHeight(220);
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
