// nmrcontroller.cpp
#include "nmrcontroller.h"
#include "nmrdatastore.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <algorithm>
#include <limits>

NMRController::NMRController(NMRDataStore* dataStore, QObject* parent)
    : QObject(parent)
    , m_dataStore(dataStore)
{
    // Verbinde Signale vom DataStore
    connect(m_dataStore, &NMRDataStore::dataChanged, this, &NMRController::handleDataStoreChanged);
    connect(m_dataStore, &NMRDataStore::structureAdded, this, [this](int index) {
        emit structureLoaded(index);
    });
    connect(m_dataStore, &NMRDataStore::structureRemoved, this, [this](int index) {
        emit structureRemoved(index);
    });
    connect(m_dataStore, &NMRDataStore::referenceChanged, this, [this](int index) {
        emit referenceChanged(index);
    });
    connect(m_dataStore, &NMRDataStore::elementVisibilityChanged, this, [this](const QString& element, bool visible) {
        emit elementVisibilityChanged(element, visible);
    });
    connect(m_dataStore, &NMRDataStore::nucleusVisibilityChanged,
        this, [this](int index, const QString& element, int nucleusIndex, bool visible) {
            emit nucleusVisibilityChanged(index, element, nucleusIndex, visible);
        });

    NMR_LOG("Controller created with DataStore");
}

void NMRController::loadStructure(const QString& filename, const QString& name)
{
    NMR_LOG("Loading structure: " << filename);

    try {
        int index = m_dataStore->addStructure(filename, name);
        NMR_LOG("Structure loaded with index: " << index);

        // Sortiere Konformere, wenn vorhanden
        const auto* structure = m_dataStore->getStructure(index);
        if (structure) {
            sortCompoundConformers(structure->formula);
        }

    } catch (const std::exception& e) {
        NMR_LOG("Error loading structure: " << e.what());
        emit spectrumGenerationFailed(tr("Fehler beim Laden der Struktur: %1").arg(e.what()));
    }
}

void NMRController::removeStructure(int index)
{
    NMR_LOG("Removing structure with index: " << index);
    m_dataStore->removeStructure(index);
}

void NMRController::clearAllStructures()
{
    NMR_LOG("Clearing all structures");
    m_dataStore->clearAllStructures();
    emit allStructuresCleared();
}

void NMRController::setReference(int index)
{
    NMR_LOG("Setting reference to index: " << index);
    m_dataStore->setReference(index);
}

void NMRController::setStructureVisible(int index, bool visible)
{
    NMR_LOG("Setting structure " << index << " visibility to: " << visible);
    m_dataStore->setStructureVisible(index, visible);
    emit structureVisibilityChanged(index, visible);
}

void NMRController::setStructureScaleFactor(int index, double factor)
{
    NMR_LOG("Setting structure " << index << " scale factor to: " << factor);
    m_dataStore->setStructureScaleFactor(index, factor);
    emit scaleFactorChanged(index, factor);
}

void NMRController::setCompoundScaleFactor(const QString& compound, double factor)
{
    NMR_LOG("Setting compound " << compound << " scale factor to: " << factor);
    m_dataStore->setCompoundScaleFactor(compound, factor);
    emit compoundScaleFactorChanged(compound, factor);
}

void NMRController::setElementVisibility(const QString& element, bool visible)
{
    NMR_LOG("Setting element " << element << " visibility to: " << visible);
    m_dataStore->setAllNucleiVisible(element, visible);
}

void NMRController::setNucleusVisibility(int structureIndex, const QString& element, int nucleusIndex, bool visible)
{
    NMR_LOG("Setting nucleus " << element << "_" << nucleusIndex << " visibility to: " << visible);
    m_dataStore->setNucleusVisible(structureIndex, element, nucleusIndex, visible);
}

QStringList NMRController::getAvailableElements() const
{
    return m_dataStore->getAvailableElements();
}

bool NMRController::isElementVisible(const QString& element) const
{
    return m_dataStore->isElementVisible(element);
}

bool NMRController::generateSpectrum(int plotPoints, double lineWidth)
{
    NMR_LOG("Generating spectrum with " << plotPoints << " points and lineWidth " << lineWidth);

    if (!hasReference()) {
        NMR_LOG("Failed to generate spectrum: No reference structure");
        emit spectrumGenerationFailed(tr("Bitte wählen Sie eine Referenzstruktur."));
        return false;
    }

    if (!hasVisibleStructures()) {
        NMR_LOG("Failed to generate spectrum: No visible structures");
        emit spectrumGenerationFailed(tr("Keine sichtbaren Strukturen vorhanden."));
        return false;
    }

    // Sammle die Daten für das Spektrum
    m_compoundElementShifts = m_dataStore->getCompoundElementShifts();
    m_compoundScaleFactors = m_dataStore->getCompoundScaleFactors();

    if (m_compoundElementShifts.empty()) {
        NMR_LOG("Failed to generate spectrum: No shifts");
        emit spectrumGenerationFailed(tr("Keine chemischen Verschiebungen gefunden."));
        return false;
    }

    // Berechne den Spektrumsbereich
    m_spectrumRange = calculateSpectrumRange(m_compoundElementShifts);

    NMR_LOG("Spectrum generation successful");
    emit spectrumGenerated();
    return true;
}

std::pair<double, double> NMRController::getSpectrumRange() const
{
    return m_spectrumRange;
}

bool NMRController::exportData(const QString& filename)
{
    NMR_LOG("Exporting data to file: " << filename);

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        NMR_LOG("Failed to open file for export: " << filename);
        // emit dataExportFailed(tr("Datei konnte nicht zum Schreiben geöffnet werden."));
        return false;
    }

    QTextStream out(&file);

    // Header
    out << "Element,Kern,Referenz-Abschirmung,Abschirmung,Chem. Verschiebung,Gewicht\n";

    // Daten
    auto shifts = m_dataStore->getAllShifts();
    for (const auto& shift : shifts) {
        out << shift.element << ","
            << shift.nucleus << ","
            << QString::number(shift.referenceShielding, 'f', 3) << ","
            << QString::number(shift.shielding, 'f', 3) << ","
            << QString::number(shift.shift, 'f', 3) << ","
            << QString::number(shift.weight, 'f', 3) << "\n";
    }

    file.close();

    NMR_LOG("Data export successful");
    emit dataExported(filename);
    return true;
}

void NMRController::sortCompoundConformers(const QString& compound)
{
    NMR_LOG("Sorting conformers for compound: " << compound);
    m_dataStore->sortCompoundConformers(compound);
}

bool NMRController::hasReference() const
{
    return m_dataStore->getReferenceStructure() != nullptr;
}

bool NMRController::hasVisibleStructures() const
{
    return !m_dataStore->getVisibleStructures().empty();
}

void NMRController::handleDataStoreChanged()
{
    NMR_LOG("Data store changed notification received");
}

std::pair<double, double> NMRController::calculateSpectrumRange(
    const std::map<QString, std::map<QString, std::vector<double>>>& compoundElementShifts,
    double padding)
{
    double xMin = std::numeric_limits<double>::max();
    double xMax = std::numeric_limits<double>::lowest();

    // Find global min/max across all compounds and elements
    for (const auto& [compound, elementShifts] : compoundElementShifts) {
        for (const auto& [element, shifts] : elementShifts) {
            if (!shifts.empty()) {
                auto [localMin, localMax] = std::minmax_element(shifts.begin(), shifts.end());
                xMin = std::min(xMin, *localMin);
                xMax = std::max(xMax, *localMax);
            }
        }
    }

    // Add padding to range
    return { xMin - padding, xMax + padding };
}
