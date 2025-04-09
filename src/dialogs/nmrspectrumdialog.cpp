// nmrspectrumdialog.cpp
#include "nmrspectrumdialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QTableWidget>
#include <QTextStream>
#include <QTreeView>
#include <QVBoxLayout>
#include <QtCharts>

#include "CuteChart/src/charts.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

//--------------------------------------------------
// NMRTreeItem implementation
//--------------------------------------------------

NMRTreeItem::NMRTreeItem(const QVector<QVariant>& data, NMRTreeItem* parent, ItemType type)
    : m_itemData(data)
    , m_parentItem(parent)
    , m_structureIndex(-1)
    , m_type(type)
    , m_isReference(false)
{
    NMR_LOG("Created tree item of type " << type);
}

NMRTreeItem::~NMRTreeItem()
{
    qDeleteAll(m_childItems);
}

void NMRTreeItem::appendChild(NMRTreeItem* item)
{
    m_childItems.append(item);
}

void NMRTreeItem::removeChild(int row)
{
    if (row >= 0 && row < m_childItems.size()) {
        NMR_LOG("Removing child at row " << row);
        delete m_childItems.takeAt(row);
    }
}

NMRTreeItem* NMRTreeItem::child(int row) const
{
    if (row < 0 || row >= m_childItems.size())
        return nullptr;
    return m_childItems.at(row);
}

int NMRTreeItem::childCount() const
{
    return m_childItems.size();
}

int NMRTreeItem::columnCount() const
{
    return m_itemData.size();
}

QVariant NMRTreeItem::data(int column) const
{
    if (column < 0 || column >= m_itemData.size())
        return QVariant();
    return m_itemData.at(column);
}

NMRTreeItem* NMRTreeItem::parentItem() const
{
    return m_parentItem;
}

int NMRTreeItem::row() const
{
    if (m_parentItem)
        return m_parentItem->m_childItems.indexOf(const_cast<NMRTreeItem*>(this));
    return 0;
}

void NMRTreeItem::setData(int column, const QVariant& value)
{
    if (column < 0 || column >= m_itemData.size())
        return;
    m_itemData[column] = value;
}

void NMRTreeItem::sortChildren(int column, Qt::SortOrder order)
{
    // Sort children by the given column
    std::sort(m_childItems.begin(), m_childItems.end(),
        [column, order](const NMRTreeItem* a, const NMRTreeItem* b) {
            // Handle different data types appropriately
            if (a->data(column).type() == QVariant::Double || b->data(column).type() == QVariant::Double) {
                double valA = a->data(column).toDouble();
                double valB = b->data(column).toDouble();
                return order == Qt::AscendingOrder ? valA < valB : valA > valB;
            } else {
                QString valA = a->data(column).toString();
                QString valB = b->data(column).toString();
                return order == Qt::AscendingOrder ? valA < valB : valA > valB;
            }
        });
}

//--------------------------------------------------
// NMRStructureModel implementation
//--------------------------------------------------

NMRStructureModel::NMRStructureModel(QObject* parent)
    : QAbstractItemModel(parent)
    , m_structures(nullptr)
{
    m_headerLabels << "Struktur" << "Energie (Hartree)" << "Sichtbar" << "Scaling Factor";

    // Create root item
    QVector<QVariant> rootData;
    for (const auto& label : m_headerLabels)
        rootData << label;

    m_rootItem = new NMRTreeItem(rootData, nullptr, NMRTreeItem::RootItem);

    NMR_LOG("Model created");
}

NMRStructureModel::~NMRStructureModel()
{
    delete m_rootItem;
}

QVariant NMRStructureModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    NMRTreeItem* item = getItem(index);

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (index.column() == 2) // Don't show text for checkbox column
            return QVariant();

        // For reference structures, add a visual indicator
        if (item->isReference() && index.column() == 0) {
            return QString("%1 [Referenz]").arg(item->data(0).toString());
        }

        return item->data(index.column());
    } else if (role == Qt::CheckStateRole && index.column() == 2) {
        return item->data(2).toBool() ? Qt::Checked : Qt::Unchecked;
    } else if (role == ItemTypeRole) {
        return static_cast<int>(item->type());
    } else if (role == NMRDataRole) {
        // Statt des Pointers, geben wir den Index zurück
        return item->getStructureIndex();
    } else if (role == ReferenceRole) {
        return item->isReference();
    } else if (role == ElementRole) {
        // For nucleus items, return the element
        if (item->type() == NMRTreeItem::NucleusItem) {
            QString nucleusText = item->data(0).toString();
            return nucleusText.section('_', 0, 0); // Get the element part before underscore
        }
    } else if (role == ScaleFactorRole) {
        if (item->type() == NMRTreeItem::StructureItem) {
            int structureIndex = item->getStructureIndex();
            if (structureIndex >= 0 && structureIndex < m_structures->size()) {
                return (*m_structures)[structureIndex]->scaleFactor;
            }
        }
        return 1.0;
    }

    return QVariant();
}

Qt::ItemFlags NMRStructureModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    NMRTreeItem* item = getItem(index);
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    if (index.column() == 2)
        flags |= Qt::ItemIsUserCheckable;

    if (index.column() == 3 && item->type() == NMRTreeItem::CompoundItem) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

QModelIndex NMRStructureModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    NMRTreeItem* parentItem = getItem(parent);
    NMRTreeItem* childItem = parentItem->child(row);

    if (childItem)
        return createIndex(row, column, childItem);
    return QModelIndex();
}

QModelIndex NMRStructureModel::parent(const QModelIndex& index) const
{
    if (!index.isValid())
        return QModelIndex();

    NMRTreeItem* childItem = getItem(index);
    NMRTreeItem* parentItem = childItem->parentItem();

    if (parentItem == m_rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int NMRStructureModel::rowCount(const QModelIndex& parent) const
{
    NMRTreeItem* parentItem = getItem(parent);
    return parentItem->childCount();
}

int NMRStructureModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return m_headerLabels.size();
}

QVariant NMRStructureModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return m_headerLabels.at(section);
    return QVariant();
}

bool NMRStructureModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return false;

    NMRTreeItem* item = getItem(index);

    if (role == Qt::CheckStateRole && index.column() == 2) {
        bool checked = value.toInt() == Qt::Checked;
        NMR_LOG("Setting checked state for item " << item->data(0).toString() << " to " << checked);
        item->setData(index.column(), checked);
        emit dataChanged(index, index, { role });
        return true;
    } else if ((role == Qt::EditRole || role == Qt::DisplayRole) && index.column() == 3) {
        // Skalierungsfaktor setzen
        bool ok;
        double factor = value.toDouble(&ok);
        if (!ok)
            return false;

        // Sicherstellen, dass der Wert sinnvoll ist (z.B. > 0)
        if (factor <= 0)
            factor = 0.01;

        item->setData(index.column(), factor);

        // Wenn es sich um ein Compound-Item handelt, den Skalierungsfaktor im eigentlichen Datensatz speichern
        if (item->type() == NMRTreeItem::CompoundItem) {
            // Bei generateSpectrum diesen Wert aus dem Modell auslesen
            NMR_LOG("Scale factor for compound " << item->data(0).toString() << " set to " << factor);
        }

        emit dataChanged(index, index, { Qt::DisplayRole, Qt::EditRole });
        return true;
    } else if (role == ReferenceRole) {
        bool isRef = value.toBool();
        item->setReference(isRef);

        // If it's becoming a reference, update the m_referenceIndex
        if (isRef) {
            m_referenceIndex = index.sibling(index.row(), 0);
            NMR_LOG("Setting reference index to row " << index.row());
        }

        // Update display
        emit dataChanged(index.sibling(index.row(), 0), index.sibling(index.row(), 0), { Qt::DisplayRole });
        return true;
    } else if (role == ScaleFactorRole) {
        if (item->type() == NMRTreeItem::StructureItem) {
            int structureIndex = item->getStructureIndex();
            if (structureIndex >= 0 && structureIndex < m_structures->size()) {
                (*m_structures)[structureIndex]->scaleFactor = value.toDouble();
                emit dataChanged(index, index, { role });
                return true;
            }
        }
    }

    return false;
}

NMRTreeItem* NMRStructureModel::getItem(const QModelIndex& index) const
{
    if (index.isValid()) {
        NMRTreeItem* item = static_cast<NMRTreeItem*>(index.internalPointer());
        if (item)
            return item;
    }
    return m_rootItem;
}

int NMRStructureModel::getStructureIndex(const QModelIndex& index) const
{
    if (!index.isValid())
        return -1;

    NMRTreeItem* item = getItem(index);
    return item->getStructureIndex();
}

QModelIndex NMRStructureModel::addStructure(int structureIndex, const QString& formula)
{
    if (!m_structures || structureIndex < 0 || structureIndex >= static_cast<int>(m_structures->size())) {
        NMR_LOG("Invalid structure index: " << structureIndex);
        return QModelIndex();
    }

    NMRData* data = (*m_structures)[structureIndex].get();
    NMR_LOG("Adding structure: " << data->filename << " with formula: " << formula);

    // Find or create compound group
    QModelIndex compoundIndex = findOrCreateCompound(formula);
    NMRTreeItem* parentItem = getItem(compoundIndex);

    // Add structure as child
    beginInsertRows(compoundIndex, parentItem->childCount(), parentItem->childCount());

    QVector<QVariant> structItemData;
    structItemData << QFileInfo(data->filename).fileName();
    structItemData << QString::number(data->energy, 'f', 6);
    structItemData << true; // Visible by default
    structItemData << 1.0; // Default scale factor

    auto structItem = new NMRTreeItem(structItemData, parentItem, NMRTreeItem::StructureItem);
    structItem->setStructureIndex(structureIndex);
    parentItem->appendChild(structItem);

    // Add nucleus items
    for (const auto& [element, shieldings] : data->shieldings) {
        for (const auto& [nucIndex, shielding, aniso] : shieldings) {
            QVector<QVariant> nucItemData;
            nucItemData << QString("%1_%2").arg(element).arg(nucIndex);
            nucItemData << QString::number(shielding, 'f', 2);
            nucItemData << element == "H"; // H visible by default

            auto nucItem = new NMRTreeItem(nucItemData, structItem, NMRTreeItem::NucleusItem);
            structItem->appendChild(nucItem);
        }
    }

    endInsertRows();

    QModelIndex result = index(parentItem->childCount() - 1, 0, compoundIndex);
    NMR_LOG("Structure added: " << data->filename);

    return result;
}

QModelIndex NMRStructureModel::findOrCreateCompound(const QString& formula)
{
    // Search for existing group with this formula
    for (int i = 0; i < m_rootItem->childCount(); ++i) {
        auto item = m_rootItem->child(i);
        if (item->data(0).toString() == formula) {
            return index(i, 0, QModelIndex());
        }
    }

    // Create new group
    beginInsertRows(QModelIndex(), m_rootItem->childCount(), m_rootItem->childCount());

    QVector<QVariant> groupItemData;
    groupItemData << formula; // Display text is the formula
    groupItemData << QVariant(); // No energy for group
    groupItemData << true; // Visible by default
    groupItemData << 1.0; // Default scale factor
    auto groupItem = new NMRTreeItem(groupItemData, m_rootItem, NMRTreeItem::CompoundItem);
    m_rootItem->appendChild(groupItem);

    endInsertRows();

    return index(m_rootItem->childCount() - 1, 0, QModelIndex());
}

void NMRStructureModel::removeItem(const QModelIndex& index)
{
    if (!index.isValid())
        return;

    NMRTreeItem* item = getItem(index);
    NMRTreeItem* parentItem = item->parentItem();

    if (!parentItem || parentItem == m_rootItem)
        return; // Don't remove top-level items this way

    beginRemoveRows(parent(index), index.row(), index.row());
    parentItem->removeChild(index.row());
    endRemoveRows();
}

void NMRStructureModel::clearModel()
{
    NMR_LOG("Clearing model");
    beginResetModel();

    // Delete all items except the root
    while (m_rootItem->childCount() > 0) {
        m_rootItem->removeChild(0);
    }

    m_referenceIndex = QModelIndex();

    endResetModel();
}

NMRData* NMRStructureModel::findStructureByFilename(const QString& filename)
{
    NMR_LOG("Searching for structure with filename: " << filename);

    if (!m_structures) {
        NMR_LOG("Structures vector is null");
        return nullptr;
    }

    for (const auto& structure : *m_structures) {
        if (structure->filename == filename) {
            NMR_LOG("Found structure: " << filename);
            return structure.get();
        }
    }

    NMR_LOG("Structure not found: " << filename);
    return nullptr;
}

void NMRStructureModel::sortCompoundConformers(const QModelIndex& compoundIndex)
{
    if (!compoundIndex.isValid())
        return;

    NMRTreeItem* compoundItem = getItem(compoundIndex);
    if (!compoundItem || compoundItem->type() != NMRTreeItem::CompoundItem)
        return;

    // Sort by energy (column 1)
    NMR_LOG("Sorting conformers for compound: " << compoundItem->data(0).toString());
    beginResetModel(); // Simple approach - reset the model for sorting
    compoundItem->sortChildren(1, Qt::AscendingOrder);
    endResetModel();
}

void NMRStructureModel::setReference(const QModelIndex& index)
{
    if (!index.isValid())
        return;

    NMRTreeItem* item = getItem(index);
    if (item->type() != NMRTreeItem::StructureItem)
        return;

    // If there's already a reference, unset it
    if (m_referenceIndex.isValid()) {
        NMRTreeItem* oldRefItem = getItem(m_referenceIndex);
        oldRefItem->setReference(false);
        emit dataChanged(m_referenceIndex, m_referenceIndex, { Qt::DisplayRole });
    }

    // Set new reference
    item->setReference(true);
    m_referenceIndex = index;
    emit dataChanged(index, index, { Qt::DisplayRole });

    // Emit signal about reference change with index
    int structureIndex = item->getStructureIndex();
    emit referenceChanged(structureIndex);

    NMR_LOG("Reference set to: " << item->data(0).toString() << " (index " << structureIndex << ")");
}

NMRData* NMRStructureModel::getReferenceStructure() const
{
    if (!m_referenceIndex.isValid() || !m_structures)
        return nullptr;

    NMRTreeItem* refItem = getItem(m_referenceIndex);
    int structureIndex = refItem->getStructureIndex();

    if (structureIndex < 0 || structureIndex >= static_cast<int>(m_structures->size()))
        return nullptr;

    return (*m_structures)[structureIndex].get();
}

QModelIndex NMRStructureModel::getReferenceIndex() const
{
    return m_referenceIndex;
}

QStringList NMRStructureModel::getAvailableElements() const
{
    QSet<QString> elements;

    NMR_LOG("Getting available elements");

    if (!m_structures) {
        NMR_LOG("Structures vector is null");
        return QStringList();
    }

    // Iterate through all nucleus items to find elements
    for (int i = 0; i < m_rootItem->childCount(); ++i) {
        auto compoundItem = m_rootItem->child(i);
        NMR_LOG("Checking compound: " << compoundItem->data(0).toString());

        for (int j = 0; j < compoundItem->childCount(); ++j) {
            auto structItem = compoundItem->child(j);
            NMR_LOG("Checking structure: " << structItem->data(0).toString());

            // Get elements directly from NMRData using the structure index
            int structureIndex = structItem->getStructureIndex();
            if (structureIndex >= 0 && structureIndex < static_cast<int>(m_structures->size())) {
                NMRData* data = (*m_structures)[structureIndex].get();
                if (data) {
                    for (const auto& [element, _] : data->shieldings) {
                        elements.insert(element);
                        NMR_LOG("Found element: " << element << " in structure");
                    }
                }
            }

            // Also check nucleus items (as a fallback)
            for (int k = 0; k < structItem->childCount(); ++k) {
                auto nucleusItem = structItem->child(k);
                QString nucleusText = nucleusItem->data(0).toString();
                QString element = nucleusText.section('_', 0, 0);
                elements.insert(element);
                NMR_LOG("Found element from nucleus: " << element);
            }
        }
    }

    QStringList result = elements.values();
    std::sort(result.begin(), result.end());
    NMR_LOG("Available elements: " << result.join(", ") << " (total: " << result.size() << ")");
    return result;
}

void NMRStructureModel::setElementVisibility(const QString& element, bool visible)
{
    NMR_LOG("Setting visibility of element " << element << " to " << visible);

    if (visible) {
        m_hiddenElements.remove(element);
    } else {
        m_hiddenElements.insert(element);
    }

    // Update all nucleus items of this element
    beginResetModel(); // Simplest approach, though not the most efficient

    for (int i = 0; i < m_rootItem->childCount(); ++i) {
        auto compoundItem = m_rootItem->child(i);

        for (int j = 0; j < compoundItem->childCount(); ++j) {
            auto structItem = compoundItem->child(j);

            for (int k = 0; k < structItem->childCount(); ++k) {
                auto nucleusItem = structItem->child(k);
                QString nucleusText = nucleusItem->data(0).toString();
                QString itemElement = nucleusText.section('_', 0, 0);

                if (itemElement == element) {
                    nucleusItem->setData(2, visible);
                }
            }
        }
    }

    endResetModel();
}

bool NMRStructureModel::isElementVisible(const QString& element) const
{
    return !m_hiddenElements.contains(element);
}

/**
 * Constructor for NMR Spectrum Dialog
 * Initializes the UI components and connects signals to slots
 * @param parent The parent widget
 */
NMRSpectrumDialog::NMRSpectrumDialog(QWidget* parent)
    : QDialog(parent)
    , m_plotPoints(100000)
    , m_lineWidth(0.1)
    , m_kBoltzmann(0.001987204258) // kcal/(mol·K)
    , m_temperature(298.15) // K
{
    // Setup UI
    setWindowTitle(tr("NMR Spektren Analyse"));
    setMinimumSize(800, 600);

    setupUI();

    // Übergebe Structures-Vektor an das Model
    m_structureModel->setStructuresVector(&m_structures);

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
    m_structureModel = new NMRStructureModel(this);
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
    m_updateTimer->setInterval(500); // 500 ms Verzögerung
    NMR_LOG("UI setup completed");
}

/**
 * Connects signals and slots
 */
void NMRSpectrumDialog::connectSignals()
{
    // Structure actions
    connect(m_addStructureButton, &QPushButton::clicked, this, &NMRSpectrumDialog::selectStructureFiles);
    connect(m_setReferenceButton, &QPushButton::clicked, this, &NMRSpectrumDialog::setAsReference);

    // Selection change in tree view - enable/disable reference button
    connect(m_structureView->selectionModel(), &QItemSelectionModel::selectionChanged,
        [this](const QItemSelection& selected, const QItemSelection&) {
            if (selected.isEmpty()) {
                m_setReferenceButton->setEnabled(false);
                return;
            }

            QModelIndex index = selected.indexes().first();
            NMRTreeItem::ItemType type = static_cast<NMRTreeItem::ItemType>(
                m_structureModel->data(index, NMRStructureModel::ItemTypeRole).toInt());

            // Only enable for structure items
            m_setReferenceButton->setEnabled(type == NMRTreeItem::StructureItem);
        });

    // Model data changes
    connect(m_structureModel, &QAbstractItemModel::dataChanged, this, &NMRSpectrumDialog::handleDataChanged);
    connect(m_structureModel, &NMRStructureModel::referenceChanged, this, &NMRSpectrumDialog::onReferenceChanged);

    // Configuration changes
    connect(m_maxPoints, QOverload<int>::of(&QSpinBox::valueChanged), this, &NMRSpectrumDialog::setPlotPoints);
    connect(m_lineWidthBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NMRSpectrumDialog::setLineWidth);

    // Action buttons
    connect(m_generateButton, &QPushButton::clicked, this, &NMRSpectrumDialog::generateSpectrum);
    connect(m_exportButton, &QPushButton::clicked, this, &NMRSpectrumDialog::exportData);
    connect(m_clearButton, &QPushButton::clicked, this, &NMRSpectrumDialog::clearData);

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

    // Get available elements from model
    QStringList elements = m_structureModel->getAvailableElements();

    NMR_LOG("Setting up element filters with " << elements.size() << " elements");

    if (elements.isEmpty()) {
        auto label = new QLabel(tr("Keine Elemente verfügbar"), m_elementFilterBox);
        m_elementFilterBox->layout()->addWidget(label);
        return;
    }

    // Create checkbox for each element
    for (const QString& element : elements) {
        auto checkbox = new QCheckBox(element, m_elementFilterBox);
        // checkbox->setChecked(m_structureModel->isElementVisible(element));
        checkbox->setChecked(element == "H");
        checkbox->setObjectName(element); // Store element in object name

        connect(checkbox, &QCheckBox::stateChanged, this, &NMRSpectrumDialog::elementFilterChanged);

        m_elementFilterBox->layout()->addWidget(checkbox);
        NMR_LOG("Added checkbox for element: " << element);
    }
}

/**
 * Updates element filter checkboxes
 */
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

    m_structureModel->setElementVisibility(element, visible);

    // Regenerate spectrum with updated visibility
    generateSpectrum();
}

/**
 * Handles data changes in the model
 */
void NMRSpectrumDialog::handleDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    NMR_LOG("Data changed from (" << topLeft.row() << "," << topLeft.column()
                                  << ") to (" << bottomRight.row() << "," << bottomRight.column() << ")");

    // Check if visibility column changed
    if (topLeft.column() <= 2 && bottomRight.column() >= 2) {
        NMR_LOG("Visibility changed for item at row " << topLeft.row());

        // Get the checked state
        QVariant checkState = m_structureModel->data(topLeft.sibling(topLeft.row(), 2), Qt::CheckStateRole);
        bool checked = checkState.toInt() == Qt::Checked;

        // Update children recursively
        // updateVisibilityRecursive(topLeft.sibling(topLeft.row(), 0), checked);

        // Regenerate spectrum with updated visibility
        generateSpectrum();
    }

    // Check if scale factor column changed
    else if (topLeft.column() <= 3 && bottomRight.column() >= 3) {
        NMR_LOG("Scale factor changed for item at row " << topLeft.row());

        // Regenerate spectrum with updated scale factor
        generateSpectrum();
    }
}

/**
 * Handles reference change
 */
void NMRSpectrumDialog::onReferenceChanged(int referenceIndex)
{
    NMR_LOG("Reference changed to index: " << referenceIndex);

    if (referenceIndex < 0 || referenceIndex >= static_cast<int>(m_structures.size())) {
        NMR_LOG("Invalid reference index");
        return;
    }

    NMRData* newReference = m_structures[referenceIndex].get();

    if (!newReference) {
        NMR_LOG("New reference is null");
        return;
    }

    NMR_LOG("Reference changed to " << newReference->name);

    // Calculate reference shieldings
    calculateReferenceShieldings(*newReference);

    // Regenerate spectrum with the new reference
    generateSpectrum();
}

/**
 * Updates visibility of child items recursively
 */
void NMRSpectrumDialog::updateVisibilityRecursive(const QModelIndex& index, bool checked)
{
    if (!index.isValid())
        return;

    NMR_LOG("Updating visibility recursively for " << m_structureModel->data(index, Qt::DisplayRole).toString() << " to " << checked);

    // Update this item's visibility
    m_structureModel->setData(index.sibling(index.row(), 2), checked ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);

    // Update all children
    int rows = m_structureModel->rowCount(index);
    for (int i = 0; i < rows; ++i) {
        updateVisibilityRecursive(m_structureModel->index(i, 0, index), checked);
    }
}

/**
 * Selects structure files when the add structure button is clicked
 */
void NMRSpectrumDialog::selectStructureFiles()
{
    QStringList filenames = QFileDialog::getOpenFileNames(this,
        tr("Strukturen wählen"),
        QString(),
        tr("ORCA Output (*.out);;Alle Dateien (*)"));

    for (const QString& filename : filenames) {
        addStructure(filename, QFileInfo(filename).fileName());
    }

    // Update element filters after adding structures
    updateElementFilters();
}

/**
 * Sets a structure as reference
 */
void NMRSpectrumDialog::setAsReference()
{
    QModelIndex index = m_structureView->selectionModel()->currentIndex();
    if (!index.isValid())
        return;

    // Get the structure item from the selected row
    QModelIndex structIndex = index;
    while (structIndex.isValid() && m_structureModel->data(structIndex, NMRStructureModel::ItemTypeRole).toInt() != NMRTreeItem::StructureItem) {
        structIndex = structIndex.parent();
    }

    if (!structIndex.isValid())
        return;

    m_structureModel->setReference(structIndex);
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

/**
 * Derives a molecular formula from an NMR data structure
 * @param data The NMR data structure
 * @return A string containing the molecular formula
 */
QString NMRSpectrumDialog::deriveMolecularFormula(const NMRData& data)
{
    // Count elements
    std::map<QString, int> elementCounts;

    for (const auto& [element, shieldings] : data.shieldings) {
        elementCounts[element] += shieldings.size();
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

    NMR_LOG("Derived formula: " << formula << " for " << data.name);
    return formula;
}

/**
 * Calculates reference shieldings for each element
 * @param reference The reference structure
 */
void NMRSpectrumDialog::calculateReferenceShieldings(NMRData& reference)
{
    std::map<QString, int> count;

    // Clear existing reference values
    reference.reference.clear();

    // Sum up all shieldings per element
    for (const auto& [element, shieldings] : reference.shieldings) {
        for (const auto& [nucIndex, shielding, aniso] : shieldings) {
            reference.reference[element] += shielding;
            count[element]++;
        }
    }

    // Calculate average shielding per element
    for (auto& [element, shielding] : reference.reference) {
        shielding /= count[element];
        NMR_LOG("Reference shielding for " << element << ": " << shielding);
    }
}

/**
 * Adds a structure to the analysis
 * @param filename The filename of the structure
 * @param name The display name for the structure
 */
void NMRSpectrumDialog::addStructure(const QString& filename, const QString& name)
{
    try {
        NMR_LOG("Adding structure: " << filename);

        // Parse the structure
        auto data = parseOrcaOutput(filename, name);

        // Calculate formula
        QString formula = deriveMolecularFormula(data);
        NMR_LOG("Formula calculated: " << formula);

        // Store the structure and get its index
        m_structures.push_back(std::make_unique<NMRData>(std::move(data)));
        int structureIndex = m_structures.size() - 1;

        // Add to model with index
        QModelIndex structIndex = m_structureModel->addStructure(structureIndex, formula);

        // Check if this is TMS and should be set as reference
        if (isTMS(*m_structures[structureIndex])) {
            NMR_LOG("TMS detected, setting as reference");
            m_structureModel->setReference(structIndex);
        } else if (!m_structureModel->getReferenceStructure()) {
            // If no reference is set yet, use this as the reference
            NMR_LOG("No reference set, using this structure as reference");
            m_structureModel->setReference(structIndex);
        }

        // Handle conformer classification
        handleConformerClassification(m_structures[structureIndex].get());

        // Expand the structure's parent
        m_structureView->expand(structIndex.parent());

        // Update element filters AFTER structure is fully added
        updateElementFilters();

    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Fehler"),
            tr("Fehler beim Hinzufügen der Struktur: %1").arg(e.what()));
        NMR_LOG("Error adding structure: " << e.what());
    }
}

/**
 * Handles classification and organization of conformers
 * @param newStruct The new structure to check
 */
void NMRSpectrumDialog::handleConformerClassification(NMRData* newStruct)
{
    if (!newStruct) {
        NMR_LOG("Null structure for conformer classification");
        return;
    }

    NMR_LOG("Handling conformer classification for " << newStruct->name);

    // Find compound group for this structure
    QString formula = deriveMolecularFormula(*newStruct);
    QModelIndex compoundIndex = m_structureModel->findOrCreateCompound(formula);

    // Sort conformers by energy
    m_structureModel->sortCompoundConformers(compoundIndex);

    NMR_LOG("Conformer classification completed for " << newStruct->name);
}

/**
 * Checks if two structures are different conformations of the same compound
 * @param str1 The first structure
 * @param str2 The second structure
 * @return true if the structures are conformers of each other
 */
bool NMRSpectrumDialog::isConformation(const NMRData& str1, const NMRData& str2)
{
    // Check if both structures have the same elements
    if (str1.shieldings.size() != str2.shieldings.size()) {
        return false;
    }

    // Check each element
    for (const auto& [element1, shieldings1] : str1.shieldings) {
        // Check if element exists in str2
        auto it = str2.shieldings.find(element1);
        if (it == str2.shieldings.end()) {
            return false;
        }

        const auto& shieldings2 = it->second;

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

/**
 * Checks if a structure is TMS based on formula
 * @param data The structure to check
 * @return true if it's TMS
 */
bool NMRSpectrumDialog::isTMS(const NMRData& data)
{
    QString formula = deriveMolecularFormula(data);

    // Check for Si(CH3)4 or Si1C4H12 formula
    return (formula == "C4H12Si" || formula == "Si1C4H12" || formula.contains("TMS", Qt::CaseInsensitive));
}

/**
 * Parses an ORCA output file to extract NMR data
 * @param filename The filename to parse
 * @param name The name to assign to the data
 * @return The extracted NMR data
 */
NMRData NMRSpectrumDialog::parseOrcaOutput(const QString& filename, const QString& name)
{
    NMR_LOG("Parsing ORCA output file: " << filename);

    NMRData data;
    data.name = name;
    data.filename = filename;
    data.isReference = false;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        NMR_LOG("Failed to open file: " << filename);
        throw std::runtime_error("Datei konnte nicht geöffnet werden");
    }

    QTextStream in(&file);
    QString content = in.readAll();

    // Extract energy
    data.energy = extractEnergy(content);
    NMR_LOG("Extracted energy: " << data.energy);

    // Parse NMR shieldings
    parseNMRShieldings(content, data);

    NMR_LOG("Parsing completed successfully");
    return data;
}

/**
 * Parses NMR shielding data from ORCA output
 * @param content The content of the ORCA output file
 * @param data The NMRData structure to populate
 */
void NMRSpectrumDialog::parseNMRShieldings(const QString& content, NMRData& data)
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

        int nucleus = match.captured(1).toInt();
        QString element = match.captured(2);
        double shielding = match.captured(3).toDouble();
        double anisotropy = match.captured(4).toDouble();

        data.shieldings[element].emplace_back(nucleus, shielding, anisotropy);
        NMR_LOG("Added shielding: " << element << "_" << nucleus << " = " << shielding);
    }

    NMR_LOG("Found " << data.shieldings.size() << " elements with shieldings");
}

/**
 * Extracts the energy from ORCA output
 * @param content The content of the ORCA output file
 * @return The extracted energy value in Hartree
 */
double NMRSpectrumDialog::extractEnergy(const QString& content)
{
    QRegularExpression rx("FINAL SINGLE POINT ENERGY\\s+(-?\\d+\\.\\d+)");
    auto match = rx.match(content);
    if (match.hasMatch()) {
        return match.captured(1).toDouble();
    }

    NMR_LOG("Failed to extract energy from file");
    throw std::runtime_error("Energie nicht gefunden");
}

/**
 * Gets visible structures from the model
 * @return Vector of pointers to visible structures
 */
std::vector<NMRData*> NMRSpectrumDialog::getVisibleStructures()
{
    std::vector<NMRData*> visibleStructures;

    NMR_LOG("Getting visible structures");

    // Iterate through all compounds
    for (int i = 0; i < m_structureModel->rowCount(); ++i) {
        QModelIndex compoundIndex = m_structureModel->index(i, 0);
        QVariant compoundVisible = m_structureModel->data(
            compoundIndex.sibling(compoundIndex.row(), 2), Qt::CheckStateRole);

        NMR_LOG("Compound: " << m_structureModel->data(compoundIndex, Qt::DisplayRole).toString()
                             << " visible: " << (compoundVisible.toInt() == Qt::Checked));

        // Skip invisible compounds
        if (compoundVisible.toInt() != Qt::Checked) {
            continue;
        }

        // Iterate through structures in this compound
        for (int j = 0; j < m_structureModel->rowCount(compoundIndex); ++j) {
            QModelIndex structIndex = m_structureModel->index(j, 0, compoundIndex);
            QVariant structVisible = m_structureModel->data(
                structIndex.sibling(structIndex.row(), 2), Qt::CheckStateRole);

            NMR_LOG("  Structure: " << m_structureModel->data(structIndex, Qt::DisplayRole).toString()
                                    << " visible: " << (structVisible.toInt() == Qt::Checked));

            // Skip invisible structures
            if (structVisible.toInt() != Qt::Checked) {
                continue;
            }

            // Get NMRData using index
            NMRData* data = getStructureFromIndex(structIndex);

            if (data) {
                NMR_LOG("  Adding visible structure: " << data->name);
                visibleStructures.push_back(data);
            }
        }
    }

    NMR_LOG("Found " << visibleStructures.size() << " visible structures");
    return visibleStructures;
}

/**
 * Gets the structure from the model based on the index
 * @param index The index of the structure in the model
 * @return Pointer to the NMRData structure
 */
NMRData* NMRSpectrumDialog::getStructureFromIndex(const QModelIndex& index)
{
    if (!index.isValid())
        return nullptr;

    int structureIndex = m_structureModel->getStructureIndex(index);
    if (structureIndex < 0 || structureIndex >= static_cast<int>(m_structures.size()))
        return nullptr;

    return m_structures[structureIndex].get();
}

/**
 * Generates the NMR spectrum based on loaded structures
 */
void NMRSpectrumDialog::generateSpectrum()
{
    NMR_LOG("Generating spectrum");

    if (m_structures.empty()) {
        NMR_LOG("No structures available");
        return; // No warning message if no structures are loaded
    }

    // Get reference structure
    NMRData* reference = m_structureModel->getReferenceStructure();
    if (!reference) {
        QMessageBox::warning(this, tr("Keine Referenz"),
            tr("Bitte wählen Sie eine Referenzstruktur."));
        NMR_LOG("No reference structure set");
        return;
    }

    m_shiftTable->setRowCount(0);
    m_chart->Clear();

    // Collect visible structures from model
    std::vector<NMRData*> visibleStructures = getVisibleStructures();
    if (visibleStructures.empty()) {
        NMR_LOG("No visible structures");
        return; // No warning if no visible structures
    }

    // Organize structures by compound (formula)
    std::map<QString, std::vector<NMRData*>> compoundStructures = organizeStructuresByCompound(visibleStructures);

    // Process structures by compound and collect shifts
    std::vector<ShiftData> allShifts;
    std::map<QString, std::map<QString, std::vector<double>>> compoundElementShifts;
    std::map<QString, double> scalingfactors;
    for (const auto& [compound, structures] : compoundStructures) {
        NMR_LOG("Processing compound: " << compound << " with " << structures.size() << " structures");

        // Calculate Boltzmann weights for this compound's structures
        std::vector<double> energies;
        for (const auto& structure : structures) {
            energies.push_back(structure->energy);
        }
        auto weights = calculateBoltzmannWeights(energies);

        // Process each structure in this compound
        for (size_t i = 0; i < structures.size(); ++i) {
            const auto& structure = structures[i];
            double weight = weights[i];

            NMR_LOG("  Structure: " << structure->name << " weight: " << weight);

            for (const auto& [element, shieldings] : structure->shieldings) {
                // Skip hidden elements
                if (!m_structureModel->isElementVisible(element)) {
                    NMR_LOG("    Element " << element << " is hidden, skipping");
                    continue;
                }

                // Get reference shielding for this element
                double refShielding = reference->reference[element];

                for (const auto& [nucIndex, shielding, aniso] : shieldings) {
                    double shift = shielding - refShielding; // refShielding - shielding; // Note: calculate as ref - sample

                    NMR_LOG("    Nucleus: " << element << "_" << nucIndex
                                            << " ref: " << refShielding << " shield: " << shielding << " shift: " << shift);

                    ShiftData data{
                        element,
                        nucIndex,
                        refShielding,
                        shielding,
                        shift,
                        weight
                    };
                    allShifts.push_back(data);
                    compoundElementShifts[compound][element].push_back(shift);
                }
                scalingfactors[compound] = structure->scaleFactor;
                NMR_LOG("    Scaling factor for " << compound << ": " << scalingfactors[compound]);
            }
        }
    }

    // Update table with all shift data
    updateTable(allShifts);

    // Generate spectrum with separate series for each compound
    updatePlotByCompound(compoundElementShifts, scalingfactors);

    NMR_LOG("Spectrum generation complete");
}

/**
 * Calculates Boltzmann weights based on energies
 * @param energies The energies in Hartree
 * @return Vector of Boltzmann weights
 */
std::vector<double> NMRSpectrumDialog::calculateBoltzmannWeights(const std::vector<double>& energies)
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

/**
 * Organizes structures by compound name
 * @param structures Vector of structure pointers
 * @return Map of compound names to vectors of structure pointers
 */
std::map<QString, std::vector<NMRData*>> NMRSpectrumDialog::organizeStructuresByCompound(
    const std::vector<NMRData*>& structures)
{
    std::map<QString, std::vector<NMRData*>> result;

    for (const auto& structure : structures) {
        QString formula = deriveMolecularFormula(*structure);
        result[formula].push_back(structure);
    }

    return result;
}

/**
 * Updates the table with shift data
 * @param shifts Vector of shift data to display
 */
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

/**
 * Updates the plot with separate series for each compound
 * @param compoundElementShifts Map of compounds to their element shifts
 */
void NMRSpectrumDialog::updatePlotByCompound(
    const std::map<QString, std::map<QString, std::vector<double>>>& compoundElementShifts, const std::map<QString, double>& scalingfactors)
{
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

    // Calculate global plot range
    auto [xMin, xMax] = calculatePlotRange(compoundElementShifts);

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

            double scaleFactor = 1.0; // Standardwert

            // Suche nach dem Skalierungsfaktor für dieses Compound
            auto scaleFactorIt = scalingfactors.find(compound);
            if (scaleFactorIt != scalingfactors.end()) {
                scaleFactor = scaleFactorIt->second;
            }
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
                stickSeries->append(shift, 1.0);
                stickSeries->append(shift, 0.0);
            }

            m_chart->addSeries(stickSeries, seriesIndex, blendedColor, seriesName + "_stick", true);

            seriesIndex++;
        }
    }
    // m_chart->Chart()->setXRange(xMax,xMin);
}

/**
 * Calculates the plot range for all shifts
 * @param compoundElementShifts Map of compounds to their element shifts
 * @return Pair of minimum and maximum x values
 */
std::pair<double, double> NMRSpectrumDialog::calculatePlotRange(
    const std::map<QString, std::map<QString, std::vector<double>>>& compoundElementShifts)
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
    const double padding = 5.0;
    return { xMin - padding, xMax + padding };
}

/**
 * Exports data to a CSV file
 */
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

    NMR_LOG("Data exported to " << filename);
}

/**
 * Clears all data
 */
void NMRSpectrumDialog::clearData()
{
    if (QMessageBox::question(this, tr("Daten löschen"),
            tr("Möchten Sie wirklich alle Daten löschen?"),
            QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::No) {
        return;
    }

    NMR_LOG("Clearing all data");

    m_structures.clear();
    m_structureModel->clearModel();
    m_shiftTable->setRowCount(0);
    m_chart->Clear();

    // Update element filters
    updateElementFilters();

    NMR_LOG("All data cleared");
}
