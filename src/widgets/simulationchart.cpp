// simulationchart.cpp - Live time-series charts for an interactive MD/Opt run
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026.

#include "simulationchart.h"

#include <QtCharts>

#include "CuteChart/src/charts.h"

#include <QVBoxLayout>

SimulationChartWidget::SimulationChartWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);

    // --- Temperature chart: instantaneous T + thermostat setpoint (tracks the ramp) ---
    m_tempChart = new ListChart;
    m_tempChart->setTitle(tr("Temperature"));
    m_tempChart->setXAxis(tr("step"));
    m_tempChart->setYAxis(tr("T [K]"));
    m_tempChart->setAnimationOptions(QChart::NoAnimation);
    m_tempChart->chart()->setZoomStrategy(ZoomStrategy::Rectangular);
    lay->addWidget(m_tempChart, 1);

    m_tSeries = new QLineSeries;
    m_tTargetSeries = new QLineSeries;
    m_tempChart->addSeries(m_tSeries, 0, QColor(220, 50, 40), tr("T"), false);
    m_tempChart->addSeries(m_tTargetSeries, 1, QColor(40, 90, 220), tr("T target"), false);

    // --- Energy chart: potential / kinetic / total (Hartree) ---
    m_energyChart = new ListChart;
    m_energyChart->setTitle(tr("Energy"));
    m_energyChart->setXAxis(tr("step"));
    m_energyChart->setYAxis(tr("E [Eh]"));
    m_energyChart->setAnimationOptions(QChart::NoAnimation);
    m_energyChart->chart()->setZoomStrategy(ZoomStrategy::Rectangular);
    lay->addWidget(m_energyChart, 1);

    m_epotSeries = new QLineSeries;
    m_ekinSeries = new QLineSeries;
    m_etotSeries = new QLineSeries;
    m_energyChart->addSeries(m_epotSeries, 0, QColor(40, 140, 60), tr("E_pot"), false);
    m_energyChart->addSeries(m_ekinSeries, 1, QColor(220, 140, 0), tr("E_kin"), false);
    m_energyChart->addSeries(m_etotSeries, 2, QColor(120, 60, 180), tr("E_tot"), false);

    m_rescaleThrottle.start();
}

void SimulationChartWidget::reset()
{
    for (QLineSeries* s : { m_tSeries, m_tTargetSeries, m_epotSeries, m_ekinSeries, m_etotSeries }) {
        if (s)
            s->clear();
    }
    m_rescaleThrottle.restart();
}

void SimulationChartWidget::appendFrame(SimulationFramePtr frame)
{
    if (!frame)
        return;

    const double x = static_cast<double>(frame->step);

    // Energies are always present (also for geometry optimisation, where ekin = 0).
    m_epotSeries->append(x, frame->energy);
    m_ekinSeries->append(x, frame->ekin);
    m_etotSeries->append(x, frame->energy + frame->ekin);
    capSeries(m_epotSeries);
    capSeries(m_ekinSeries);
    capSeries(m_etotSeries);

    // Temperature is MD-only (the optimiser leaves both at 0).
    const bool hasTemperature = frame->targetTemperature > 0.0 || frame->temperature > 0.0;
    if (hasTemperature) {
        m_tSeries->append(x, frame->temperature);
        m_tTargetSeries->append(x, frame->targetTemperature);
        capSeries(m_tSeries);
        capSeries(m_tTargetSeries);
    }

    // Rescaling the axes is the expensive part — throttle it to ~8 Hz; points are still
    // appended every frame so nothing is lost, only the view refresh is coalesced.
    if (m_rescaleThrottle.elapsed() >= 120) {
        if (hasTemperature)
            m_tempChart->chart()->formatAxis();
        m_energyChart->chart()->formatAxis();
        m_rescaleThrottle.restart();
    }
}

void SimulationChartWidget::capSeries(QLineSeries* s)
{
    const int over = s->count() - m_maxPoints;
    if (over > 0)
        s->removePoints(0, over);  // drop the oldest points (rolling window)
}
