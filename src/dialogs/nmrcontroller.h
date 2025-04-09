// nmrcontroller.h
#ifndef NMRCONTROLLER_H
#define NMRCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <map>
#include <memory>
#include <vector>

class NMRDataStore;

// Debug-Makros zur einfachen Fehlersuche
#define NMR_DEBUG 1

#if NMR_DEBUG
#include <QDebug>
#define NMR_LOG(msg) qDebug() << "[NMRController] " << msg
#else
#define NMR_LOG(msg)
#endif

class NMRController : public QObject {
    Q_OBJECT

public:
    explicit NMRController(NMRDataStore* dataStore, QObject* parent = nullptr);

    // Struktur-Management
    void loadStructure(const QString& filename, const QString& name);
    void removeStructure(int index);
    void clearAllStructures();
    void setReference(int index);
    void setStructureVisible(int index, bool visible);
    void setStructureScaleFactor(int index, double factor);
    void setCompoundScaleFactor(const QString& compound, double factor);

    // Element- und Kern-Management
    void setElementVisibility(const QString& element, bool visible);
    void setNucleusVisibility(int structureIndex, const QString& element, int nucleusIndex, bool visible);
    QStringList getAvailableElements() const;
    bool isElementVisible(const QString& element) const;

    // Spektrum-Generierung
    bool generateSpectrum(int plotPoints, double lineWidth);
    std::pair<double, double> getSpectrumRange() const;

    // Datenexport
    bool exportData(const QString& filename);

    // Konformer-Management
    void sortCompoundConformers(const QString& compound);

    // Hilfsmethoden
    bool hasReference() const;
    bool hasVisibleStructures() const;

signals:
    void spectrumGenerated();
    void spectrumGenerationFailed(const QString& errorMessage);
    void dataExported(const QString& filename);
    void dataExportFailed(const QString& errorMessage);
    void structureLoaded(int index);
    void structureRemoved(int index);
    void allStructuresCleared();
    void referenceChanged(int index);
    void structureVisibilityChanged(int index, bool visible);
    void elementVisibilityChanged(const QString& element, bool visible);
    void nucleusVisibilityChanged(int structureIndex, const QString& element, int nucleusIndex, bool visible);
    void scaleFactorChanged(int index, double factor);
    void compoundScaleFactorChanged(const QString& compound, double factor);

private slots:
    void handleDataStoreChanged();

private:
    NMRDataStore* m_dataStore;

    // Spektrum-Daten
    std::map<QString, std::map<QString, std::vector<double>>> m_compoundElementShifts;
    std::map<QString, double> m_compoundScaleFactors;
    std::pair<double, double> m_spectrumRange;

    // Hilfsmethoden
    static std::pair<double, double> calculateSpectrumRange(
        const std::map<QString, std::map<QString, std::vector<double>>>& compoundElementShifts,
        double padding = 5.0);
};

#endif // NMRCONTROLLER_H
