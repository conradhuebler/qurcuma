// nmrspectrumdialog.h
#ifndef NMRSPECTRUMDIALOG_H
#define NMRSPECTRUMDIALOG_H

#include <QAbstractItemModel>
#include <QDialog>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

// Debug-Makros zur einfachen Fehlersuche
#define NMR_DEBUG 1

#if NMR_DEBUG
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
class ListChart;

/**
 * Structure for NMR spectrum data
 */
struct NMRData {
    QString name; // Display name
    QString filename; // Source file
    QString formula; // Molecular formula
    double energy = 0.0; // Energy in Hartree
    bool isReference = false; // Whether this is a reference

    // Map of element to vector of (nucleus index, shielding, anisotropy)
    std::map<QString, std::vector<std::tuple<int, double, double>>> shieldings;

    // Reference shieldings per element (average)
    std::map<QString, double> reference;
};

/**
 * Structure for chemical shift data
 */
struct ShiftData {
    QString element; // Element symbol
    int nucleus; // Nucleus index
    double referenceShielding; // Reference shielding value
    double shielding; // Actual shielding value
    double shift; // Chemical shift
    double weight; // Boltzmann weight
};

/**
 * Tree item class for NMR structure model
 */
class NMRTreeItem {
public:
    enum ItemType {
        RootItem,
        CompoundItem,
        StructureItem,
        NucleusItem
    };

    explicit NMRTreeItem(const QVector<QVariant>& data, NMRTreeItem* parent = nullptr, ItemType type = RootItem);
    ~NMRTreeItem();

    void appendChild(NMRTreeItem* child);
    void removeChild(int row);
    NMRTreeItem* child(int row) const;
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    int row() const;
    NMRTreeItem* parentItem() const;
    void setData(int column, const QVariant& value);
    // void setNMRData(NMRData* data) { m_nmrData = data; }
    // NMRData* getNMRData() const { return m_nmrData; }
    void sortChildren(int column, Qt::SortOrder order);
    ItemType type() const { return m_type; }
    void setType(ItemType type) { m_type = type; }
    void setReference(bool isRef) { m_isReference = isRef; }
    bool isReference() const { return m_isReference; }
    int getStructureIndex() const { return m_structureIndex; }
    void setStructureIndex(int index) { m_structureIndex = index; }

private:
    QVector<QVariant> m_itemData;
    QVector<NMRTreeItem*> m_childItems;
    NMRTreeItem* m_parentItem;
    // NMRData* m_nmrData;
    int m_structureIndex;
    ItemType m_type;
    bool m_isReference;
};

/**
 * Tree model for NMR structures
 */
class NMRStructureModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Roles {
        ItemTypeRole = Qt::UserRole + 1,
        NMRDataRole,
        ReferenceRole,
        ElementRole
    };

    explicit NMRStructureModel(QObject* parent = nullptr);
    ~NMRStructureModel();

    // QAbstractItemModel implementation
    QVariant data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    // Model-specific methods
    // QModelIndex addStructure(NMRData* data, const QString &formula);
    QModelIndex addStructure(int structureIndex, const QString& formula);
    int getStructureIndex(const QModelIndex& index) const;
    void removeItem(const QModelIndex& index);
    void clearModel();
    NMRData* findStructureByFilename(const QString& filename);
    void sortCompoundConformers(const QModelIndex& compoundIndex);
    QModelIndex findOrCreateCompound(const QString& formula);
    void setReference(const QModelIndex& index);
    NMRData* getReferenceStructure() const;
    QModelIndex getReferenceIndex() const;

    // Filter methods
    QStringList getAvailableElements() const;
    void setElementVisibility(const QString& element, bool visible);
    bool isElementVisible(const QString& element) const;

signals:
    // void referenceChanged(NMRData* newReference);
    void referenceChanged(int referenceIndex);

private:
    NMRTreeItem* m_rootItem;
    QStringList m_headerLabels;
    QModelIndex m_referenceIndex;
    QSet<QString> m_hiddenElements;

    NMRTreeItem* getItem(const QModelIndex& index) const;
    // QModelIndex findStructureParent(NMRData* data);
    std::vector<std::unique_ptr<NMRData>>* m_structures;
    void setStructuresVector(std::vector<std::unique_ptr<NMRData>>* structures) { m_structures = structures; }
    friend class NMRSpectrumDialog;
};

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
     * Adds a structure
     * @param filename The structure file
     * @param name The display name
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
     * Handles data change in the model
     */
    void handleDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);

    /**
     * Handles reference change
     */
    void onReferenceChanged(int referenceIndex);

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

    // Data model
    NMRStructureModel* m_structureModel = nullptr;
    std::vector<std::unique_ptr<NMRData>> m_structures;
    // Configuration
    int m_plotPoints;
    double m_lineWidth;
    const double m_kBoltzmann;
    const double m_temperature;

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
     * Derives a molecular formula from NMR data
     * @param data The NMR data
     * @return The derived formula string
     */
    QString deriveMolecularFormula(const NMRData& data);

    /**
     * Updates visibility of child items
     * @param index The index that changed
     * @param checked The new checked state
     */
    void updateVisibilityRecursive(const QModelIndex& index, bool checked);

    /**
     * Sets up the table columns
     */
    void setupTable();

    /**
     * Calculates reference shieldings for each element
     * @param reference The reference structure
     */
    void calculateReferenceShieldings(NMRData& reference);

    /**
     * Handles classification of conformers
     * @param newStrct The new structure
     */
    void handleConformerClassification(NMRData* newStruct);

    /**
     * Checks if two structures are conformations of each other
     * @param str1 First structure
     * @param str2 Second structure
     * @return true if they are conformations
     */
    bool isConformation(const NMRData& str1, const NMRData& str2);

    /**
     * Checks if a structure is TMS based on formula
     * @param data The structure to check
     * @return true if it's TMS
     */
    bool isTMS(const NMRData& data);

    /**
     * Parses an ORCA output file
     * @param filename The file to parse
     * @param name The name to assign
     * @return The parsed NMR data
     */
    NMRData parseOrcaOutput(const QString& filename, const QString& name);

    /**
     * Parses NMR shieldings from ORCA output
     * @param content The file content
     * @param data The data structure to populate
     */
    void parseNMRShieldings(const QString& content, NMRData& data);

    /**
     * Extracts energy from ORCA output (in Hartree)
     * @param content The file content
     * @return The extracted energy
     */
    double extractEnergy(const QString& content);

    /**
     * Calculates Boltzmann weights (with Hartree energies)
     * @param energies The energies in Hartree
     * @return Vector of weights
     */
    std::vector<double> calculateBoltzmannWeights(const std::vector<double>& energies);

    /**
     * Gets visible structures from the model
     * @return Vector of pointers to visible structures
     */
    std::vector<NMRData*> getVisibleStructures();

    /**
     * Organizes structures by compound
     * @param structures The structures to organize
     * @return Map of compound names to structure vectors
     */
    std::map<QString, std::vector<NMRData*>> organizeStructuresByCompound(
        const std::vector<NMRData*>& structures);

    /**
     * Updates the table with shift data
     * @param shifts The shifts to display
     */
    void updateTable(const std::vector<ShiftData>& shifts);

    /**
     * Updates the plot with compound-element shifts
     * @param compoundElementShifts Map of compounds to element shifts
     */
    void updatePlotByCompound(
        const std::map<QString, std::map<QString, std::vector<double>>>& compoundElementShifts);

    /**
     * Calculates the plot range
     * @param compoundElementShifts Map of compounds to element shifts
     * @return Pair of min and max x values
     */
    std::pair<double, double> calculatePlotRange(
        const std::map<QString, std::map<QString, std::vector<double>>>& compoundElementShifts);

    /**
     * Gets the structure from a model index
     * @param index The model index
     * @return Pointer to the NMR data
     */
    NMRData* getStructureFromIndex(const QModelIndex& index);
};

#endif // NMRSPECTRUMDIALOG_H
