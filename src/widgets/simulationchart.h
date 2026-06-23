// simulationchart.h - Live time-series charts for an interactive MD/Opt run
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - plots temperature (instantaneous + setpoint) and energies
// (potential / kinetic / total) against step, fed from SimulationWorker::frameReady.

#pragma once

#include "simulationframe.h"

#include <QElapsedTimer>
#include <QWidget>

class ListChart;        // CuteChart composite chart + series legend
class QLineSeries;      // QtCharts (global namespace in Qt6)

/**
 * @brief Two stacked live charts (Temperature, Energy) for the running simulation.
 *
 * Series are created once; appendFrame() pushes one point per emitted frame with a rolling
 * cap on the point count, and a throttled axis rescale keeps the view following the data.
 * Claude Generated 2026.
 */
class SimulationChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit SimulationChartWidget(QWidget* parent = nullptr);

public slots:
    /** @brief Append the frame's temperature + energies at frame->step. */
    void appendFrame(SimulationFramePtr frame);

    /** @brief Clear all series (call at the start of a new run). */
    void reset();

private:
    void capSeries(QLineSeries* s);

    ListChart* m_tempChart = nullptr;
    ListChart* m_energyChart = nullptr;
    QLineSeries* m_tSeries = nullptr;        // instantaneous temperature
    QLineSeries* m_tTargetSeries = nullptr;  // thermostat setpoint (tracks the ramp)
    QLineSeries* m_epotSeries = nullptr;
    QLineSeries* m_ekinSeries = nullptr;
    QLineSeries* m_etotSeries = nullptr;

    QElapsedTimer m_rescaleThrottle;
    int m_maxPoints = 2000;  // rolling window per series (bounds memory over long runs)
};
