// simulationchart.h - Live time-series charts for an interactive MD/Opt run
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - plots temperature (instantaneous + setpoint), energies
// (potential / kinetic / total) and any number of tracked geometric measurements
// (distances, angles, dihedrals) against time, fed from
// SimulationWorker::frameReady.

#pragma once

#include "measurements.h"
#include "simulationframe.h"

#include <QElapsedTimer>
#include <QVector>
#include <QWidget>

class ListChart;        // CuteChart composite chart + series legend
class QLineSeries;      // QtCharts (global namespace in Qt6)
class QScatterSeries;
class QXYSeries;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QTableWidget;
class QLabel;
class QPushButton;

/**
 * @brief Live charts for the running simulation: temperature, energy, tracked geometry.
 *
 * Samples are kept in plain vectors and the displayed series are rebuilt from them,
 * so the view options (time window, normalisation, histogram binning) can change
 * retroactively without losing data. The raw samples are also what the CSV export
 * writes, so an exported number is the measured one and not a re-read of the plot.
 *
 * Two view modes, as the operator asked for: a sliding window over the last N
 * picoseconds, and the accumulated run. The mode decides both the visible time
 * range and which samples the histogram bins.
 *
 * Claude Generated 2026.
 */
class SimulationChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit SimulationChartWidget(QWidget* parent = nullptr);

    /** @brief MD time step in femtoseconds, so the time axis can read picoseconds.
     *  Set from the simulation config before a run; 0 keeps the axis in steps. */
    void setTimestepFs(double fs);

    /** @brief Element symbols of the current structure, used to label a new
     *  measurement ("N1-H4"). Set when the molecule changes. */
    void setElements(const QVector<QString>& elements);

public slots:
    /** @brief Append the frame's temperature, energies and tracked measurements. */
    void appendFrame(SimulationFramePtr frame);

    /** @brief Drop all samples (call at the start of a new run). */
    void reset();

    /** @brief Offer the viewer's current selection as a new tracked measurement.
     *  2 atoms become a distance, 3 an angle, 4 a dihedral; anything else is
     *  ignored (the Add button reports why). */
    void setSelection(const QVector<int>& atoms);

private slots:
    void addFromSelection();
    void removeSelectedRow();
    void exportCsv();
    void refreshViews();   ///< rebuild the displayed series from the stored samples

private:
    /** @brief One tracked quantity plus its samples and its plot series. */
    struct Track {
        measure::Tracked def;
        QVector<double> values;   ///< one per recorded frame, parallel to m_timePs
        QLineSeries* series = nullptr;
        QLineSeries* histogram = nullptr;
    };

    QWidget* buildControlBar();
    /// Recreate the measurement and histogram series from m_tracks. ListChart has no
    /// per-series removal, so both charts are cleared (which deletes their series)
    /// and refilled; that also keeps the legend in step with the table.
    void rebuildMeasurementSeries();
    QWidget* buildTrackTable();
    void rebuildTrackTable();
    /** @brief First sample index inside the current view (0 when accumulated). */
    int windowStart() const;
    void updateUnitLabel();

    // --- charts ---
    ListChart* m_tempChart = nullptr;
    ListChart* m_energyChart = nullptr;
    ListChart* m_measureChart = nullptr;
    ListChart* m_histogramChart = nullptr;
    QLineSeries* m_tSeries = nullptr;        // instantaneous temperature
    QLineSeries* m_tTargetSeries = nullptr;  // thermostat setpoint (tracks the ramp)
    QLineSeries* m_epotSeries = nullptr;
    QLineSeries* m_ekinSeries = nullptr;
    QLineSeries* m_etotSeries = nullptr;
    QScatterSeries* m_eventSeries = nullptr; // reaction events marked on E_pot

    // --- controls ---
    QComboBox* m_modeCombo = nullptr;
    QDoubleSpinBox* m_windowSpin = nullptr;
    QCheckBox* m_normaliseCheck = nullptr;
    QSpinBox* m_binSpin = nullptr;
    QTableWidget* m_trackTable = nullptr;
    QPushButton* m_addBtn = nullptr;
    QLabel* m_unitLabel = nullptr;

    // --- samples ---
    QVector<double> m_timePs;      ///< x value of every recorded frame
    QVector<double> m_epot, m_ekin, m_etot, m_temp, m_tempTarget;
    QVector<Track> m_tracks;
    QVector<int> m_selection;      ///< viewer selection offered to "Add"
    QVector<QString> m_elements;

    double m_timestepFs = 0.0;
    QElapsedTimer m_rescaleThrottle;
    int m_maxPoints = 20000;       ///< rolling sample cap (bounds memory over long runs)
};
