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
    // auto fileLayout = new QVBoxLayout(fileGroupBox);

    auto structureGroupBox = new QGroupBox(tr("Strukturen"), this);
    auto structureLayout = new QVBoxLayout(structureGroupBox);

    // Reference section
    auto refLayout = new QHBoxLayout();
    m_referenceEdit = new QLineEdit(this);
    m_referenceEdit->setReadOnly(true);
    m_referenceButton = new QPushButton(tr("Referenz..."), this);
    refLayout->addWidget(new QLabel(tr("Referenz:")));
    refLayout->addWidget(m_referenceEdit);
    refLayout->addWidget(m_referenceButton);

    // Structure tree
    m_structureTree = new QTreeWidget(this);
    m_structureTree->setHeaderLabels({ tr("Struktur"), tr("Energie"), tr("Sichtbar") });
    m_structureTree->setColumnWidth(0, 200);
    m_structureTree->setColumnWidth(1, 100);

    m_addStructureButton = new QPushButton(tr("Struktur hinzufügen..."), this);

    structureLayout->addLayout(refLayout);
    structureLayout->addWidget(m_structureTree);
    structureLayout->addWidget(m_addStructureButton);

    // Connect signals
    connect(m_referenceButton, &QPushButton::clicked, this, [this]() {
        QString filename = QFileDialog::getOpenFileName(this,
            tr("Referenzstruktur wählen"),
            QString(),
            tr("ORCA Output (*.out);;Alle Dateien (*)"));
        if (!filename.isEmpty()) {
            setReference(filename, QFileInfo(filename).fileName());
            m_referenceEdit->setText(QFileInfo(filename).fileName());
        }
    });

    connect(m_addStructureButton, &QPushButton::clicked, this, [this]() {
        QStringList filenames = QFileDialog::getOpenFileNames(this,
            tr("Strukturen wählen"),
            QString(),
            tr("ORCA Output (*.out);;Alle Dateien (*)"));
        for (const QString& filename : filenames) {
            addStructure(filename, QFileInfo(filename).fileName());
        }
    });

    connect(m_structureTree, &QTreeWidget::itemChanged,
        this, [this](QTreeWidgetItem* item, int column) {
            if (column == 2) { // Visibility column
                updateTreeVisibility(item, column);
            }
        });

    // Chart section
    m_chart = new ListChart;
    m_chart->Chart()->setZoomStrategy(ZoomStrategy::Z_Horizontal);
    // Table section
    m_shiftTable = new QTableWidget(this);
    setupTable();

    QHBoxLayout* configLayout = new QHBoxLayout();
    m_maxPoints = new QSpinBox(this);
    m_maxPoints->setRange(10, 1000000);
    m_maxPoints->setValue(100000);
    configLayout->addWidget(new QLabel(tr("Max. Punkte: "), this));
    configLayout->addWidget(m_maxPoints);
    connect(m_maxPoints, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() {
        m_plotPoints = m_maxPoints->value();
    });
    m_lineWidthBox = new QDoubleSpinBox(this);
    m_lineWidthBox->setRange(0.01, 10.0);
    m_lineWidthBox->setSingleStep(0.05);
    m_lineWidthBox->setValue(0.1);
    connect(m_lineWidthBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() {
        m_lineWidth = m_lineWidthBox->value();
    });
    configLayout->addWidget(new QLabel(tr("Linienbreite: "), this));
    configLayout->addWidget(m_lineWidthBox);

    // Buttons
    auto buttonLayout = new QHBoxLayout();
    auto generateButton = new QPushButton(tr("Spektrum generieren"), this);
    auto exportButton = new QPushButton(tr("Exportieren"), this);
    auto clearButton = new QPushButton(tr("Daten löschen"), this);
    buttonLayout->addWidget(clearButton);
    buttonLayout->addWidget(generateButton);
    buttonLayout->addWidget(exportButton);

    // Layout assembly
    topLayout->addWidget(structureGroupBox, 1);
    topLayout->addWidget(m_chart, 2);
    bottomLayout->addWidget(m_shiftTable);

    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(bottomLayout);
    mainLayout->addLayout(configLayout);
    mainLayout->addLayout(buttonLayout);

    // Connect signals
    // connect(removeButton, &QPushButton::clicked, this, &NMRSpectrumDialog::removeSelectedItem);
    connect(generateButton, &QPushButton::clicked, this, &NMRSpectrumDialog::generateSpectrum);
    connect(exportButton, &QPushButton::clicked, this, &NMRSpectrumDialog::exportData);
    connect(clearButton, &QPushButton::clicked, this, &NMRSpectrumDialog::clearData);
}

QTreeWidgetItem* NMRSpectrumDialog::findOrCreateCompoundGroup(const QString& baseName)
{
    // Search for existing group
    for (int i = 0; i < m_structureTree->topLevelItemCount(); ++i) {
        auto item = m_structureTree->topLevelItem(i);
        if (item->text(0) == baseName) {
            return item;
        }
    }

    // Create new group
    auto groupItem = new QTreeWidgetItem(m_structureTree);
    groupItem->setText(0, baseName);
    groupItem->setFlags(groupItem->flags() | Qt::ItemIsUserCheckable);
    groupItem->setCheckState(2, Qt::Checked);
    return groupItem;
}

QTreeWidgetItem* NMRSpectrumDialog::addStructureToTree(const NMRData& data)
{
    QString baseName = QFileInfo(data.filename).baseName();
    QTreeWidgetItem* parentItem = findOrCreateCompoundGroup(baseName);

    // Add structure as child
    auto structItem = new QTreeWidgetItem(parentItem);
    structItem->setText(0, QFileInfo(data.filename).fileName());
    structItem->setText(1, QString::number(data.energy, 'f', 6));
    structItem->setFlags(structItem->flags() | Qt::ItemIsUserCheckable);
    structItem->setCheckState(2, Qt::Checked);

    // Add nucleus items
    for (const auto& [element, shieldings] : data.shieldings) {
        for (const auto& [nucIndex, shielding, aniso] : shieldings) {
            auto nucItem = new QTreeWidgetItem(structItem);
            nucItem->setText(0, QString("%1_%2").arg(element).arg(nucIndex));
            nucItem->setText(1, QString::number(shielding, 'f', 2));
            nucItem->setFlags(nucItem->flags() | Qt::ItemIsUserCheckable);
            nucItem->setCheckState(2, Qt::Checked);
        }
    }

    return structItem;
}

void NMRSpectrumDialog::updateTreeVisibility(QTreeWidgetItem* item, int column)
{
    // Update children
    for (int i = 0; i < item->childCount(); ++i) {
        QTreeWidgetItem* child = item->child(i);
        child->setCheckState(column, item->checkState(column));
    }

    // Update corresponding series in chart
    QString seriesName = item->text(0);
    // TODO: Implement series visibility update in chart
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
        data.isReference = true;
        m_reference = data;

        // Calculate average reference shieldings per element
        std::map<QString, int> count;
        for (const auto& [element, shieldings] : m_reference.shieldings) {
            for (const auto& [nucIndex, shielding, aniso] : shieldings) {
                m_reference.reference[element] += shielding;
                count[element]++;
            }
        }
        for (auto& [element, shielding] : m_reference.reference) {
            shielding /= count[element];
        }

        // Update reference display in tree
        auto refItem = new QTreeWidgetItem(m_structureTree);
        refItem->setText(0, tr("Referenz: %1").arg(name));
        refItem->setText(1, QString::number(data.energy, 'f', 6));
        refItem->setFlags(refItem->flags() | Qt::ItemIsUserCheckable);
        refItem->setCheckState(2, Qt::Checked);

        // Add nucleus items for reference
        for (const auto& [element, shieldings] : data.shieldings) {
            for (const auto& [nucIndex, shielding, aniso] : shieldings) {
                auto nucItem = new QTreeWidgetItem(refItem);
                nucItem->setText(0, QString("%1_%2").arg(element).arg(nucIndex));
                nucItem->setText(1, QString::number(shielding, 'f', 2));
                nucItem->setFlags(nucItem->flags() | Qt::ItemIsUserCheckable);
                nucItem->setCheckState(2, Qt::Checked);
            }
        }

        refItem->setExpanded(true);
        m_referenceEdit->setText(QFileInfo(filename).fileName());

    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Fehler"),
            tr("Fehler beim Laden der Referenz: %1").arg(e.what()));
    }
}

void NMRSpectrumDialog::addStructure(const QString& filename, const QString& name)
{
    try {
        // if (!m_reference.isReference) {
        //     throw std::runtime_error("Bitte zuerst Referenzstruktur laden");
        // }

        auto data = parseOrcaOutput(filename, name, false);

        // Check compatibility with reference
        if (!areStructuresCompatible(m_reference, data)) {
            throw std::runtime_error("Struktur nicht kompatibel mit Referenz");
        }

        // Store the structure
        m_structures.push_back(std::make_unique<NMRData>(std::move(data)));

        // Find or create group in tree and add structure
        const auto& newStruct = m_structures.back();
        auto structItem = addStructureToTree(*newStruct);

        // Check for conformers
        QString baseName = QFileInfo(filename).baseName();
        bool isConformer = false;

        // Group conformers based on matching atom types and positions
        for (int i = 0; i < m_structureTree->topLevelItemCount(); ++i) {
            auto groupItem = m_structureTree->topLevelItem(i);
            if (groupItem->text(0) == baseName && structItem->parent() != groupItem) {

                // Check if this is really a conformer
                for (int j = 0; j < groupItem->childCount(); ++j) {
                    auto existingStruct = groupItem->child(j);
                    // Find corresponding structure in m_structures
                    for (const auto& str : m_structures) {
                        if (str->filename == existingStruct->text(0)) {
                            if (isConformation(*str, *newStruct)) {
                                isConformer = true;
                                break;
                            }
                        }
                    }
                    if (isConformer)
                        break;
                }

                if (isConformer) {
                    // Move item to existing group
                    QTreeWidgetItem* parent = structItem->parent();
                    parent->removeChild(structItem);
                    groupItem->addChild(structItem);
                    break;
                }
            }
        }

        // Sort conformers by energy
        if (isConformer) {
            auto parent = structItem->parent();
            QList<QTreeWidgetItem*> items;
            while (parent->childCount() > 0) {
                items.append(parent->takeChild(0));
            }

            std::sort(items.begin(), items.end(),
                [](QTreeWidgetItem* a, QTreeWidgetItem* b) {
                    return a->text(1).toDouble() < b->text(1).toDouble();
                });

            for (auto item : items) {
                parent->addChild(item);
            }
        }

    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Fehler"),
            tr("Fehler beim Hinzufügen der Struktur: %1").arg(e.what()));
    }
}

bool NMRSpectrumDialog::isConformation(const NMRData& str1, const NMRData& str2)
{
    // Check if both structures have the same elements
    if (str1.shieldings.size() != str2.shieldings.size()) {
        return false;
    }

    // Check each element
    for (const auto& [element1, shieldings1] : str1.shieldings) {
        // Check if element exists in str2
        if (str2.shieldings.find(element1) == str2.shieldings.end()) {
            return false;
        }

        const auto& shieldings2 = str2.shieldings.at(element1);

        // Check if number of nuclei matches
        if (shieldings1.size() != shieldings2.size()) {
            return false;
        }

        // Create sorted vectors of nucleus indices for comparison
        std::vector<int> indices1, indices2;
        for (const auto& [idx, _, __] : shieldings1) {
            indices1.push_back(idx);
        }
        for (const auto& [idx, _, __] : shieldings2) {
            indices2.push_back(idx);
        }

        std::sort(indices1.begin(), indices1.end());
        std::sort(indices2.begin(), indices2.end());

        // Compare sorted indices
        if (indices1 != indices2) {
            return false;
        }
    }

    return true;
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
    qDebug() << "Min energy: " << minEnergy << " kT: " << kT << " Energies: " << energies;
    for (double e : energies) {
        sumExp += std::exp(-(e - minEnergy) / kT);
        qDebug() << "Exp: " << std::exp(-(e - minEnergy) / kT);
    }
    qDebug() << "Sum exp: " << sumExp;
    for (double e : energies) {
        weights.push_back(std::exp(-(e - minEnergy) / kT) / sumExp);
        qDebug() << "Weight: " << std::exp(-(e - minEnergy) / kT) / sumExp;
    }

    return weights;
}

void NMRSpectrumDialog::generateSpectrum()
{
    if (m_structures.empty()) {
        QMessageBox::warning(this, tr("Warnung"), tr("Keine Strukturen geladen"));
        return;
    }
    m_shiftTable->setRowCount(0);

    // Get reference shieldings
    const auto& reference = m_reference.reference;

    // Calculate Boltzmann weights for non-reference structures
    std::vector<double> energies;
    for (size_t i = 0; i < m_structures.size(); ++i) {
        energies.push_back(m_structures[i]->energy);
    }
    auto weights = calculateBoltzmannWeights(energies);

    // Collect all shift data
    std::vector<ShiftData> allShifts;
    std::map<QString, std::vector<double>> elementShifts; // for plotting

    // Process each non-reference structure
    for (const auto& structure : m_structures) {
        QString baseName = QFileInfo(structure->filename).baseName();
        for (const auto& [element, shieldings] : structure->shieldings) {
            for (const auto& [nucIndex, shielding, aniso] : shieldings) {
                double weight = weights[&structure - &m_structures[0]];
                QString seriesName = QString("%1_%2_%3")
                                         .arg(baseName)
                                         .arg(element)
                                         .arg(nucIndex);
                // Find matching reference nucleus
                // for (const auto& [refNucIndex, refShielding, refAniso] : reference) {
                // qDebug() << "Element: " << element << " Nucleus: " << nucIndex
                //    << " Ref Nucleus: " << refNucIndex;
                double refShielding = m_reference.reference[element];
                // if (nucIndex == refNucIndex) {
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
            }
            }
    }

    // Update table
    updateTable(allShifts);

    // Update plot
    updatePlotBoth(elementShifts);
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
}

void NMRSpectrumDialog::updatePlot(const std::map<QString, std::vector<double>>& elementShifts,
    bool showSticks)
{
    // Static color map to avoid recreation each time
    static const QMap<QString, QColor> elementColors{
        { "H", QColor(255, 0, 0) },
        { "C", QColor(0, 255, 0) },
        { "N", QColor(0, 0, 255) },
        { "O", QColor(255, 165, 0) },
        { "F", QColor(128, 0, 128) },
        { "P", QColor(165, 42, 42) },
        { "S", QColor(128, 128, 0) }
    };

    // Calculate plot range using std::minmax_element
    auto [xMin, xMax] = std::accumulate(
        elementShifts.begin(), elementShifts.end(),
        std::pair<double, double>{ 1000, -1000 },
        [](const auto& acc, const auto& element) {
            const auto [minShift, maxShift] = std::minmax_element(
                element.second.begin(), element.second.end());
            return std::pair<double, double>{
                std::min(acc.first, *minShift - 5),
                std::max(acc.second, *maxShift + 5)
            };
        });

    // Pre-calculate x-values for performance
    std::vector<double> xValues(m_plotPoints);
    const double xStep = (xMax - xMin) / (m_plotPoints - 1);
    for (int i = 0; i < m_plotPoints; ++i) {
        xValues[i] = xMin + i * xStep;
    }

    // Precompute gaussian function parameters
    const double lineWidth2 = 2 * std::pow(m_lineWidth, 2);

    int index = 0;
    for (const auto& [element, shifts] : elementShifts) {
        const QColor& color = elementColors.value(element, QColor(0, 0, 0));

        if (showSticks) {
            // Create stick spectrum
            auto stickSeries = new QLineSeries();
            stickSeries->setName(element + "_sticks");
            stickSeries->setColor(color);

            // For each shift, create a vertical line
            for (double shift : shifts) {
                stickSeries->append(shift, 0.0); // Bottom point
                stickSeries->append(shift, 1.0); // Top point
                stickSeries->append(shift, 0.0); // Back to bottom (to break the line)
            }

            m_chart->addSeries(stickSeries, index, color, element + "_stick", true);
        } else {
            // Create continuous spectrum
            QVector<QPointF> points(m_plotPoints);

            // Initialize points with x-values and zero y-values
            for (int i = 0; i < m_plotPoints; ++i) {
                points[i] = QPointF(xValues[i], 0.0);
            }

            // Add all peaks directly to points
            for (double shift : shifts) {
                for (int i = 0; i < m_plotPoints; ++i) {
                    double x = xValues[i];
                    points[i].setY(points[i].y() + std::exp(-std::pow(x - shift, 2) / lineWidth2));
                }
            }

            // Create and setup series
            auto combinedSeries = new QLineSeries();
            combinedSeries->setName(element);
            combinedSeries->setColor(color);
            combinedSeries->replace(points);

            m_chart->addSeries(combinedSeries, index, color, element, false);
        }
        index++;
    }

    // Setup axes once
    auto axisX = new QValueAxis();
    auto axisY = new QValueAxis();
    axisX->setTitleText(tr("Chemical Shift (ppm)"));
    axisY->setTitleText(tr("Relative Intensity"));
    axisX->setRange(xMax, xMin);
    axisY->setRange(0, showSticks ? 1.2 : 5.0); // Angepasster Y-Bereich für Sticks
}

// Optional: Überladene Version für beide Darstellungen gleichzeitig
void NMRSpectrumDialog::updatePlotBoth(const std::map<QString, std::vector<double>>& elementShifts)
{
    m_chart->Clear();

    updatePlot(elementShifts, false); // Kontinuierliches Spektrum
    updatePlot(elementShifts, true); // Stick-Spektrum
}

void NMRSpectrumDialog::clearData()
{
    if (QMessageBox::question(this, tr("Daten löschen"),
            tr("Möchten Sie wirklich alle Daten löschen?"),
            QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::No) {
        return;
    }
    m_structures.clear();
    m_reference = NMRData();
    m_structureTree->clear();
    m_shiftTable->setRowCount(0);
    m_chart->Clear();
}