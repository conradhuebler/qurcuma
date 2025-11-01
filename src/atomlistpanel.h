// Claude Generated - Phase 2C - Atom List Panel with QTableView
#ifndef ATOMLISTPANEL_H
#define ATOMLISTPANEL_H

#include <QWidget>
#include <QTableView>
#include <QAbstractTableModel>
#include <QVector>
#include <QVector3D>
#include <QString>

/**
 * @brief Table model for displaying atom properties
 *
 * Provides a sortable/filterable table view of atom data:
 * Index, Element, X, Y, Z, Charge
 */
class AtomTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    struct AtomData {
        int index;
        QString element;
        float x, y, z;
        float charge;
    };

    explicit AtomTableModel(QObject *parent = nullptr);

    // QAbstractTableModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Data management
    void setAtomData(const QVector<AtomData>& atoms);
    void clear();
    const QVector<AtomData>& getAtomData() const { return m_atoms; }

private:
    QVector<AtomData> m_atoms;
    static const int COLUMN_COUNT = 6;  // Index, Element, X, Y, Z, Charge
};

/**
 * @brief Panel widget for browsing and selecting atoms
 *
 * Displays atom properties in a table with:
 * - Sortable columns
 * - Selection highlighting (synchronized with 3D viewer)
 * - Right-click context menu (Copy, Export, Focus)
 * - Keyboard shortcuts
 */
class AtomListPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AtomListPanel(QWidget *parent = nullptr);
    ~AtomListPanel();

    // Data management
    void updateAtomList(const QVector<QVector3D>& positions, const QVector<QString>& elements,
                       const QVector<float>& charges = QVector<float>());

    // Selection management
    void setSelectedAtoms(const QVector<int>& indices);
    QVector<int> getSelectedAtoms() const;
    void clearSelection();

signals:
    // Emitted when user selects atom(s) in table
    void atomSelectionChanged(const QVector<int>& selectedIndices);
    void focusAtom(int atomIndex);  // User wants to focus on this atom

private slots:
    void onTableSelectionChanged();
    void onTableDoubleClicked(const QModelIndex& index);
    void onContextMenu(const QPoint& pos);
    void copySelectedAtoms();
    void focusSelectedAtom();

private:
    void setupUI();
    void setupContextMenu();
    void setupConnections();

    QTableView *m_tableView = nullptr;
    AtomTableModel *m_model = nullptr;

    // Prevent recursive updates when syncing with 3D viewer
    bool m_updatingSelection = false;

    void setModelData(const QVector<AtomTableModel::AtomData>& atoms);
};

#endif // ATOMLISTPANEL_H
