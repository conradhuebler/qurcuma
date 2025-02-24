// nmrspectrumdialog.cpp
#include "nmrspectrumdialog.h"
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <QtCharts>

#include "CuteChart/src/charts.h"

#include <cmath>

NMRSpectrumDialog::NMRSpectrumDialog(QWidget* parent)
    : QDialog(parent)
{
    // Setup UI
    setWindowTitle(tr("NMR Spektren Analyse"));
    setMinimumSize(800, 600);

    auto mainLayout = new QVBoxLayout(this);
    auto topLayout = new QHBoxLayout();
    auto bottomLayout = new QHBoxLayout();

    // File list section
    auto fileGroupBox = new QGroupBox(tr("Strukturen"), this);
    auto fileLayout = new QVBoxLayout(fileGroupBox);
    m_fileList = new QListWidget(this);
    auto removeButton = new QPushButton(tr("Entfernen"), this);
    fileLayout->addWidget(m_fileList);
    fileLayout->addWidget(removeButton);

    // Chart section
    m_chart = new ListChart;
    // m_chart->setTitle(tr("NMR Spektrum"));
    // m_chart->legend()->setVisible(true);
    // m_chart->legend()->setAlignment(Qt::AlignBottom);

    // m_chartView = m_chart->chartView();
    // m_chartView->setRenderHint(QPainter::Antialiasing);

    // Table section
    m_shiftTable = new QTableWidget(this);
    setupTable();

    // Buttons
    auto buttonLayout = new QHBoxLayout();
    auto generateButton = new QPushButton(tr("Spektrum generieren"), this);
    auto exportButton = new QPushButton(tr("Exportieren"), this);
    buttonLayout->addWidget(generateButton);
    buttonLayout->addWidget(exportButton);

    // Layout assembly
    topLayout->addWidget(fileGroupBox, 1);
    topLayout->addWidget(m_chart, 2);
    bottomLayout->addWidget(m_shiftTable);

    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(bottomLayout);
    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(removeButton, &QPushButton::clicked, this, &NMRSpectrumDialog::removeSelectedItem);
    connect(generateButton, &QPushButton::clicked, this, &NMRSpectrumDialog::generateSpectrum);
    connect(exportButton, &QPushButton::clicked, this, &NMRSpectrumDialog::exportData);
}

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
}

void NMRSpectrumDialog::setReference(const QString& filename, const QString& name)
{
    try {
        auto data = parseOrcaOutput(filename, name, true);
        m_structures.clear(); // Remove old reference
        m_structures.push_back(std::make_unique<NMRData>(std::move(data)));
        m_fileList->clear();
        m_fileList->addItem(tr("Referenz: %1").arg(name));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Fehler"), tr("Fehler beim Laden der Referenz: %1").arg(e.what()));
    }
}

void NMRSpectrumDialog::addStructure(const QString& filename, const QString& name)
{
    try {
        if (m_structures.empty()) {
            throw std::runtime_error("Keine Referenzstruktur geladen");
        }

        auto data = parseOrcaOutput(filename, name, false);
        // if (!areStructuresCompatible(*m_structures[0], data)) {
        //     throw std::runtime_error("Struktur nicht kompatibel mit Referenz");
        // }

        m_structures.push_back(std::make_unique<NMRData>(std::move(data)));
        m_fileList->addItem(name);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Fehler"), tr("Fehler beim Hinzufügen der Struktur: %1").arg(e.what()));
    }
}

NMRData NMRSpectrumDialog::parseOrcaOutput(const QString& filename, const QString& name, bool isRef)
{
    NMRData data;
    data.name = name;
    data.filename = filename;
    data.isReference = isRef;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error("Datei konnte nicht geöffnet werden");
    }

    QTextStream in(&file);
    QString content = in.readAll();

    // Extract energy
    data.energy = extractEnergy(content);

    // Parse NMR shieldings
    QRegularExpression rx("(\\d+)\\s+([A-Za-z]+)\\s+([-]?\\d+\\.\\d+)\\s+([-]?\\d+\\.\\d+)");
    QRegularExpression headerRx("CHEMICAL SHIELDING SUMMARY \\(ppm\\)");

    auto headerMatch = headerRx.match(content);
    if (!headerMatch.hasMatch()) {
        throw std::runtime_error("NMR Daten nicht gefunden");
    }

    auto matches = rx.globalMatch(content, headerMatch.capturedEnd());
    while (matches.hasNext()) {
        auto match = matches.next();

        int nucleus = match.captured(1).toInt();
        QString element = match.captured(2);
        double shielding = match.captured(3).toDouble();
        double anisotropy = match.captured(4).toDouble();

        data.shieldings[element].emplace_back(nucleus, shielding, anisotropy);
    }

    return data;
}

double NMRSpectrumDialog::extractEnergy(const QString& content)
{
    QRegularExpression rx("FINAL SINGLE POINT ENERGY\\s+(-?\\d+\\.\\d+)");
    auto match = rx.match(content);
    if (match.hasMatch()) {
        return match.captured(1).toDouble();
    }
    throw std::runtime_error("Energie nicht gefunden");
}

bool NMRSpectrumDialog::areStructuresCompatible(const NMRData& str1, const NMRData& str2)
{
    /*  if (str1.shieldings.size() != str2.shieldings.size()) return false;

      for (size_t i = 0; i < str1.shieldings.size(); ++i) {
          if (std::get<1>(str1.shieldings[i]) != std::get<1>(str2.shieldings[i])) {
              return false;
          }
      }*/
    return true;
}

std::vector<double> NMRSpectrumDialog::calculateBoltzmannWeights(const std::vector<double>& energies)
{
    const double kT = m_kBoltzmann * m_temperature;
    std::vector<double> weights;

    // Find minimum energy to prevent overflow
    double minEnergy = *std::min_element(energies.begin(), energies.end());

    // Calculate Boltzmann distribution
    double sumExp = 0.0;
    for (double e : energies) {
        sumExp += std::exp(-(e - minEnergy) / kT);
    }

    for (double e : energies) {
        weights.push_back(std::exp(-(e - minEnergy) / kT) / sumExp);
    }

    return weights;
}

void NMRSpectrumDialog::generateSpectrum()
{
    if (m_structures.empty()) {
        QMessageBox::warning(this, tr("Warnung"), tr("Keine Strukturen geladen"));
        return;
    }

    if (!m_structures[0]->isReference) {
        QMessageBox::warning(this, tr("Warnung"), tr("Erste Struktur muss Referenz sein"));
        return;
    }

    // Clear old data
    // m_chart->removeAllSeries();
    m_shiftTable->setRowCount(0);

    // Get reference shieldings
    const auto& reference = m_structures[0]->shieldings;

    // Calculate Boltzmann weights for non-reference structures
    std::vector<double> energies;
    for (size_t i = 1; i < m_structures.size(); ++i) {
        energies.push_back(m_structures[i]->energy);
    }
    auto weights = calculateBoltzmannWeights(energies);

    // Collect all shift data
    std::vector<ShiftData> allShifts;
    std::map<QString, std::vector<double>> elementShifts; // for plotting

    // Process each non-reference structure
    for (size_t structIndex = 1; structIndex < m_structures.size(); ++structIndex) {
        const auto& structure = m_structures[structIndex];
        double weight = weights[structIndex - 1];

        // Process each element in the structure
        for (const auto& [element, shieldings] : structure->shieldings) {
            if (reference.count(element) > 0) {
                // Process each nucleus
                for (const auto& [nucIndex, shielding, aniso] : shieldings) {
                    // Find matching reference nucleus
                    for (const auto& [refNucIndex, refShielding, refAniso] : reference.at(element)) {
                        if (nucIndex == refNucIndex) {
                            double shift = shielding - refShielding;

                            ShiftData data{
                                element,
                                nucIndex,
                                refShielding,
                                shielding,
                                shift,
                                weight
                            };
                            allShifts.push_back(data);
                            elementShifts[element].push_back(shift);
                            break;
                        }
                    }
                }
            }
        }
    }

    // Update table
    updateTable(allShifts);

    // Update plot
    updatePlot(elementShifts);
}

void NMRSpectrumDialog::updateTable(const std::vector<ShiftData>& shifts)
{
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
}

void NMRSpectrumDialog::exportData()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Spektrum exportieren"), "",
        tr("CSV Dateien (*.csv);;Alle Dateien (*)"));

    if (filename.isEmpty())
        return;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << "Element,Kern,Referenz-Abschirmung,Abschirmung,Chem. Verschiebung,Gewicht\n";

    for (int row = 0; row < m_shiftTable->rowCount(); ++row) {
        QStringList fields;
        for (int col = 0; col < m_shiftTable->columnCount(); ++col) {
            fields << m_shiftTable->item(row, col)->text();
        }
        out << fields.join(",") << "\n";
    }
}

void NMRSpectrumDialog::updatePlot(const std::map<QString, std::vector<double>>& elementShifts)
{
    // Clear previous series
    // m_chart->removeAllSeries();

    // Find plot range
    double xMin = 1000, xMax = -1000;
    for (const auto& [element, shifts] : elementShifts) {
        for (double shift : shifts) {
            xMin = std::min(xMin, shift - 5);
            xMax = std::max(xMax, shift + 5);
        }
    }

    // Create series for each element
    QMap<QString, QColor> elementColors{
        { "H", QColor(255, 0, 0) },
        { "C", QColor(0, 255, 0) },
        { "N", QColor(0, 0, 255) },
        { "O", QColor(255, 165, 0) },
        { "F", QColor(128, 0, 128) },
        { "P", QColor(165, 42, 42) },
        { "S", QColor(128, 128, 0) }
    };
    int index = 0;
    for (const auto& [element, shifts] : elementShifts) {
        // Create combined series for all peaks of this element
        auto combinedSeries = new QLineSeries();
        combinedSeries->setName(element);

        QColor color = elementColors.value(element, QColor(0, 0, 0));
        combinedSeries->setColor(color);

        // Initialize combined series with zeros
        QVector<QPointF> points;
        for (int i = 0; i < m_plotPoints; ++i) {
            double x = xMin + i * (xMax - xMin) / (m_plotPoints - 1);
            points.append(QPointF(x, 0.0));
        }

        // Add all peaks for this element
        for (double shift : shifts) {
            auto peakSeries = createGaussianPeak(shift, 1.0, xMin, xMax, m_plotPoints);

            // Add peak values to combined series
            for (int i = 0; i < m_plotPoints; ++i) {
                points[i].setY(points[i].y() + peakSeries->at(i).y());
            }
            delete peakSeries;
        }

        combinedSeries->replace(points);
        m_chart->addSeries(combinedSeries, index, color, element, false);
        index++;
    }

    // Setup axes
    auto axisX = new QValueAxis();
    auto axisY = new QValueAxis();

    axisX->setTitleText(tr("Chemical Shift (ppm)"));
    axisY->setTitleText(tr("Relative Intensity"));

    axisX->setRange(xMax, xMin); // Reverse direction for NMR convention
    axisY->setRange(0, 5);

    // m_chart->addAxis(axisX, Qt::AlignBottom);
    // m_chart->addAxis(axisY, Qt::AlignLeft);

    // Attach axes to all series
    /*for (auto series : m_chart->series()) {
        series->attachAxis(axisX);
        series->attachAxis(axisY);
    }*/
}

QLineSeries* NMRSpectrumDialog::createGaussianPeak(double shift, double intensity,
    double xMin, double xMax, int points)
{
    auto series = new QLineSeries();

    for (int i = 0; i < points; ++i) {
        double x = xMin + i * (xMax - xMin) / (points - 1);
        double y = intensity * std::exp(-std::pow(x - shift, 2) / (2 * std::pow(m_lineWidth, 2)));
        series->append(x, y);
    }

    return series;
}

void NMRSpectrumDialog::handleMarkerClicked()
{
    auto marker = qobject_cast<QScatterSeries*>(sender());
    if (marker) {
        // Handle marker click - could show details about the peak
        QMessageBox::information(this, tr("Peak Information"),
            tr("Chemical Shift: %1 ppm").arg(marker->at(0).x()));
    }
}

void NMRSpectrumDialog::removeSelectedItem()
{
    auto items = m_fileList->selectedItems();
    for (auto item : items) {
        int row = m_fileList->row(item);
        if (row > 0) { // Prevent removing reference
            delete m_fileList->takeItem(row);
            m_structures.erase(m_structures.begin() + row);
        }
    }
}
