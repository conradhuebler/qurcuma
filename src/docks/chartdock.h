// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// ChartDock — dock holding the live simulation charts (temperature, energy and
// any tracked distances, angles and dihedrals). Thin wrapper around
// SimulationChartWidget; the widget's own slots are forwarded so MainWindow wires
// to the dock and does not reach through to the inner widget.
//
// The charts used to live in a modeless dialog. As a dock they take part in the
// layout presets and in View > Dock Panels like every other panel. It starts
// hidden, because an empty plot would only take space before a run.
//
// Claude Generated 2026.

#pragma once

#include "dockconfig.h"

#include <QDockWidget>
#include <QVector>

#include "../simulationframe.h"

class SimulationChartWidget;

class ChartDock : public QDockWidget {
    Q_OBJECT

public:
    explicit ChartDock(QWidget* parent = nullptr);

    SimulationChartWidget* widget() const { return m_charts; }

public slots:
    void appendFrame(SimulationFramePtr frame);
    void reset();
    void setTimestepFs(double fs);
    void setElements(const QVector<QString>& elements);
    void setSelection(const QVector<int>& atoms);

private:
    SimulationChartWidget* m_charts = nullptr;
};
