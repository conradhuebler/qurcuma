// nmrstructureproxymodel.h
#ifndef NMRSTRUCTUREPROXYMODEL_H
#define NMRSTRUCTUREPROXYMODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QString>
#include <memory>
#include <vector>

class NMRDataStore;

// Debug-Makros zur einfachen Fehlersuche
#define NMR_DEBUG 1

#if NMR_DEBUG
#include <QDebug>
#define NMR_LOG(msg) qDebug() << "[NMRProxyModel] " << msg
#else
#define NMR_LOG(msg)
#endif

class NMRStructureProxyModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Roles {
        ItemTypeRole = Qt::UserRole + 1,
        StructureIndexRole,
        NucleusIndexRole,
        ElementRole,
        FormulaRole,
        ReferenceRole,
        ScaleFactorRole
    };

    enum ItemType {
        RootItem,
        CompoundItem,
        StructureItem,
        ElementGroupItem,
        NucleusItem
    };

    explicit NMRStructureProxyModel(NMRDataStore* dataStore, QObject* parent = nullptr);
    ~NMRStructureProxyModel();

    // QAbstractItemModel implementation
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Proxy model specific methods
    QModelIndex getCompoundIndex(const QString& formula) const;
    QModelIndex getStructureIndex(int structureIndex) const;
    QModelIndex getElementGroupIndex(const QModelIndex& structureIndex, const QString& element) const;
    QModelIndex getNucleusIndex(const QModelIndex& elementGroupIndex, int nucleusIndex) const;

    int getStructureIndex(const QModelIndex& index) const;
    QString getCompoundFormula(const QModelIndex& index) const;
    QString getElementSymbol(const QModelIndex& index) const;
    int getNucleusIndex(const QModelIndex& index) const;
    ItemType getItemType(const QModelIndex& index) const;

    // Reference management
    QModelIndex getReferenceIndex() const;
    void setReference(const QModelIndex& index);

signals:
    void doubleClickItemAction(const QModelIndex& index);

private slots:
    void handleDataStoreChanged();
    void handleStructureAdded(int structureIndex);
    void handleStructureRemoved(int structureIndex);
    void handleStructureChanged(int structureIndex);
    void handleReferenceChanged(int structureIndex);

private:
    struct TreeItem {
        TreeItem(TreeItem* parent = nullptr, ItemType type = RootItem);
        ~TreeItem();

        QString displayText;
        TreeItem* parent = nullptr;
        std::vector<std::unique_ptr<TreeItem>> children;
        ItemType type = RootItem;
        int structureIndex = -1;
        int nucleusIndex = -1;
        QString formula;
        QString element;
        double energy = 0.0;
        bool visible = true;
        double scaleFactor = 1.0;
        bool isReference = false;
    };

    NMRDataStore* m_dataStore;
    std::unique_ptr<TreeItem> m_rootItem;
    QStringList m_headerLabels;

    // Helper methods
    TreeItem* getItem(const QModelIndex& index) const;
    void rebuildModel();
    QModelIndex createIndexForItem(TreeItem* item, int column) const;
    void updateStructureItem(TreeItem* structureItem, int structureIndex);
    TreeItem* findStructureItem(int structureIndex) const;
    TreeItem* findCompoundItem(const QString& formula) const;
    QModelIndex getIndexForStructure(int structureIndex) const;
};

#endif // NMRSTRUCTUREPROXYMODEL_H
