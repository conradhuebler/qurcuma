// nmrspectrumdialog.h
#pragma once

#include <QDialog>
#include <QListWidget>
#include <QtCharts>

#include "CuteChart/src/charts.h"

#include <map>
#include <memory>
#include <vector>

struct NMRData {
    QString name;
    QString filename;
    double energy;
    // element, nucleus_index, shielding, anisotropy
    std::map<QString, std::vector<std::tuple<int, double, double>>> shieldings;
    bool isReference;
};
struct ShiftData {
    QString element;
    int nucleus;
    double referenceShielding;
    double shielding;
    double shift;
    double weight;
};
class NMRSpectrumDialog : public QDialog {
    Q_OBJECT

public:
    // Structure to hold NMR data

    explicit NMRSpectrumDialog(QWidget* parent = nullptr);
    ~NMRSpectrumDialog() = default;

    // Main interface functions
    void setReference(const QString& filename, const QString& name);
    void addStructure(const QString& filename, const QString& name);

private slots:
    void removeSelectedItem();
    void generateSpectrum();
    void exportData();
    void handleMarkerClicked();

private:
    // UI elements
    QListWidget* m_fileList;
    // QChartView* m_chartView;
    QTableWidget* m_shiftTable;
    ListChart* m_chart;
    ChartView* m_chartView;
    // Data members
    std::vector<std::unique_ptr<NMRData>> m_structures;

    // Helper functions
    NMRData parseOrcaOutput(const QString& filename, const QString& name, bool isRef);
    bool areStructuresCompatible(const NMRData& str1, const NMRData& str2);
    double extractEnergy(const QString& content);
    std::vector<double> calculateBoltzmannWeights(const std::vector<double>& energies);
    void updatePlot(const std::map<QString, std::vector<double>>& elementShifts);
    void updateTable(const std::vector<ShiftData>& elementShifts);
    QLineSeries* createGaussianPeak(double shift, double intensity,
        double xMin, double xMax, int points);
    void setupTable();

    // Constants
    const double m_temperature = 298.15; // K
    const double m_kBoltzmann = 0.0019872041; // kcal/(mol·K)
    const double m_lineWidth = 0.1; // ppm
    const int m_plotPoints = 1000;
};
