// nmrdatastore.cpp
#include "nmrdatastore.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <stdexcept>

NMRDataStore::NMRDataStore(QObject* parent)
    : QObject(parent)
{
    NMR_LOG("DataStore created");
}

int NMRDataStore::addStructure(const QString& filename, const QString& name)
{
    NMR_LOG("Adding structure: " << filename);

    try {
        // Parse file and create structure
        auto structure = parseOrcaOutput(filename, name);

        // Calculate formula
        structure.formula = deriveFormula(structure);

        // Add to structures vector
        m_structures.push_back(std::move(structure));
        int index = m_structures.size() - 1;

        // Check if this should be set as reference (TMS or first structure)
        if (isTMS(m_structures[index]) || m_structures.size() == 1) {
            setReference(index);
        }

        // Set default visibility
        for (auto& [element, nuclei] : m_structures[index].nuclei) {
            for (auto& nucleus : nuclei) {
                // Default: H is visible, others are not
                nucleus.visible = (element == "H");
            }
        }

        // Emit signal
        emit structureAdded(index);
        emit dataChanged();

        return index;
    } catch (const std::exception& e) {
        NMR_LOG("Error adding structure: " << e.what());
        throw;
    }
}

void NMRDataStore::removeStructure(int index)
{
    if (index < 0 || index >= static_cast<int>(m_structures.size())) {
        NMR_LOG("Invalid index for removeStructure: " << index);
        return;
    }

    // If removing the reference, we'll need to select a new one
    bool wasReference = (index == m_referenceIndex);

    // Remove the structure
    m_structures.erase(m_structures.begin() + index);

    // Update reference index
    if (wasReference) {
        m_referenceIndex = -1;
        // Select a new reference if we have structures
        if (!m_structures.empty()) {
            // Try to find TMS
            for (size_t i = 0; i < m_structures.size(); ++i) {
                if (isTMS(m_structures[i])) {
                    setReference(i);
                    break;
                }
            }
            // If no TMS found, use the first structure
            if (m_referenceIndex == -1) {
                setReference(0);
            }
        }
    } else if (m_referenceIndex > index) {
        // Reference was after the removed structure, adjust index
        m_referenceIndex--;
    }

    emit structureRemoved(index);
    emit dataChanged();
}

void NMRDataStore::setReference(int index)
{
    if (index < 0 || index >= static_cast<int>(m_structures.size())) {
        NMR_LOG("Invalid index for setReference: " << index);
        return;
    }

    // Unset previous reference
    if (m_referenceIndex >= 0 && m_referenceIndex < static_cast<int>(m_structures.size())) {
        m_structures[m_referenceIndex].isReference = false;
    }

    // Set new reference
    m_structures[index].isReference = true;
    m_referenceIndex = index;

    // Calculate reference shieldings
    calculateReferenceShieldings();

    NMR_LOG("Set reference to structure at index " << index << ": " << m_structures[index].name);

    emit referenceChanged(index);
    emit structureChanged(index);
    emit dataChanged();
}

void NMRDataStore::setStructureVisible(int index, bool visible)
{
    if (index < 0 || index >= static_cast<int>(m_structures.size())) {
        NMR_LOG("Invalid index for setStructureVisible: " << index);
        return;
    }

    m_structures[index].visible = visible;

    emit structureChanged(index);
    emit dataChanged();
}

void NMRDataStore::setStructureScaleFactor(int index, double factor)
{
    if (index < 0 || index >= static_cast<int>(m_structures.size())) {
        NMR_LOG("Invalid index for setStructureScaleFactor: " << index);
        return;
    }

    if (factor <= 0)
        factor = 0.01; // Ensure factor is positive

    m_structures[index].scaleFactor = factor;

    NMR_LOG("Set scale factor for structure " << index << " to " << factor);

    emit structureChanged(index);
    emit dataChanged();
}

void NMRDataStore::setCompoundScaleFactor(const QString& compound, double factor)
{
    if (factor <= 0)
        factor = 0.01; // Ensure factor is positive

    m_compoundScaleFactors[compound] = factor;

    NMR_LOG("Set scale factor for compound " << compound << " to " << factor);

    emit dataChanged();
}

double NMRDataStore::getCompoundScaleFactor(const QString& compound) const
{
    auto it = m_compoundScaleFactors.find(compound);
    if (it != m_compoundScaleFactors.end()) {
        return it->second;
    }
    return 1.0; // Default scale factor
}

const NMRDataStore::NMRStructure* NMRDataStore::getStructure(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_structures.size())) {
        NMR_LOG("Invalid index for getStructure: " << index);
        return nullptr;
    }

    return &m_structures[index];
}

NMRDataStore::NMRStructure* NMRDataStore::getStructure(int index)
{
    if (index < 0 || index >= static_cast<int>(m_structures.size())) {
        NMR_LOG("Invalid index for getStructure: " << index);
        return nullptr;
    }

    return &m_structures[index];
}

const NMRDataStore::NMRStructure* NMRDataStore::getReferenceStructure() const
{
    if (m_referenceIndex < 0 || m_referenceIndex >= static_cast<int>(m_structures.size())) {
        NMR_LOG("No valid reference structure");
        return nullptr;
    }

    return &m_structures[m_referenceIndex];
}

int NMRDataStore::getReferenceIndex() const
{
    return m_referenceIndex;
}

const std::vector<NMRDataStore::NMRStructure>& NMRDataStore::getAllStructures() const
{
    return m_structures;
}

int NMRDataStore::getStructureCount() const
{
    return static_cast<int>(m_structures.size());
}

void NMRDataStore::clearAllStructures()
{
    m_structures.clear();
    m_referenceIndex = -1;
    m_elementVisibility.clear();
    m_compoundScaleFactors.clear();

    NMR_LOG("All structures cleared");

    emit dataChanged();
}

void NMRDataStore::setNucleusVisible(int structureIndex, const QString& element, int nucleusIndex, bool visible)
{
    if (structureIndex < 0 || structureIndex >= static_cast<int>(m_structures.size())) {
        NMR_LOG("Invalid structure index for setNucleusVisible: " << structureIndex);
        return;
    }

    auto& structure = m_structures[structureIndex];
    auto elementIt = structure.nuclei.find(element);

    if (elementIt == structure.nuclei.end()) {
        NMR_LOG("Element not found in structure: " << element);
        return;
    }

    // Find the nucleus by index
    auto& nuclei = elementIt->second;
    for (auto& nucleus : nuclei) {
        if (nucleus.index == nucleusIndex) {
            nucleus.visible = visible;
            break;
        }
    }

    emit nucleusVisibilityChanged(structureIndex, element, nucleusIndex, visible);
    emit structureChanged(structureIndex);
    emit dataChanged();
}

void NMRDataStore::setAllNucleiVisible(const QString& element, bool visible)
{
    NMR_LOG("Setting all nuclei of element " << element << " to " << visible);

    m_elementVisibility[element] = visible;

    // Update all structures
    for (size_t i = 0; i < m_structures.size(); ++i) {
        auto& structure = m_structures[i];
        auto elementIt = structure.nuclei.find(element);

        if (elementIt != structure.nuclei.end()) {
            for (auto& nucleus : elementIt->second) {
                nucleus.visible = visible;
            }
            emit structureChanged(i);
        }
    }

    emit elementVisibilityChanged(element, visible);
    emit dataChanged();
}

bool NMRDataStore::isNucleusVisible(int structureIndex, const QString& element, int nucleusIndex) const
{
    if (structureIndex < 0 || structureIndex >= static_cast<int>(m_structures.size())) {
        return false;
    }

    const auto& structure = m_structures[structureIndex];
    auto elementIt = structure.nuclei.find(element);

    if (elementIt == structure.nuclei.end()) {
        return false;
    }

    const auto& nuclei = elementIt->second;
    for (const auto& nucleus : nuclei) {
        if (nucleus.index == nucleusIndex) {
            return nucleus.visible;
        }
    }

    return false;
}

bool NMRDataStore::isElementVisible(const QString& element) const
{
    auto it = m_elementVisibility.find(element);
    if (it != m_elementVisibility.end()) {
        return it->second;
    }
    return (element == "H"); // Default: only H is visible
}

QString NMRDataStore::deriveFormula(const NMRStructure& structure) const
{
    // Count elements
    std::map<QString, int> elementCounts;

    for (const auto& [element, nuclei] : structure.nuclei) {
        elementCounts[element] += nuclei.size();
    }

    // Standard element order
    const std::vector<QString> elementOrder = { "C", "H", "N", "O", "F", "P", "S", "Si" };

    // Build molecular formula
    QString formula;

    // First add elements in standard order
    for (const auto& element : elementOrder) {
        if (elementCounts.count(element)) {
            formula += element;
            if (elementCounts[element] > 1) {
                formula += QString::number(elementCounts[element]);
            }
            elementCounts.erase(element);
        }
    }

    // Then add any remaining elements alphabetically
    std::vector<QString> remainingElements;
    for (const auto& [element, count] : elementCounts) {
        remainingElements.push_back(element);
    }
    std::sort(remainingElements.begin(), remainingElements.end());

    for (const auto& element : remainingElements) {
        formula += element;
        if (elementCounts[element] > 1) {
            formula += QString::number(elementCounts[element]);
        }
    }

    NMR_LOG("Derived formula: " << formula << " for " << structure.name);
    return formula;
}

QStringList NMRDataStore::getAvailableElements() const
{
    QSet<QString> elements;

    for (const auto& structure : m_structures) {
        for (const auto& [element, _] : structure.nuclei) {
            elements.insert(element);
        }
    }

    QStringList result = elements.values();
    std::sort(result.begin(), result.end());

    return result;
}

void NMRDataStore::calculateReferenceShieldings()
{
    if (m_referenceIndex < 0 || m_referenceIndex >= static_cast<int>(m_structures.size())) {
        NMR_LOG("No valid reference structure for calculating reference shieldings");
        return;
    }

    auto& reference = m_structures[m_referenceIndex];
    std::map<QString, int> count;

    // Clear existing reference values
    reference.referenceShieldings.clear();

    // Sum up all shieldings per element
    for (const auto& [element, nuclei] : reference.nuclei) {
        for (const auto& nucleus : nuclei) {
            reference.referenceShieldings[element] += nucleus.shielding;
            count[element]++;
        }
    }

    // Calculate average shielding per element
    for (auto& [element, shielding] : reference.referenceShieldings) {
        shielding /= count[element];
        NMR_LOG("Reference shielding for " << element << ": " << shielding);
    }

    // Calculate shifts for all structures
    for (auto& structure : m_structures) {
        for (auto& [element, nuclei] : structure.nuclei) {
            if (reference.referenceShieldings.count(element)) {
                double refShielding = reference.referenceShieldings[element];
                for (auto& nucleus : nuclei) {
                    nucleus.shift = nucleus.shielding - refShielding; // Calculate as shielding - reference
                }
            }
        }
    }
}

std::vector<NMRDataStore::NMRStructure*> NMRDataStore::getVisibleStructures()
{
    std::vector<NMRStructure*> visibleStructures;

    for (auto& structure : m_structures) {
        if (structure.visible) {
            visibleStructures.push_back(&structure);
        }
    }

    return visibleStructures;
}

std::map<QString, std::vector<NMRDataStore::NMRStructure*>> NMRDataStore::organizeStructuresByCompound(
    const std::vector<NMRStructure*>& structures)
{
    std::map<QString, std::vector<NMRStructure*>> result;

    for (auto* structure : structures) {
        result[structure->formula].push_back(structure);
    }

    return result;
}

std::vector<double> NMRDataStore::calculateBoltzmannWeights(const std::vector<double>& energies)
{
    // Convert Hartree to kcal/mol (1 Hartree = 627.509 kcal/mol)
    const double hartreeToKcalMol = 627.509;
    const double kT = m_kBoltzmann * m_temperature;
    std::vector<double> weights(energies.size());

    // Convert energies to kcal/mol for Boltzmann calculation
    std::vector<double> energiesKcalMol(energies.size());
    for (size_t i = 0; i < energies.size(); ++i) {
        energiesKcalMol[i] = energies[i] * hartreeToKcalMol;
    }

    // Find minimum energy to prevent overflow
    double minEnergy = *std::min_element(energiesKcalMol.begin(), energiesKcalMol.end());

    // Calculate Boltzmann distribution
    double sumExp = 0.0;
    for (double e : energiesKcalMol) {
        sumExp += std::exp(-(e - minEnergy) / kT);
    }

    // Calculate weights
    for (size_t i = 0; i < energiesKcalMol.size(); ++i) {
        weights[i] = std::exp(-(energiesKcalMol[i] - minEnergy) / kT) / sumExp;
    }

    return weights;
}

std::vector<NMRDataStore::ShiftData> NMRDataStore::getAllShifts()
{
    std::vector<ShiftData> allShifts;

    // Make sure we have a reference
    const NMRStructure* reference = getReferenceStructure();
    if (!reference) {
        NMR_LOG("No reference structure for getting shifts");
        return allShifts;
    }

    // Get visible structures organized by compound
    auto visibleStructures = getVisibleStructures();
    auto compoundStructures = organizeStructuresByCompound(visibleStructures);

    // Process each compound
    for (const auto& [compound, structures] : compoundStructures) {
        // Calculate Boltzmann weights
        std::vector<double> energies;
        for (const auto* structure : structures) {
            energies.push_back(structure->energy);
        }
        auto weights = calculateBoltzmannWeights(energies);

        // Process each structure
        for (size_t i = 0; i < structures.size(); ++i) {
            const auto* structure = structures[i];
            double weight = weights[i];

            for (const auto& [element, nuclei] : structure->nuclei) {
                // Skip hidden elements
                if (!isElementVisible(element)) {
                    continue;
                }

                // Get reference shielding
                if (!reference->referenceShieldings.count(element)) {
                    continue;
                }
                double refShielding = reference->referenceShieldings.at(element);

                // Process each nucleus
                for (const auto& nucleus : nuclei) {
                    // Skip hidden nuclei
                    if (!nucleus.visible) {
                        continue;
                    }

                    ShiftData data{
                        element,
                        nucleus.index,
                        refShielding,
                        nucleus.shielding,
                        nucleus.shift,
                        weight
                    };
                    allShifts.push_back(data);
                }
            }
        }
    }

    return allShifts;
}

std::map<QString, std::map<QString, std::vector<double>>> NMRDataStore::getCompoundElementShifts()
{
    std::map<QString, std::map<QString, std::vector<double>>> compoundElementShifts;

    // Get visible structures organized by compound
    auto visibleStructures = getVisibleStructures();
    auto compoundStructures = organizeStructuresByCompound(visibleStructures);

    // Process each compound
    for (const auto& [compound, structures] : compoundStructures) {
        // Process each structure
        for (const auto* structure : structures) {
            for (const auto& [element, nuclei] : structure->nuclei) {
                // Skip hidden elements
                if (!isElementVisible(element)) {
                    continue;
                }

                // Process each nucleus
                for (const auto& nucleus : nuclei) {
                    // Skip hidden nuclei
                    if (!nucleus.visible) {
                        continue;
                    }

                    compoundElementShifts[compound][element].push_back(nucleus.shift);
                }
            }
        }
    }

    return compoundElementShifts;
}

std::map<QString, double> NMRDataStore::getCompoundScaleFactors()
{
    return m_compoundScaleFactors;
}

void NMRDataStore::sortCompoundConformers(const QString& compound)
{
    // Find all structures with this compound formula
    std::vector<int> conformerIndices;

    for (size_t i = 0; i < m_structures.size(); ++i) {
        if (m_structures[i].formula == compound) {
            conformerIndices.push_back(i);
        }
    }

    // Sort by energy
    std::sort(conformerIndices.begin(), conformerIndices.end(),
        [this](int a, int b) {
            return m_structures[a].energy < m_structures[b].energy;
        });

    NMR_LOG("Sorted " << conformerIndices.size() << " conformers of compound " << compound);

    // Note: Since we can't reorder the m_structures vector without invalidating pointers,
    // we just notify that the compound has been sorted, and the UI model will need to update
    for (int index : conformerIndices) {
        emit structureChanged(index);
    }
    emit dataChanged();
}

bool NMRDataStore::isTMS(const NMRStructure& structure) const
{
    QString formula = structure.formula;

    // Check for Si(CH3)4 or Si1C4H12 formula
    return (formula == "C4H12Si" || formula == "Si1C4H12" || structure.name.contains("TMS", Qt::CaseInsensitive));
}

bool NMRDataStore::isConformation(const NMRStructure& str1, const NMRStructure& str2) const
{
    // Check if both structures have the same elements
    if (str1.nuclei.size() != str2.nuclei.size()) {
        return false;
    }

    // Check each element
    for (const auto& [element1, nuclei1] : str1.nuclei) {
        // Check if element exists in str2
        auto it = str2.nuclei.find(element1);
        if (it == str2.nuclei.end()) {
            return false;
        }

        const auto& nuclei2 = it->second;

        // Check if number of nuclei matches
        if (nuclei1.size() != nuclei2.size()) {
            return false;
        }

        // Create sorted vectors of nucleus indices for comparison
        std::vector<int> indices1, indices2;
        for (const auto& nucleus : nuclei1) {
            indices1.push_back(nucleus.index);
        }
        for (const auto& nucleus : nuclei2) {
            indices2.push_back(nucleus.index);
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

NMRDataStore::NMRStructure NMRDataStore::parseOrcaOutput(const QString& filename, const QString& name) const
{
    NMR_LOG("Parsing ORCA output file: " << filename);

    NMRStructure structure;
    structure.name = name;
    structure.filename = filename;
    structure.isReference = false;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        NMR_LOG("Failed to open file: " << filename);
        throw std::runtime_error("Datei konnte nicht geöffnet werden");
    }

    QTextStream in(&file);
    QString content = in.readAll();

    // Extract energy
    structure.energy = extractEnergy(content);
    NMR_LOG("Extracted energy: " << structure.energy);

    // Parse NMR shieldings
    parseNMRShieldings(content, structure);

    NMR_LOG("Parsing completed successfully");
    return structure;
}

void NMRDataStore::parseNMRShieldings(const QString& content, NMRStructure& structure) const
{
    QRegularExpression rx("(\\d+)\\s+([A-Za-z]+)\\s+([-]?\\d+\\.\\d+)\\s+([-]?\\d+\\.\\d+)");
    QRegularExpression headerRx("CHEMICAL SHIELDING SUMMARY \\(ppm\\)");

    auto headerMatch = headerRx.match(content);
    if (!headerMatch.hasMatch()) {
        NMR_LOG("Failed to find NMR shielding data in file");
        throw std::runtime_error("NMR Daten nicht gefunden");
    }

    auto matches = rx.globalMatch(content, headerMatch.capturedEnd());
    while (matches.hasNext()) {
        auto match = matches.next();

        int nucleusIndex = match.captured(1).toInt();
        QString element = match.captured(2);
        double shielding = match.captured(3).toDouble();
        double anisotropy = match.captured(4).toDouble();

        NucleusData nucleus;
        nucleus.index = nucleusIndex;
        nucleus.shielding = shielding;
        nucleus.anisotropy = anisotropy;

        structure.nuclei[element].push_back(nucleus);
        NMR_LOG("Added shielding: " << element << "_" << nucleusIndex << " = " << shielding);
    }

    NMR_LOG("Found " << structure.nuclei.size() << " elements with shieldings");
}

double NMRDataStore::extractEnergy(const QString& content) const
{
    QRegularExpression rx("FINAL SINGLE POINT ENERGY\\s+(-?\\d+\\.\\d+)");
    auto match = rx.match(content);
    if (match.hasMatch()) {
        return match.captured(1).toDouble();
    }

    NMR_LOG("Failed to extract energy from file");
    throw std::runtime_error("Energie nicht gefunden");
}
