// nmrstructureproxymodel.cpp
#include "nmrstructureproxymodel.h"
#include "nmrdatastore.h"
#include <QFileInfo>

NMRStructureProxyModel::TreeItem::TreeItem(TreeItem* parent, ItemType type)
    : parent(parent)
    , type(type)
{
}

NMRStructureProxyModel::TreeItem::~TreeItem()
{
}

NMRStructureProxyModel::NMRStructureProxyModel(NMRDataStore* dataStore, QObject* parent)
    : QAbstractItemModel(parent)
    , m_dataStore(dataStore)
{
    // Setup header labels
    m_headerLabels << tr("Struktur") << tr("Energie (Hartree)") << tr("Sichtbar") << tr("Skalierung");

    // Create root item
    m_rootItem = std::make_unique<TreeItem>(nullptr, RootItem);
    m_rootItem->displayText = "Root";

    // Connect to datastore signals
    connect(m_dataStore, &NMRDataStore::dataChanged, this, &NMRStructureProxyModel::handleDataStoreChanged);
    connect(m_dataStore, &NMRDataStore::structureAdded, this, &NMRStructureProxyModel::handleStructureAdded);
    connect(m_dataStore, &NMRDataStore::structureRemoved, this, &NMRStructureProxyModel::handleStructureRemoved);
    connect(m_dataStore, &NMRDataStore::structureChanged, this, &NMRStructureProxyModel::handleStructureChanged);
    connect(m_dataStore, &NMRDataStore::referenceChanged, this, &NMRStructureProxyModel::handleReferenceChanged);

    // Build the initial model
    rebuildModel();

    NMR_LOG("Proxy model created");
}

NMRStructureProxyModel::~NMRStructureProxyModel()
{
    NMR_LOG("Proxy model destroyed");
}

QModelIndex NMRStructureProxyModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    TreeItem* parentItem = getItem(parent);
    if (!parentItem || row < 0 || static_cast<size_t>(row) >= parentItem->children.size())
        return QModelIndex();

    TreeItem* childItem = parentItem->children[row].get();
    return createIndex(row, column, childItem);
}

QModelIndex NMRStructureProxyModel::parent(const QModelIndex& index) const
{
    if (!index.isValid())
        return QModelIndex();

    TreeItem* childItem = getItem(index);
    if (!childItem || !childItem->parent || childItem->parent == m_rootItem.get())
        return QModelIndex();

    TreeItem* parentItem = childItem->parent;
    TreeItem* grandparentItem = parentItem->parent;

    if (!grandparentItem)
        return QModelIndex();

    for (size_t i = 0; i < grandparentItem->children.size(); ++i) {
        if (grandparentItem->children[i].get() == parentItem) {
            return createIndex(i, 0, parentItem);
        }
    }

    return QModelIndex();
}

int NMRStructureProxyModel::rowCount(const QModelIndex& parent) const
{
    TreeItem* parentItem = getItem(parent);
    if (!parentItem)
        return 0;

    return static_cast<int>(parentItem->children.size());
}

int NMRStructureProxyModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return m_headerLabels.size();
}

QVariant NMRStructureProxyModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    TreeItem* item = getItem(index);
    if (!item)
        return QVariant();

    // Different roles
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case 0: {
            if (item->isReference && item->type == StructureItem) {
                return QString("%1 [Referenz]").arg(item->displayText);
            }
            return item->displayText;
        }
        case 1: {
            if (item->type == StructureItem) {
                return QString::number(item->energy, 'f', 6);
            }
            return QVariant();
        }
        case 2: // For visibility, only checkstate is shown
            return QVariant();
        case 3: { // Scale factor
            if (item->type == CompoundItem) {
                return item->scaleFactor;
            }
            return QVariant();
        }
        }
    } else if (role == Qt::CheckStateRole && index.column() == 2) {
        return item->visible ? Qt::Checked : Qt::Unchecked;
    } else if (role == ItemTypeRole) {
        return static_cast<int>(item->type);
    } else if (role == StructureIndexRole) {
        return item->structureIndex;
    } else if (role == NucleusIndexRole) {
        return item->nucleusIndex;
    } else if (role == ElementRole) {
        return item->element;
    } else if (role == FormulaRole) {
        return item->formula;
    } else if (role == ReferenceRole) {
        return item->isReference;
    } else if (role == ScaleFactorRole) {
        return item->scaleFactor;
    }

    return QVariant();
}

bool NMRStructureProxyModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return false;

    TreeItem* item = getItem(index);
    if (!item)
        return false;

    if (role == Qt::CheckStateRole && index.column() == 2) {
        bool checked = value.toInt() == Qt::Checked;

        switch (item->type) {
        case CompoundItem: {
            // Toggle visibility of all structures in this compound
            item->visible = checked;
            for (const auto& child : item->children) {
                if (child->type == StructureItem && child->structureIndex >= 0) {
                    m_dataStore->setStructureVisible(child->structureIndex, checked);
                }
            }
            break;
        }
        case StructureItem: {
            // Toggle visibility of this structure
            item->visible = checked;
            if (item->structureIndex >= 0) {
                m_dataStore->setStructureVisible(item->structureIndex, checked);
            }
            break;
        }
        case ElementGroupItem: {
            // Toggle visibility of this element
            item->visible = checked;
            m_dataStore->setAllNucleiVisible(item->element, checked);
            break;
        }
        case NucleusItem: {
            // Toggle visibility of this nucleus
            item->visible = checked;
            if (item->parent && item->parent->parent) {
                TreeItem* structureItem = item->parent->parent;
                if (structureItem->type == StructureItem && structureItem->structureIndex >= 0) {
                    m_dataStore->setNucleusVisible(structureItem->structureIndex, item->element, item->nucleusIndex, checked);
                }
            }
            break;
        }
        default:
            return false;
        }

        emit dataChanged(index, index, { role });
        return true;
    } else if (role == Qt::EditRole && index.column() == 3) {
        // Set scale factor for compound
        if (item->type == CompoundItem) {
            bool ok;
            double factor = value.toDouble(&ok);
            if (!ok || factor <= 0)
                return false;

            item->scaleFactor = factor;
            m_dataStore->setCompoundScaleFactor(item->formula, factor);

            emit dataChanged(index, index, { Qt::EditRole, Qt::DisplayRole });
            return true;
        }
    }

    return false;
}

Qt::ItemFlags NMRStructureProxyModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    TreeItem* item = getItem(index);
    if (!item)
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    // Make column 2 (Visible) checkable
    if (index.column() == 2) {
        flags |= Qt::ItemIsUserCheckable;
    }

    // Make scale factor editable for compound items
    if (index.column() == 3 && item->type == CompoundItem) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

QVariant NMRStructureProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        return m_headerLabels.value(section);
    }

    return QVariant();
}

QModelIndex NMRStructureProxyModel::getCompoundIndex(const QString& formula) const
{
    TreeItem* item = findCompoundItem(formula);
    if (item) {
        return createIndexForItem(item, 0);
    }
    return QModelIndex();
}

QModelIndex NMRStructureProxyModel::getStructureIndex(int structureIndex) const
{
    TreeItem* item = findStructureItem(structureIndex);
    if (item) {
        return createIndexForItem(item, 0);
    }
    return QModelIndex();
}

QModelIndex NMRStructureProxyModel::getElementGroupIndex(const QModelIndex& structureIndex, const QString& element) const
{
    TreeItem* structureItem = getItem(structureIndex);
    if (!structureItem || structureItem->type != StructureItem) {
        return QModelIndex();
    }

    for (size_t i = 0; i < structureItem->children.size(); ++i) {
        TreeItem* child = structureItem->children[i].get();
        if (child->type == ElementGroupItem && child->element == element) {
            return createIndex(i, 0, child);
        }
    }

    return QModelIndex();
}

QModelIndex NMRStructureProxyModel::getNucleusIndex(const QModelIndex& elementGroupIndex, int nucleusIndex) const
{
    TreeItem* elementGroupItem = getItem(elementGroupIndex);
    if (!elementGroupItem || elementGroupItem->type != ElementGroupItem) {
        return QModelIndex();
    }

    for (size_t i = 0; i < elementGroupItem->children.size(); ++i) {
        TreeItem* child = elementGroupItem->children[i].get();
        if (child->type == NucleusItem && child->nucleusIndex == nucleusIndex) {
            return createIndex(i, 0, child);
        }
    }

    return QModelIndex();
}

int NMRStructureProxyModel::getStructureIndex(const QModelIndex& index) const
{
    TreeItem* item = getItem(index);
    if (!item) {
        return -1;
    }

    // If this is a structure item, return its index directly
    if (item->type == StructureItem) {
        return item->structureIndex;
    }

    // If this is an element group or nucleus, find the parent structure
    if (item->type == ElementGroupItem && item->parent) {
        return item->parent->structureIndex;
    }

    if (item->type == NucleusItem && item->parent && item->parent->parent) {
        return item->parent->parent->structureIndex;
    }

    return -1;
}

QString NMRStructureProxyModel::getCompoundFormula(const QModelIndex& index) const
{
    TreeItem* item = getItem(index);
    if (!item) {
        return QString();
    }

    // If this is a compound item, return its formula directly
    if (item->type == CompoundItem) {
        return item->formula;
    }

    // If this is a structure, element group, or nucleus, find the parent compound
    if (item->type == StructureItem && item->parent) {
        return item->parent->formula;
    }

    if (item->type == ElementGroupItem && item->parent && item->parent->parent) {
        return item->parent->parent->formula;
    }

    if (item->type == NucleusItem && item->parent && item->parent->parent && item->parent->parent->parent) {
        return item->parent->parent->parent->formula;
    }

    return QString();
}

QString NMRStructureProxyModel::getElementSymbol(const QModelIndex& index) const
{
    TreeItem* item = getItem(index);
    if (!item) {
        return QString();
    }

    if (item->type == ElementGroupItem) {
        return item->element;
    }

    if (item->type == NucleusItem) {
        return item->element;
    }

    return QString();
}

int NMRStructureProxyModel::getNucleusIndex(const QModelIndex& index) const
{
    TreeItem* item = getItem(index);
    if (item && item->type == NucleusItem) {
        return item->nucleusIndex;
    }

    return -1;
}

NMRStructureProxyModel::ItemType NMRStructureProxyModel::getItemType(const QModelIndex& index) const
{
    TreeItem* item = getItem(index);
    if (item) {
        return item->type;
    }

    return RootItem;
}

QModelIndex NMRStructureProxyModel::getReferenceIndex() const
{
    int referenceIndex = m_dataStore->getReferenceIndex();
    if (referenceIndex >= 0) {
        return getIndexForStructure(referenceIndex);
    }

    return QModelIndex();
}

void NMRStructureProxyModel::setReference(const QModelIndex& index)
{
    TreeItem* item = getItem(index);
    if (!item || item->type != StructureItem) {
        return;
    }

    m_dataStore->setReference(item->structureIndex);
}

void NMRStructureProxyModel::handleDataStoreChanged()
{
    NMR_LOG("DataStore changed, rebuilding model");
    rebuildModel();
}

void NMRStructureProxyModel::handleStructureAdded(int structureIndex)
{
    NMR_LOG("Structure added with index: " << structureIndex);

    const NMRDataStore::NMRStructure* structure = m_dataStore->getStructure(structureIndex);
    if (!structure) {
        return;
    }

    // Find or create compound item
    TreeItem* compoundItem = findCompoundItem(structure->formula);
    if (!compoundItem) {
        // Create new compound item
        beginInsertRows(QModelIndex(), m_rootItem->children.size(), m_rootItem->children.size());

        auto newCompoundItem = std::make_unique<TreeItem>(m_rootItem.get(), CompoundItem);
        newCompoundItem->displayText = structure->formula;
        newCompoundItem->formula = structure->formula;
        newCompoundItem->visible = true;
        newCompoundItem->scaleFactor = 1.0;

        compoundItem = newCompoundItem.get();
        m_rootItem->children.push_back(std::move(newCompoundItem));

        endInsertRows();
    }

    // Add structure item to compound
    beginInsertRows(createIndexForItem(compoundItem, 0), compoundItem->children.size(), compoundItem->children.size());

    auto structureItem = std::make_unique<TreeItem>(compoundItem, StructureItem);
    structureItem->displayText = QFileInfo(structure->filename).fileName();
    structureItem->structureIndex = structureIndex;
    structureItem->energy = structure->energy;
    structureItem->visible = structure->visible;
    structureItem->isReference = structure->isReference;

    TreeItem* structItemPtr = structureItem.get();
    compoundItem->children.push_back(std::move(structureItem));

    endInsertRows();

    // Add element groups and nuclei
    for (const auto& [element, nuclei] : structure->nuclei) {
        QModelIndex structIndex = createIndexForItem(structItemPtr, 0);

        // Add element group
        beginInsertRows(structIndex, structItemPtr->children.size(), structItemPtr->children.size());

        auto elementGroupItem = std::make_unique<TreeItem>(structItemPtr, ElementGroupItem);
        elementGroupItem->displayText = element;
        elementGroupItem->element = element;
        elementGroupItem->visible = m_dataStore->isElementVisible(element);

        TreeItem* elementGroupPtr = elementGroupItem.get();
        structItemPtr->children.push_back(std::move(elementGroupItem));

        endInsertRows();

        // Add nuclei to element group
        QModelIndex elementGroupIndex = createIndexForItem(elementGroupPtr, 0);

        beginInsertRows(elementGroupIndex, 0, nuclei.size() - 1);

        for (const auto& nucleus : nuclei) {
            auto nucleusItem = std::make_unique<TreeItem>(elementGroupPtr, NucleusItem);
            nucleusItem->displayText = QString("%1_%2").arg(element).arg(nucleus.index);
            nucleusItem->element = element;
            nucleusItem->nucleusIndex = nucleus.index;
            nucleusItem->visible = nucleus.visible;

            elementGroupPtr->children.push_back(std::move(nucleusItem));
        }

        endInsertRows();
    }

    // Sort compounds by formula
    std::sort(m_rootItem->children.begin(), m_rootItem->children.end(),
        [](const std::unique_ptr<TreeItem>& a, const std::unique_ptr<TreeItem>& b) {
            return a->formula < b->formula;
        });

    // Emit structure changed
    emit dataChanged(createIndexForItem(structItemPtr, 0), createIndexForItem(structItemPtr, columnCount() - 1));
}

void NMRStructureProxyModel::handleStructureRemoved(int structureIndex)
{
    NMR_LOG("Structure removed with index: " << structureIndex);
    rebuildModel(); // Simple approach: just rebuild the entire model
}

void NMRStructureProxyModel::handleStructureChanged(int structureIndex)
{
    NMR_LOG("Structure changed with index: " << structureIndex);

    TreeItem* structureItem = findStructureItem(structureIndex);
    if (!structureItem) {
        return;
    }

    updateStructureItem(structureItem, structureIndex);

    QModelIndex structureIdx = createIndexForItem(structureItem, 0);
    emit dataChanged(structureIdx, createIndexForItem(structureItem, columnCount() - 1));
}

void NMRStructureProxyModel::handleReferenceChanged(int structureIndex)
{
    NMR_LOG("Reference changed to structure with index: " << structureIndex);

    // Update old reference
    for (auto& compoundItem : m_rootItem->children) {
        for (auto& structureItem : compoundItem->children) {
            if (structureItem->type == StructureItem && structureItem->isReference && structureItem->structureIndex != structureIndex) {
                structureItem->isReference = false;
                QModelIndex idx = createIndexForItem(structureItem.get(), 0);
                emit dataChanged(idx, idx);
            }
        }
    }

    // Update new reference
    TreeItem* newRefItem = findStructureItem(structureIndex);
    if (newRefItem) {
        newRefItem->isReference = true;
        QModelIndex idx = createIndexForItem(newRefItem, 0);
        emit dataChanged(idx, idx);
    }
}

NMRStructureProxyModel::TreeItem* NMRStructureProxyModel::getItem(const QModelIndex& index) const
{
    if (index.isValid()) {
        TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
        if (item) {
            return item;
        }
    }

    return m_rootItem.get();
}

void NMRStructureProxyModel::rebuildModel()
{
    beginResetModel();

    // Clear existing items
    m_rootItem->children.clear();

    // Group structures by compound
    std::map<QString, std::vector<int>> compoundStructures;
    for (int i = 0; i < m_dataStore->getStructureCount(); ++i) {
        const auto* structure = m_dataStore->getStructure(i);
        if (structure) {
            compoundStructures[structure->formula].push_back(i);
        }
    }

    // Create compound items
    for (const auto& [formula, structureIndices] : compoundStructures) {
        auto compoundItem = std::make_unique<TreeItem>(m_rootItem.get(), CompoundItem);
        compoundItem->displayText = formula;
        compoundItem->formula = formula;
        compoundItem->visible = true;
        compoundItem->scaleFactor = m_dataStore->getCompoundScaleFactor(formula);

        // Create structure items for this compound
        for (int structureIndex : structureIndices) {
            const auto* structure = m_dataStore->getStructure(structureIndex);
            if (!structure) {
                continue;
            }

            auto structureItem = std::make_unique<TreeItem>(compoundItem.get(), StructureItem);
            structureItem->displayText = QFileInfo(structure->filename).fileName();
            structureItem->structureIndex = structureIndex;
            structureItem->energy = structure->energy;
            structureItem->visible = structure->visible;
            structureItem->isReference = structure->isReference;

            // Create element groups
            for (const auto& [element, nuclei] : structure->nuclei) {
                auto elementGroupItem = std::make_unique<TreeItem>(structureItem.get(), ElementGroupItem);
                elementGroupItem->displayText = element;
                elementGroupItem->element = element;
                elementGroupItem->visible = m_dataStore->isElementVisible(element);

                // Create nucleus items
                for (const auto& nucleus : nuclei) {
                    auto nucleusItem = std::make_unique<TreeItem>(elementGroupItem.get(), NucleusItem);
                    nucleusItem->displayText = QString("%1_%2").arg(element).arg(nucleus.index);
                    nucleusItem->element = element;
                    nucleusItem->nucleusIndex = nucleus.index;
                    nucleusItem->visible = nucleus.visible;

                    elementGroupItem->children.push_back(std::move(nucleusItem));
                }

                structureItem->children.push_back(std::move(elementGroupItem));
            }

            compoundItem->children.push_back(std::move(structureItem));
        }

        // Sort structures by energy
        std::sort(compoundItem->children.begin(), compoundItem->children.end(),
            [](const std::unique_ptr<TreeItem>& a, const std::unique_ptr<TreeItem>& b) {
                return a->energy < b->energy;
            });

        m_rootItem->children.push_back(std::move(compoundItem));
    }

    // Sort compounds alphabetically
    std::sort(m_rootItem->children.begin(), m_rootItem->children.end(),
        [](const std::unique_ptr<TreeItem>& a, const std::unique_ptr<TreeItem>& b) {
            return a->formula < b->formula;
        });

    endResetModel();
}

QModelIndex NMRStructureProxyModel::createIndexForItem(TreeItem* item, int column) const
{
    if (!item || item == m_rootItem.get()) {
        return QModelIndex();
    }

    // Find the row of this item in its parent's children
    TreeItem* parentItem = item->parent;
    if (!parentItem) {
        return QModelIndex();
    }

    for (size_t i = 0; i < parentItem->children.size(); ++i) {
        if (parentItem->children[i].get() == item) {
            return createIndex(i, column, item);
        }
    }

    return QModelIndex();
}

void NMRStructureProxyModel::updateStructureItem(TreeItem* structureItem, int structureIndex)
{
    const auto* structure = m_dataStore->getStructure(structureIndex);
    if (!structure || !structureItem) {
        return;
    }

    // Update structure item properties
    structureItem->visible = structure->visible;
    structureItem->energy = structure->energy;
    structureItem->isReference = structure->isReference;

    // Update compound scale factor
    if (structureItem->parent) {
        TreeItem* compoundItem = structureItem->parent;
        compoundItem->scaleFactor = m_dataStore->getCompoundScaleFactor(compoundItem->formula);
    }
}

NMRStructureProxyModel::TreeItem* NMRStructureProxyModel::findStructureItem(int structureIndex) const
{
    // Search all compounds
    for (const auto& compoundItem : m_rootItem->children) {
        for (const auto& structureItem : compoundItem->children) {
            if (structureItem->type == StructureItem && structureItem->structureIndex == structureIndex) {
                return structureItem.get();
            }
        }
    }

    return nullptr;
}

NMRStructureProxyModel::TreeItem* NMRStructureProxyModel::findCompoundItem(const QString& formula) const
{
    for (const auto& compoundItem : m_rootItem->children) {
        if (compoundItem->type == CompoundItem && compoundItem->formula == formula) {
            return compoundItem.get();
        }
    }

    return nullptr;
}

QModelIndex NMRStructureProxyModel::getIndexForStructure(int structureIndex) const
{
    TreeItem* item = findStructureItem(structureIndex);
    if (item) {
        return createIndexForItem(item, 0);
    }

    return QModelIndex();
}
