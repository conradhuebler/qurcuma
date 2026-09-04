// simulationchart.cpp - Live time-series charts for an interactive MD/Opt run
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026.

#include "simulationchart.h"

#include <QtCharts>

#include "CuteChart/src/charts.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

#include <cmath>

namespace {

// Distinguishable colours for tracked measurements, cycled.
const QVector<QColor>& trackColours()
{
    static const QVector<QColor> c {
        QColor(31, 119, 180), QColor(255, 127, 14), QColor(44, 160, 44),
        QColor(214, 39, 40), QColor(148, 103, 189), QColor(140, 86, 75),
        QColor(227, 119, 194), QColor(23, 190, 207)
    };
    return c;
}

} // namespace

SimulationChartWidget::SimulationChartWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);

    lay->addWidget(buildControlBar());

    // One chart per tab: stacking them left each plot a thin strip, and in a dock
    // the vertical space is shared with everything else. No chart titles either —
    // the tab already names the chart and the axes carry the units, so a title
    // would only eat height. Claude Generated 2026.
    auto* tabs = new QTabWidget(this);

    m_tempChart = new ListChart;
    m_tempChart->setXAxis(tr("t [ps]"));
    m_tempChart->setYAxis(tr("T [K]"));
    m_tempChart->setAnimationOptions(QChart::NoAnimation);
    m_tempChart->chart()->setZoomStrategy(ZoomStrategy::Rectangular);
    tabs->addTab(m_tempChart, tr("Temperature"));

    m_tSeries = new QLineSeries;
    m_tTargetSeries = new QLineSeries;
    m_tempChart->addSeries(m_tSeries, 0, QColor(220, 50, 40), tr("T"), false);
    m_tempChart->addSeries(m_tTargetSeries, 1, QColor(40, 90, 220), tr("T target"), false);

    m_energyChart = new ListChart;
    m_energyChart->setXAxis(tr("t [ps]"));
    m_energyChart->setYAxis(tr("E [Eh]"));
    m_energyChart->setAnimationOptions(QChart::NoAnimation);
    m_energyChart->chart()->setZoomStrategy(ZoomStrategy::Rectangular);
    tabs->addTab(m_energyChart, tr("Energy"));

    m_epotSeries = new QLineSeries;
    m_ekinSeries = new QLineSeries;
    m_etotSeries = new QLineSeries;
    m_energyChart->addSeries(m_epotSeries, 0, QColor(40, 140, 60), tr("E_pot"), false);
    m_energyChart->addSeries(m_ekinSeries, 1, QColor(220, 140, 0), tr("E_kin"), false);
    m_energyChart->addSeries(m_etotSeries, 2, QColor(120, 60, 180), tr("E_tot"), false);

    // Reaction events (reactive GFN-FF): one marker per topology rebuild at (t, E_pot).
    m_eventSeries = new QScatterSeries;
    m_eventSeries->setMarkerSize(9.0);
    m_energyChart->addSeries(m_eventSeries, 3, QColor(230, 40, 40), tr("events"), false);

    m_measureChart = new ListChart;
    m_measureChart->setXAxis(tr("t [ps]"));
    m_measureChart->setYAxis(tr("value"));
    m_measureChart->setAnimationOptions(QChart::NoAnimation);
    m_measureChart->chart()->setZoomStrategy(ZoomStrategy::Rectangular);
    tabs->addTab(m_measureChart, tr("Measurements"));

    // Drawn as a step outline through a line series rather than a bar series:
    // CuteChart's ChartView assumes a QValueAxis on every series it is given, and a
    // bar series brings a QBarCategoryAxis, which would dereference a null axis.
    m_histogramChart = new ListChart;
    m_histogramChart->setXAxis(tr("value"));
    m_histogramChart->setYAxis(tr("count"));
    m_histogramChart->setAnimationOptions(QChart::NoAnimation);
    m_histogramChart->chart()->setZoomStrategy(ZoomStrategy::Rectangular);
    tabs->addTab(m_histogramChart, tr("Histogram"));

    lay->addWidget(tabs, 1);

    lay->addWidget(buildTrackTable());

    m_rescaleThrottle.start();
    updateUnitLabel();
}

QWidget* SimulationChartWidget::buildControlBar()
{
    auto* bar = new QWidget(this);
    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(2, 2, 2, 2);

    row->addWidget(new QLabel(tr("View:"), bar));
    m_modeCombo = new QComboBox(bar);
    m_modeCombo->addItem(tr("Sliding window"), 0);
    m_modeCombo->addItem(tr("Accumulated"), 1);
    m_modeCombo->setCurrentIndex(1);
    m_modeCombo->setToolTip(tr("Sliding window shows only the last stretch of the run and bins "
                               "only those samples into the histogram. Accumulated uses everything "
                               "recorded so far."));
    row->addWidget(m_modeCombo);

    m_windowSpin = new QDoubleSpinBox(bar);
    m_windowSpin->setRange(0.01, 10000.0);
    m_windowSpin->setDecimals(2);
    m_windowSpin->setValue(5.0);
    m_windowSpin->setSuffix(tr(" ps"));
    m_windowSpin->setToolTip(tr("Length of the sliding window."));
    row->addWidget(m_windowSpin);

    m_normaliseCheck = new QCheckBox(tr("Normalise"), bar);
    m_normaliseCheck->setToolTip(tr("Scale every tracked measurement to 0…1 over the shown range, "
                                    "so quantities with different units (Angstrom and degrees) can "
                                    "be compared in one plot. The exported CSV always holds the raw "
                                    "values."));
    row->addWidget(m_normaliseCheck);

    row->addWidget(new QLabel(tr("Bins:"), bar));
    m_binSpin = new QSpinBox(bar);
    m_binSpin->setRange(2, 500);
    m_binSpin->setValue(40);
    m_binSpin->setToolTip(tr("Number of histogram bins."));
    row->addWidget(m_binSpin);

    row->addStretch();

    auto* csvBtn = new QPushButton(tr("Export CSV…"), bar);
    csvBtn->setToolTip(tr("Write time, energies, temperature and every tracked measurement "
                          "as raw values."));
    connect(csvBtn, &QPushButton::clicked, this, &SimulationChartWidget::exportCsv);
    row->addWidget(csvBtn);

    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &SimulationChartWidget::refreshViews);
    connect(m_windowSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &SimulationChartWidget::refreshViews);
    connect(m_normaliseCheck, &QCheckBox::toggled, this, &SimulationChartWidget::refreshViews);
    connect(m_binSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &SimulationChartWidget::refreshViews);

    return bar;
}

QWidget* SimulationChartWidget::buildTrackTable()
{
    auto* box = new QWidget(this);
    auto* lay = new QVBoxLayout(box);
    lay->setContentsMargins(2, 2, 2, 2);
    lay->setSpacing(2);

    m_trackTable = new QTableWidget(0, 3, box);
    m_trackTable->setHorizontalHeaderLabels({ tr("Measurement"), tr("Type"), tr("Atoms") });
    m_trackTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_trackTable->verticalHeader()->setVisible(false);
    m_trackTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_trackTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_trackTable->setMaximumHeight(120);
    lay->addWidget(m_trackTable);

    auto* row = new QHBoxLayout;
    m_addBtn = new QPushButton(tr("Add from selection"), box);
    m_addBtn->setToolTip(tr("Track the atoms currently selected in the viewer: "
                            "2 give a distance, 3 an angle, 4 a dihedral."));
    connect(m_addBtn, &QPushButton::clicked, this, &SimulationChartWidget::addFromSelection);
    row->addWidget(m_addBtn);

    auto* removeBtn = new QPushButton(tr("Remove"), box);
    connect(removeBtn, &QPushButton::clicked, this, &SimulationChartWidget::removeSelectedRow);
    row->addWidget(removeBtn);

    m_unitLabel = new QLabel(box);
    m_unitLabel->setStyleSheet(QStringLiteral("QLabel { color: palette(mid); }"));
    row->addWidget(m_unitLabel);
    row->addStretch();
    lay->addLayout(row);

    return box;
}

void SimulationChartWidget::setTimestepFs(double fs)
{
    m_timestepFs = fs;
    const QString axis = (fs > 0.0) ? tr("t [ps]") : tr("step");
    for (ListChart* c : { m_tempChart, m_energyChart, m_measureChart })
        if (c)
            c->setXAxis(axis);
    m_windowSpin->setSuffix(fs > 0.0 ? tr(" ps") : tr(" steps"));
}

void SimulationChartWidget::setElements(const QVector<QString>& elements)
{
    m_elements = elements;
}

void SimulationChartWidget::setSelection(const QVector<int>& atoms)
{
    m_selection = atoms;
    measure::Tracked::Kind kind;
    const bool usable = measure::Tracked::kindForCount(atoms.size(), kind);
    if (m_addBtn) {
        m_addBtn->setEnabled(usable);
        m_addBtn->setText(usable
                ? tr("Add %1 from selection").arg(
                      kind == measure::Tracked::Kind::Distance ? tr("distance")
                          : kind == measure::Tracked::Kind::Angle ? tr("angle") : tr("dihedral"))
                : tr("Add from selection"));
    }
}

void SimulationChartWidget::addFromSelection()
{
    measure::Tracked::Kind kind;
    if (!measure::Tracked::kindForCount(m_selection.size(), kind)) {
        QMessageBox::information(this, tr("Add measurement"),
            tr("Select 2, 3 or 4 atoms in the viewer first: 2 give a distance, 3 an angle, "
               "4 a dihedral. Currently selected: %1.").arg(m_selection.size()));
        return;
    }

    Track t;
    t.def.kind = kind;
    t.def.atoms = m_selection;
    t.def.label = measure::Tracked::makeLabel(m_selection, m_elements);
    t.def.colour = trackColours()[m_tracks.size() % trackColours().size()];

    // Backfill from the frames already recorded is not possible (their positions are
    // not kept), so the new track is padded with NaN for the frames it missed and
    // stays parallel to m_timePs.
    t.values = QVector<double>(m_timePs.size(), std::numeric_limits<double>::quiet_NaN());

    m_tracks.append(t);
    rebuildMeasurementSeries();
    rebuildTrackTable();
    updateUnitLabel();
    refreshViews();
}

void SimulationChartWidget::removeSelectedRow()
{
    const int row = m_trackTable->currentRow();
    if (row < 0 || row >= m_tracks.size())
        return;
    m_tracks.remove(row);
    rebuildMeasurementSeries();
    rebuildTrackTable();
    updateUnitLabel();
    refreshViews();
}

void SimulationChartWidget::rebuildMeasurementSeries()
{
    // ListChart::clear() routes to QChart::removeAllSeries(), which DELETES the
    // series, so the stored pointers must be replaced rather than freed here.
    m_measureChart->clear();
    m_histogramChart->clear();
    for (int i = 0; i < m_tracks.size(); ++i) {
        m_tracks[i].series = new QLineSeries;
        m_measureChart->addSeries(m_tracks[i].series, i, m_tracks[i].def.colour,
            m_tracks[i].def.label, false);
        m_tracks[i].histogram = new QLineSeries;
        m_histogramChart->addSeries(m_tracks[i].histogram, i, m_tracks[i].def.colour,
            m_tracks[i].def.label, false);
    }
}

void SimulationChartWidget::rebuildTrackTable()
{
    m_trackTable->setRowCount(0);
    for (const Track& t : m_tracks) {
        const int r = m_trackTable->rowCount();
        m_trackTable->insertRow(r);
        auto* nameItem = new QTableWidgetItem(t.def.label);
        nameItem->setForeground(t.def.colour);
        m_trackTable->setItem(r, 0, nameItem);
        const QString type = t.def.kind == measure::Tracked::Kind::Distance
            ? tr("distance [A]")
            : t.def.kind == measure::Tracked::Kind::Angle ? tr("angle [deg]") : tr("dihedral [deg]");
        m_trackTable->setItem(r, 1, new QTableWidgetItem(type));
        QStringList idx;
        for (int a : t.def.atoms)
            idx << QString::number(a + 1);
        m_trackTable->setItem(r, 2, new QTableWidgetItem(idx.join(QStringLiteral(", "))));
    }
}

void SimulationChartWidget::updateUnitLabel()
{
    if (!m_unitLabel)
        return;
    bool haveDistance = false, haveAngle = false;
    for (const Track& t : m_tracks) {
        if (t.def.kind == measure::Tracked::Kind::Distance)
            haveDistance = true;
        else
            haveAngle = true;
    }
    if (haveDistance && haveAngle && !m_normaliseCheck->isChecked())
        m_unitLabel->setText(tr("mixed units on one axis — switch on Normalise to compare"));
    else
        m_unitLabel->clear();
    if (m_measureChart)
        m_measureChart->setYAxis(m_normaliseCheck->isChecked()
                ? tr("normalised (0…1)")
                : (haveDistance && !haveAngle) ? tr("d [A]")
                                               : (haveAngle && !haveDistance) ? tr("angle [deg]") : tr("value"));
}

void SimulationChartWidget::reset()
{
    m_timePs.clear();
    m_epot.clear();
    m_ekin.clear();
    m_etot.clear();
    m_temp.clear();
    m_tempTarget.clear();
    for (Track& t : m_tracks)
        t.values.clear();
    if (m_eventSeries)
        m_eventSeries->clear();
    m_rescaleThrottle.restart();
    refreshViews();
}

void SimulationChartWidget::appendFrame(SimulationFramePtr frame)
{
    if (!frame)
        return;

    const double x = (m_timestepFs > 0.0)
        ? frame->step * m_timestepFs / 1000.0    // fs -> ps
        : double(frame->step);

    m_timePs.append(x);
    m_epot.append(frame->energy);
    m_ekin.append(frame->ekin);
    m_etot.append(frame->energy + frame->ekin);
    m_temp.append(frame->temperature);
    m_tempTarget.append(frame->targetTemperature);

    for (Track& t : m_tracks)
        t.values.append(t.def.evaluate(frame->positions));

    // Rolling cap: drop the oldest samples so a long run does not grow without bound.
    if (m_timePs.size() > m_maxPoints) {
        const int drop = m_timePs.size() - m_maxPoints;
        auto trim = [drop](QVector<double>& v) { v.remove(0, qMin(drop, v.size())); };
        trim(m_timePs);
        trim(m_epot);
        trim(m_ekin);
        trim(m_etot);
        trim(m_temp);
        trim(m_tempTarget);
        for (Track& t : m_tracks)
            trim(t.values);
    }

    // Reaction events keep their own series: they are points, not a time series.
    if (!frame->events.isEmpty() && m_eventSeries) {
        for (int k = 0; k < frame->events.size(); ++k)
            m_eventSeries->append(x, frame->energy);
    }

    // Rebuilding every series on every frame would be wasteful; the throttle keeps
    // it at ~8 Hz while the samples above are recorded in full.
    if (m_rescaleThrottle.elapsed() >= 120) {
        refreshViews();
        m_rescaleThrottle.restart();
    }
}

int SimulationChartWidget::windowStart() const
{
    if (m_timePs.isEmpty() || m_modeCombo->currentData().toInt() == 1)
        return 0;   // accumulated
    const double cutoff = m_timePs.last() - m_windowSpin->value();
    int i = 0;
    while (i < m_timePs.size() && m_timePs[i] < cutoff)
        ++i;
    return i;
}

void SimulationChartWidget::refreshViews()
{
    const int start = windowStart();
    const int n = m_timePs.size();

    auto fill = [&](QLineSeries* s, const QVector<double>& v) {
        if (!s)
            return;
        QVector<QPointF> pts;
        pts.reserve(n - start);
        for (int i = start; i < n && i < v.size(); ++i)
            if (std::isfinite(v[i]))
                pts.append(QPointF(m_timePs[i], v[i]));
        s->replace(pts);
    };

    fill(m_epotSeries, m_epot);
    fill(m_ekinSeries, m_ekin);
    fill(m_etotSeries, m_etot);

    // Temperature is MD-only; the optimiser leaves both at zero.
    bool hasTemperature = false;
    for (int i = start; i < n; ++i)
        if (m_temp[i] > 0.0 || m_tempTarget[i] > 0.0) {
            hasTemperature = true;
            break;
        }
    if (hasTemperature) {
        fill(m_tSeries, m_temp);
        fill(m_tTargetSeries, m_tempTarget);
    }

    const bool normalise = m_normaliseCheck->isChecked();
    for (Track& t : m_tracks) {
        if (!t.series)
            continue;
        // Normalisation is over the SHOWN range, so a sliding window keeps using
        // the full height of the plot as the run develops.
        double lo = std::numeric_limits<double>::max();
        double hi = -std::numeric_limits<double>::max();
        if (normalise) {
            for (int i = start; i < n && i < t.values.size(); ++i)
                if (std::isfinite(t.values[i])) {
                    lo = std::min(lo, t.values[i]);
                    hi = std::max(hi, t.values[i]);
                }
        }
        const bool scalable = normalise && hi > lo;

        QVector<QPointF> pts;
        pts.reserve(n - start);
        QVector<double> windowValues;
        for (int i = start; i < n && i < t.values.size(); ++i) {
            const double v = t.values[i];
            if (!std::isfinite(v))
                continue;
            windowValues.append(v);
            pts.append(QPointF(m_timePs[i], scalable ? (v - lo) / (hi - lo) : v));
        }
        t.series->replace(pts);

        // Histogram of exactly the samples the time series shows.
        if (t.histogram) {
            const measure::Histogram h = measure::histogram(windowValues, m_binSpin->value());
            QVector<QPointF> steps;
            for (int b = 0; b < h.counts.size(); ++b) {
                const double left = h.min + b * h.binWidth;
                const double right = left + h.binWidth;
                steps.append(QPointF(left, h.counts[b]));
                steps.append(QPointF(right, h.counts[b]));
            }
            t.histogram->replace(steps);
        }
    }

    updateUnitLabel();
    if (hasTemperature && m_tempChart)
        m_tempChart->chart()->formatAxis();
    if (m_energyChart)
        m_energyChart->chart()->formatAxis();
    if (m_measureChart && !m_tracks.isEmpty())
        m_measureChart->chart()->formatAxis();
    if (m_histogramChart && !m_tracks.isEmpty())
        m_histogramChart->chart()->formatAxis();
}

void SimulationChartWidget::exportCsv()
{
    if (m_timePs.isEmpty()) {
        QMessageBox::information(this, tr("Export CSV"), tr("Nothing recorded yet."));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Export chart data"),
        QStringLiteral("simulation.csv"), tr("CSV files (*.csv)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export CSV"), tr("Could not write %1.").arg(path));
        return;
    }
    QTextStream out(&file);

    // Raw values over the whole recorded range, independent of the view mode and of
    // the normalisation toggle: an exported number is a measured one.
    out << (m_timestepFs > 0.0 ? "t_ps" : "step")
        << ",E_pot_Eh,E_kin_Eh,E_tot_Eh,T_K,T_target_K";
    for (const Track& t : m_tracks)
        out << ',' << t.def.label << '_' << t.def.unit();
    out << '\n';

    for (int i = 0; i < m_timePs.size(); ++i) {
        out << m_timePs[i] << ',' << m_epot[i] << ',' << m_ekin[i] << ',' << m_etot[i]
            << ',' << m_temp[i] << ',' << m_tempTarget[i];
        for (const Track& t : m_tracks) {
            out << ',';
            if (i < t.values.size() && std::isfinite(t.values[i]))
                out << t.values[i];
        }
        out << '\n';
    }
    file.close();
}
