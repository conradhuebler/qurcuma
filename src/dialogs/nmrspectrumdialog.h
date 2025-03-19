// nmrspectrumdialog.h
#pragma once

#include <QDialog>
#include <QListWidget>
#include <QtCharts>

#include "CuteChart/src/charts.h"

#include <map>
#include <memory>
#include <vector>

class QSPinBox;
class QDoubleSpinBox;

struct NMRData {
    QString name;
    QString filename;
    double energy;
    // element, nucleus_index, shielding, anisotropy
    std::map<QString, std::vector<std::tuple<int, double, double>>> shieldings;
    std::map<QString, double> reference;
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
    QTreeWidget* m_structureTree; // Ersetzt m_fileList
    QLineEdit* m_referenceEdit;
    QPushButton* m_referenceButton;
    QPushButton* m_addStructureButton;

    // Tree helper functions
    QTreeWidgetItem* addStructureToTree(const NMRData& data);
    QTreeWidgetItem* findOrCreateCompoundGroup(const QString& baseName);
    void updateTreeVisibility(QTreeWidgetItem* item, int column);
    void clearData();

    // QChartView* m_chartView;
    QTableWidget* m_shiftTable;
    ListChart* m_chart;
    ChartView* m_chartView;
    QSpinBox* m_maxPoints;
    QDoubleSpinBox* m_lineWidthBox;
    // Data members
    std::vector<std::unique_ptr<NMRData>> m_structures;
    NMRData m_reference;

    // Helper functions
    bool isConformation(const NMRData& str1, const NMRData& str2);
    NMRData parseOrcaOutput(const QString& filename, const QString& name, bool isRef);
    bool areStructuresCompatible(const NMRData& str1, const NMRData& str2);
    double extractEnergy(const QString& content);
    std::vector<double> calculateBoltzmannWeights(const std::vector<double>& energies);
    void updatePlot(const std::map<QString, std::vector<double>>& elementShifts, bool showSticks = true);
    void updateTable(const std::vector<ShiftData>& elementShifts);
    // QLineSeries* createGaussianPeak(double shift, double intensity,
    //     double xMin, double xMax, int points);
    void setupTable();
    void updatePlotBoth(const std::map<QString, std::vector<double>>& elementShifts);
    // Constants
    const double m_temperature = 298.15; // K
    const double m_kBoltzmann = 3.166811563e-6; // kcal/(mol·K)
    double m_lineWidth = 0.1; // ppm
    int m_plotPoints = 100000;
};
