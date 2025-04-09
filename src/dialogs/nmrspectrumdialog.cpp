// nmrspectrumdialog.cpp
#include "nmrspectrumdialog.h"
#include "nmrcontroller.h"
#include "nmrdatastore.h"
#include "nmrstructureproxymodel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelection>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>
#include <QtCharts>

#include "CuteChart/src/charts.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

/**
 * Constructor for NMR Spectrum Dialog
 * Initializes the UI components and connects signals to slots
 * @param parent The parent widget
 */
NMRSpectrumDialog::NMRSpectrumDialog(QWidget* parent)
    : QDialog(parent)
{
    // Create model components
    m_dataStore = new NMRDataStore(this);
    m_controller = new NMRController(m_dataStore, this);
    m_structureModel = new NMRStructureProxyModel(m_dataStore, this);

    // Setup UI
    setWindowTitle(tr("NMR Spektren Analyse"));
    setMinimumSize(800, 600);

    setupUI();
    connectSignals();

    NMR_LOG("Dialog created");
}

/**
 * Sets up the user interface components
 */
void NMRSpectrumDialog::setupUI()
{
    auto mainLayout = new QVBoxLayout(this);
    auto topLayout = new QHBoxLayout();
    auto bottomLayout = new QHBoxLayout();

    // Structure section
    auto structureGroupBox = new QGroupBox(tr("Strukturen"), this);
    auto structureLayout = new QVBoxLayout(structureGroupBox);

    // Structure tree model and view
    m_structureView = new QTreeView(this);
    m_structureView->setModel(m_structureModel);
    m_structureView->setAlternatingRowColors(true);
    m_structureView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_structureView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_structureView->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_structureView->setAnimated(true);
    m_structureView->setExpandsOnDoubleClick(true);
    m_structureView->setIndentation(20);
    m_structureView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_structureView->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents); // Skalierungsspalte

    // Structure buttons
    auto buttonLayout = new QHBoxLayout();
    m_addStructureButton = new QPushButton(tr("Struktur hinzufügen..."), this);
    m_setReferenceButton = new QPushButton(tr("Als Referenz setzen"), this);
    m_setReferenceButton->setEnabled(false); // Enable only when a structure is selected
    buttonLayout->addWidget(m_addStructureButton);
    buttonLayout->addWidget(m_setReferenceButton);

    // Element filter
    m_elementFilterBox = new QGroupBox(tr("Elementfilter"), this);
    auto filterLayout = new QHBoxLayout(m_elementFilterBox);
    // We'll populate this later as structures are added

    structureLayout->addWidget(m_structureView);
    structureLayout->addLayout(buttonLayout);
    structureLayout->addWidget(m_elementFilterBox);

    // Chart section
    m_chart = new ListChart;
    m_chart->Chart()->setZoomStrategy(ZoomStrategy::Z_Horizontal);

    // Table section
    m_shiftTable = new QTableWidget(this);
    setupTable();

    // Configuration layout
    QHBoxLayout* configLayout = new QHBoxLayout();

    // Max points spinner
    m_maxPoints = new QSpinBox(this);
    m_maxPoints->setRange(10, 1000000);
    m_maxPoints->setValue(m_plotPoints);
    configLayout->addWidget(new QLabel(tr("Max. Punkte: "), this));
    configLayout->addWidget(m_maxPoints);

    // Line width spinner
    m_lineWidthBox = new QDoubleSpinBox(this);
    m_lineWidthBox->setRange(0.01, 10.0);
    m_lineWidthBox->setSingleStep(0.05);
    m_lineWidthBox->setValue(m_lineWidth);
    configLayout->addWidget(new QLabel(tr("Linienbreite: "), this));
    configLayout->addWidget(m_lineWidthBox);

    // Buttons
    auto actionButtonLayout = new QHBoxLayout();
    m_generateButton = new QPushButton(tr("Spektrum generieren"), this);
    m_exportButton = new QPushButton(tr("Exportieren"), this);
    m_clearButton = new QPushButton(tr("Daten löschen"), this);
    actionButtonLayout->addWidget(m_clearButton);
    actionButtonLayout->addWidget(m_generateButton);
    actionButtonLayout->addWidget(m_exportButton);

    // Layout assembly
    topLayout->addWidget(structureGroupBox, 1);
    topLayout->addWidget(m_chart, 2);
    bottomLayout->addWidget(m_shiftTable);

    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(bottomLayout);
    mainLayout->addLayout(configLayout);
    mainLayout->addLayout(actionButtonLayout);

    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(500); // 500 ms delay

    NMR_LOG("UI setup completed");
}

/**
 * Connects signals and slots
 */
void NMRSpectrumDialog::connectSignals()
{
    // Button actions
    connect(m_addStructureButton, &QPushButton::clicked, this, &NMRSpectrumDialog::selectStructureFiles);
    connect(m_setReferenceButton, &QPushButton::clicked, this, &NMRSpectrumDialog::setAsReference);
    connect(m_generateButton, &QPushButton::clicked, this, &NMRSpectrumDialog::generateSpectrum);
    connect(m_exportButton, &QPushButton::clicked, this, &NMRSpectrumDialog::exportData);
    connect(m_clearButton, &QPushButton::clicked, this, &NMRSpectrumDialog::clearData);

    // Configuration changes
    connect(m_maxPoints, QOverload<int>::of(&QSpinBox::valueChanged), this, &NMRSpectrumDialog::setPlotPoints);
    connect(m_lineWidthBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NMRSpectrumDialog::setLineWidth);

    // TreeView selection
    connect(m_structureView->selectionModel(), &QItemSelectionModel::selectionChanged,
        this, &NMRSpectrumDialog::handleSelectionChanged);

    // Model changes
    connect(m_structureModel, &QAbstractItemModel::dataChanged, this, &NMRSpectrumDialog::handleDataChanged);

    // Controller signals
    connect(m_controller, &NMRController::spectrumGenerated, this, &NMRSpectrumDialog::handleSpectrumGenerated);
    connect(m_controller, &NMRController::spectrumGenerationFailed, this, &NMRSpectrumDialog::handleSpectrumGenerationFailed);

    // Timer connection
    connect(m_updateTimer, &QTimer::timeout, this, &NMRSpectrumDialog::generateSpectrum);

    NMR_LOG("Signals connected");
}

/**
 * Creates checkboxes for element filtering
 */
void NMRSpectrumDialog::setupElementFilters()
{
    // Clear existing filters
    QLayoutItem* child;
    while ((child = m_elementFilterBox->layout()->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    // Get available elements
    QStringList elements = m_controller->getAvailableElements();

    NMR_LOG("Setting up element filters with " << elements.size() << " elements");

    if (elements.isEmpty()) {
        auto label = new QLabel(tr("Keine Elemente verfügbar"), m_elementFilterBox);
        m_elementFilterBox->layout()->addWidget(label);
        return;
    }

    // Create checkbox for each element
    for (const QString& element : elements) {
        auto checkbox = new QCheckBox(element, m_elementFilterBox);
        checkbox->setChecked(m_controller->isElementVisible(element));
        checkbox->setObjectName(element); // Store element in object name

        connect(checkbox, &QCheckBox::stateChanged, this, &NMRSpectrumDialog::elementFilterChanged);

        m_elementFilterBox->layout()->addWidget(checkbox);
        NMR_LOG("Added checkbox for element: " << element);
    }
}

/**
 * Updates element filter checkboxes
 ndle*/
void NMRSpectrumDialog::updateElementFilters()
{
    setupElementFilters();
}

/**
 * Handles element filter changes
 */
void NMRSpectrumDialog::elementFilterChanged(int state)
{
    QCheckBox* checkbox = qobject_cast<QCheckBox*>(sender());
    if (!checkbox)
        return;

    QString element = checkbox->objectName();
    bool visible = (state == Qt::Checked);

    NMR_LOG("Element filter changed for " << element << " to " << visible);

    m_controller->setElementVisibility(element, visible);

    // Schedule spectrum update
    m_updateTimer->start();
}

/**
 * Handles selection change in the tree view
 */
void NMRSpectrumDialog::handleSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    // Prüfe, ob eine Auswahl getroffen wurde
    if (selected.isEmpty()) {
        m_setReferenceButton->setEnabled(false);
        return;
    }

    // Hole das ausgewählte Element
    QModelIndex current = selected.indexes().first();

    auto itemType = m_structureModel->getItemType(current);

    // Only enable reference button for structure items
    m_setReferenceButton->setEnabled(itemType == NMRStructureProxyModel::StructureItem);

    NMR_LOG("Selection changed to item type " << itemType);
}

/**
 * Handles data change in the model
 */
void NMRSpectrumDialog::handleDataChanged()
{
    NMR_LOG("Model data changed");

    // Schedule spectrum update
    m_updateTimer->start();
}

/**
 * Sets a structure as reference
 */
void NMRSpectrumDialog::setAsReference()
{
    QModelIndex index = m_structureView->selectionModel()->currentIndex();
    if (!index.isValid())
        return;

    auto itemType = m_structureModel->getItemType(index);
    if (itemType != NMRStructureProxyModel::StructureItem)
        return;

    int structureIndex = m_structureModel->getStructureIndex(index);
    if (structureIndex < 0)
        return;

    m_controller->setReference(structureIndex);
    NMR_LOG("Set reference to structure index " << structureIndex);
}

/**
 * Sets the number of plot points
 * @param points The number of points to use
 */
void NMRSpectrumDialog::setPlotPoints(int points)
{
    m_plotPoints = points;
    NMR_LOG("Plot points set to " << points);
}

/**
 * Sets the line width for the spectrum
 * @param width The width to use
 */
void NMRSpectrumDialog::setLineWidth(double width)
{
    m_lineWidth = width;
    NMR_LOG("Line width set to " << width);
}

/**
 * Sets up the table for displaying chemical shifts
 */
void NMRSpectrumDialog::setupTable()
{
    m_shiftTable->setColumnCount(6);
    m_shiftTable->setHorizontalHeaderLabels({ tr("Element"),
        tr("Kern"),
        tr("Referenz-Abschirmung"),
        tr("Abschirmung"),
        tr("Chem. Verschiebung"),
        tr("Gewicht") });
    m_shiftTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    NMR_LOG("Table setup completed");
}

void NMRSpectrumDialog::addStructure(const QString& filename, const QString& name)
{
    m_controller->loadStructure(filename, name);
    updateElementFilters();
    NMR_LOG("Added structure: " << name);
}

/**
 * Selects structure files when the add structure button is clicked
 */
void NMRSpectrumDialog::selectStructureFiles()
{
    QStringList filenames = QFileDialog::getOpenFileNames(
        this,
        tr("Strukturen wählen"),
        QString(),
        tr("ORCA Output (*.out);;Alle Dateien (*)"));

    for (const QString& filename : filenames) {
        m_controller->loadStructure(filename, QFileInfo(filename).fileName());
    }

    // Update element filters after adding structures
    updateElementFilters();
}

/**
 * Generates the NMR spectrum based on loaded structures
 */
void NMRSpectrumDialog::generateSpectrum()
{
    NMR_LOG("Generating spectrum with " << m_plotPoints << " points and line width " << m_lineWidth);

    // Clear previous data
    m_chart->Clear();
    m_shiftTable->setRowCount(0);

    // Generate spectrum using controller
    m_controller->generateSpectrum(m_plotPoints, m_lineWidth);
}

/**
 * Handles spectrum generation completion
 */
void NMRSpectrumDialog::handleSpectrumGenerated()
{
    NMR_LOG("Spectrum generation completed");

    // Update UI components
    updateTable();
    updatePlot();
}

/**
 * Handles spectrum generation failure
 */
void NMRSpectrumDialog::handleSpectrumGenerationFailed(const QString& message)
{
    NMR_LOG("Spectrum generation failed: " << message);

    QMessageBox::warning(this, tr("Fehler"), message);
}

/**
 * Updates the table with shift data
 */
void NMRSpectrumDialog::updateTable()
{
    auto shifts = m_dataStore->getAllShifts();

    m_shiftTable->setRowCount(shifts.size());

    for (size_t i = 0; i < shifts.size(); ++i) {
        const auto& data = shifts[i];

        m_shiftTable->setItem(i, 0, new QTableWidgetItem(data.element));
        m_shiftTable->setItem(i, 1, new QTableWidgetItem(QString::number(data.nucleus)));
        m_shiftTable->setItem(i, 2, new QTableWidgetItem(QString::number(data.referenceShielding, 'f', 3)));
        m_shiftTable->setItem(i, 3, new QTableWidgetItem(QString::number(data.shielding, 'f', 3)));
        m_shiftTable->setItem(i, 4, new QTableWidgetItem(QString::number(data.shift, 'f', 3)));
        m_shiftTable->setItem(i, 5, new QTableWidgetItem(QString::number(data.weight, 'f', 3)));
    }

    NMR_LOG("Table updated with " << shifts.size() << " rows");
}

/**
 * Updates the plot with the spectrum data
 */
void NMRSpectrumDialog::updatePlot()
{
    // Get data from the data store
    auto compoundElementShifts = m_dataStore->getCompoundElementShifts();
    auto compoundScaleFactors = m_dataStore->getCompoundScaleFactors();

    // Calculate spectrum range
    auto [xMin, xMax] = m_controller->getSpectrumRange();

    // Static color map for elements
    static const QMap<QString, QColor> elementColors{
        { "H", QColor(255, 0, 0) },
        { "C", QColor(0, 255, 0) },
        { "N", QColor(0, 0, 255) },
        { "O", QColor(255, 165, 0) },
        { "F", QColor(128, 0, 128) },
        { "P", QColor(165, 42, 42) },
        { "S", QColor(128, 128, 0) }
    };

    // Color map for compounds (generate dynamically)
    QMap<QString, QColor> compoundColors;
    {
        int hue = 0;
        const int hueStep = 360 / (compoundElementShifts.size() + 1);
        for (const auto& [compound, _] : compoundElementShifts) {
            compoundColors[compound] = QColor::fromHsv(hue, 255, 255);
            hue = (hue + hueStep) % 360;
        }
    }

    // Pre-calculate x-values for performance
    std::vector<double> xValues(m_plotPoints);
    const double xStep = (xMax - xMin) / (m_plotPoints - 1);
    for (int i = 0; i < m_plotPoints; ++i) {
        xValues[i] = xMin + i * xStep;
    }

    // Precompute gaussian function parameters
    const double lineWidth2 = 2 * std::pow(m_lineWidth, 2);

    // Generate series for each compound and element
    int seriesIndex = 0;
    for (const auto& [compound, elementShifts] : compoundElementShifts) {
        for (const auto& [element, shifts] : elementShifts) {
            // Get scale factor for this compound
            double scaleFactor = compoundScaleFactors.count(compound) ? compoundScaleFactors.at(compound) : 1.0;

            const QColor& color = elementColors.value(element, QColor(0, 0, 0));
            QColor compoundColor = compoundColors.value(compound, QColor(0, 0, 0));

            // Blend element and compound colors
            QColor blendedColor = QColor(
                (color.red() + compoundColor.red()) / 2,
                (color.green() + compoundColor.green()) / 2,
                (color.blue() + compoundColor.blue()) / 2);

            // Create series name
            QString seriesName = QString("%1_%2").arg(compound).arg(element);

            // Create continuous spectrum series
            QVector<QPointF> points(m_plotPoints);
            for (int i = 0; i < m_plotPoints; ++i) {
                points[i] = QPointF(xValues[i], 0.0);
            }

            // Add all peaks
            for (double shift : shifts) {
                for (int i = 0; i < m_plotPoints; ++i) {
                    double x = xValues[i];
                    points[i].setY(points[i].y() + scaleFactor * std::exp(-std::pow(x - shift, 2) / lineWidth2));
                }
            }

            // Create and add series
            auto lineSeries = new QLineSeries();
            lineSeries->setName(seriesName);
            lineSeries->setColor(blendedColor);
            lineSeries->replace(points);

            m_chart->addSeries(lineSeries, seriesIndex, blendedColor, seriesName, false);

            // Create stick series
            auto stickSeries = new QLineSeries();
            stickSeries->setName(seriesName + "_sticks");
            stickSeries->setColor(blendedColor);

            // Add stick for each shift
            for (double shift : shifts) {
                stickSeries->append(shift, 0.0);
                stickSeries->append(shift, 1.0 * scaleFactor);
                stickSeries->append(shift, 0.0);
            }

            m_chart->addSeries(stickSeries, seriesIndex, blendedColor, seriesName + "_stick", true);
            seriesIndex++;
        }
    }

    NMR_LOG("Plot updated with " << seriesIndex << " series");
}

/**
 * Exports data to a CSV file
 */
void NMRSpectrumDialog::exportData()
{
    QString filename = QFileDialog::getSaveFileName(
        this,
        tr("Spektrum exportieren"), "",
        tr("CSV Dateien (*.csv);;Alle Dateien (*)"));

    if (filename.isEmpty())
        return;

    m_controller->exportData(filename);
}

/**
 * Clears all data
 */
void NMRSpectrumDialog::clearData()
{
    if (QMessageBox::question(
            this,
            tr("Daten löschen"),
            tr("Möchten Sie wirklich alle Daten löschen?"),
            QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::No) {
        return;
    }

    m_controller->clearAllStructures();
    m_chart->Clear();
    m_shiftTable->setRowCount(0);
    updateElementFilters();

    NMR_LOG("All data cleared");
}
