// nmrspectrumdialog.h
#ifndef NMRSPECTRUMDIALOG_H
#define NMRSPECTRUMDIALOG_H

#include <QDialog>
#include <QModelIndex>
#include <map>
#include <memory>

// Debug-Makros zur einfachen Fehlersuche
#define NMR_DEBUG 1

#if NMR_DEBUG
#include <QDebug>
#define NMR_LOG(msg) qDebug() << "[NMRSpectrumDialog] " << msg
#else
#define NMR_LOG(msg)
#endif

class QLineEdit;
class QPushButton;
class QTreeView;
class QTableWidget;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QGroupBox;
class QCheckBox;
class QTimer;
class ListChart;
class QItemSelection;
class NMRDataStore;
class NMRController;
class NMRStructureProxyModel;

/**
 * Dialog for NMR spectrum analysis
 */
class NMRSpectrumDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * Constructor
     * @param parent Parent widget
     */
    explicit NMRSpectrumDialog(QWidget* parent = nullptr);

    /**
     * Adds a structure to the model
     * @param filename The filename of the structure
     * @param name The name of the structure
     */
    void addStructure(const QString& filename, const QString& name);
private slots:
    /**
     * Selects structure files to add
     */
    void selectStructureFiles();

    /**
     * Sets a structure as reference
     */
    void setAsReference();

    /**
     * Handles element filter changes
     */
    void elementFilterChanged(int state);

    /**
     * Sets the number of plot points
     */
    void setPlotPoints(int points);

    /**
     * Sets the line width
     */
    void setLineWidth(double width);

    /**
     * Generates the NMR spectrum
     */
    void generateSpectrum();

    /**
     * Exports data to CSV
     */
    void exportData();

    /**
     * Clears all data
     */
    void clearData();

    /**
     * Handles selection change in the tree view
     */
    // void handleSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
    void handleSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    /**
     * Handles data change in the model
     */
    void handleDataChanged();

    /**
     * Handles spectrum generation completion
     */
    void handleSpectrumGenerated();

    /**
     * Handles spectrum generation failure
     */
    void handleSpectrumGenerationFailed(const QString& message);

private:
    // UI components
    QPushButton* m_setReferenceButton = nullptr;
    QTreeView* m_structureView = nullptr;
    QPushButton* m_addStructureButton = nullptr;
    QGroupBox* m_elementFilterBox = nullptr;
    ListChart* m_chart = nullptr;
    QTableWidget* m_shiftTable = nullptr;
    QSpinBox* m_maxPoints = nullptr;
    QDoubleSpinBox* m_lineWidthBox = nullptr;
    QPushButton* m_generateButton = nullptr;
    QPushButton* m_exportButton = nullptr;
    QPushButton* m_clearButton = nullptr;
    QTimer* m_updateTimer = nullptr;

    // MVC components
    NMRDataStore* m_dataStore = nullptr;
    NMRController* m_controller = nullptr;
    NMRStructureProxyModel* m_structureModel = nullptr;

    // Configuration
    int m_plotPoints = 100000;
    double m_lineWidth = 0.1;

    /**
     * Sets up the user interface
     */
    void setupUI();

    /**
     * Connects signals and slots
     */
    void connectSignals();

    /**
     * Creates checkboxes for element filtering
     */
    void setupElementFilters();

    /**
     * Updates element filter checkboxes
     */
    void updateElementFilters();

    /**
     * Sets up the table columns
     */
    void setupTable();

    /**
     * Updates the table with shift data
     */
    void updateTable();

    /**
     * Updates the plot with compound-element shifts
     */
    void updatePlot();
};

#endif // NMRSPECTRUMDIALOG_H
