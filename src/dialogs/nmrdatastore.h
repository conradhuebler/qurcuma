// nmrdatastore.h
#ifndef NMRDATASTORE_H
#define NMRDATASTORE_H

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

// Debug-Makros zur einfachen Fehlersuche
#define NMR_DEBUG 1

#if NMR_DEBUG
#include <QDebug>
#define NMR_LOG(msg) qDebug() << "[NMRDataStore] " << msg
#else
#define NMR_LOG(msg)
#endif

class NMRDataStore : public QObject {
    Q_OBJECT

public:
    // Kernstruktur für Nukleonen
    struct NucleusData {
        int index;
        double shielding;
        double anisotropy;
        double shift = 0.0; // Berechneter chemischer Shift
        bool visible = true; // Sichtbar in der Anzeige
    };

    // Struktur für NMR-Daten
    struct NMRStructure {
        QString filename;
        QString name;
        QString formula;
        double energy = 0.0;
        bool isReference = false;
        double scaleFactor = 1.0;
        bool visible = true;
        std::map<QString, std::vector<NucleusData>> nuclei;
        std::map<QString, double> referenceShieldings; // Referenzabschirmungen pro Element
    };

    // Struktur für chemische Verschiebungsdaten
    struct ShiftData {
        QString element;
        int nucleus;
        double referenceShielding;
        double shielding;
        double shift;
        double weight;
    };

    // Konstruktor
    explicit NMRDataStore(QObject* parent = nullptr);

    // Struktur-Management
    int addStructure(const QString& filename, const QString& name);
    void removeStructure(int index);
    void setReference(int index);
    void setStructureVisible(int index, bool visible);
    void setStructureScaleFactor(int index, double factor);
    void setCompoundScaleFactor(const QString& compound, double factor);
    double getCompoundScaleFactor(const QString& compound) const;
    const NMRStructure* getStructure(int index) const;
    NMRStructure* getStructure(int index);
    const NMRStructure* getReferenceStructure() const;
    int getReferenceIndex() const;
    const std::vector<NMRStructure>& getAllStructures() const;
    int getStructureCount() const;
    void clearAllStructures();

    // Kern-Management
    void setNucleusVisible(int structureIndex, const QString& element, int nucleusIndex, bool visible);
    void setAllNucleiVisible(const QString& element, bool visible);
    bool isNucleusVisible(int structureIndex, const QString& element, int nucleusIndex) const;
    bool isElementVisible(const QString& element) const;

    // Hilfsmethoden
    QString deriveFormula(const NMRStructure& structure) const;
    QStringList getAvailableElements() const;
    void calculateReferenceShieldings();
    std::vector<NMRStructure*> getVisibleStructures();
    std::map<QString, std::vector<NMRStructure*>> organizeStructuresByCompound(const std::vector<NMRStructure*>& structures);
    std::vector<double> calculateBoltzmannWeights(const std::vector<double>& energies);
    std::vector<ShiftData> getAllShifts();
    std::map<QString, std::map<QString, std::vector<double>>> getCompoundElementShifts();
    std::map<QString, double> getCompoundScaleFactors();
    void sortCompoundConformers(const QString& compound);
    bool isTMS(const NMRStructure& structure) const;
    bool isConformation(const NMRStructure& str1, const NMRStructure& str2) const;

signals:
    void structureAdded(int index);
    void structureRemoved(int index);
    void structureChanged(int index);
    void referenceChanged(int index);
    void elementVisibilityChanged(const QString& element, bool visible);
    void nucleusVisibilityChanged(int structureIndex, const QString& element, int nucleusIndex, bool visible);
    void dataChanged();

private:
    std::vector<NMRStructure> m_structures;
    int m_referenceIndex = -1;
    std::map<QString, bool> m_elementVisibility;
    std::map<QString, double> m_compoundScaleFactors;

    // Konstanten für Boltzmann-Berechnung
    const double m_kBoltzmann = 0.001987204258; // kcal/(mol·K)
    const double m_temperature = 298.15; // K

    // Private Hilfsmethoden
    NMRStructure parseOrcaOutput(const QString& filename, const QString& name) const;
    void parseNMRShieldings(const QString& content, NMRStructure& structure) const;
    double extractEnergy(const QString& content) const;
};

#endif // NMRDATASTORE_H
