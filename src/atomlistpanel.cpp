// Claude Generated - Phase 2C - Atom List Panel Implementation
#include "atomlistpanel.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QDebug>

// ==================== AtomTableModel ====================

AtomTableModel::AtomTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int AtomTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_atoms.size();
}

int AtomTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return COLUMN_COUNT;
}

QVariant AtomTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_atoms.size()) {
        return QVariant();
    }

    const AtomData& atom = m_atoms[index.row()];

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
            case 0: return atom.index;
            case 1: return atom.element;
            case 2: return QString::number(atom.x, 'f', 3);
            case 3: return QString::number(atom.y, 'f', 3);
            case 4: return QString::number(atom.z, 'f', 3);
            case 5: return QString::number(atom.charge, 'f', 3);
            default: return QVariant();
        }
    }

    return QVariant();
}

QVariant AtomTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 0: return "#";
            case 1: return "Element";
            case 2: return "X (Å)";
            case 3: return "Y (Å)";
            case 4: return "Z (Å)";
            case 5: return "Charge";
            default: return QVariant();
        }
    }
    return QVariant();
}

void AtomTableModel::setAtomData(const QVector<AtomData>& atoms)
{
    beginResetModel();
    m_atoms = atoms;
    endResetModel();
}

void AtomTableModel::clear()
{
    beginResetModel();
    m_atoms.clear();
    endResetModel();
}

// ==================== AtomListPanel ====================

AtomListPanel::AtomListPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    setupContextMenu();
    setupConnections();
}

AtomListPanel::~AtomListPanel()
{
}

void AtomListPanel::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Create table
    m_tableView = new QTableView();
    m_model = new AtomTableModel(this);
    m_tableView->setModel(m_model);

    // Configure table appearance
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->setShowGrid(true);
    m_tableView->horizontalHeader()->setStretchLastSection(false);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);  // Element column stretches
    m_tableView->setSortingEnabled(true);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);

    // Column widths
    m_tableView->setColumnWidth(0, 40);   // Index
    m_tableView->setColumnWidth(1, 60);   // Element
    m_tableView->setColumnWidth(2, 70);   // X
    m_tableView->setColumnWidth(3, 70);   // Y
    m_tableView->setColumnWidth(4, 70);   // Z
    m_tableView->setColumnWidth(5, 70);   // Charge

    mainLayout->addWidget(m_tableView);
    setLayout(mainLayout);
}

void AtomListPanel::setupContextMenu()
{
    connect(m_tableView, &QTableView::customContextMenuRequested,
            this, &AtomListPanel::onContextMenu);
}

void AtomListPanel::setupConnections()
{
    connect(m_tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &AtomListPanel::onTableSelectionChanged);

    connect(m_tableView, &QTableView::doubleClicked,
            this, &AtomListPanel::onTableDoubleClicked);
}

void AtomListPanel::updateAtomList(const QVector<QVector3D>& positions,
                                    const QVector<QString>& elements,
                                    const QVector<float>& charges)
{
    QVector<AtomTableModel::AtomData> atoms;

    for (int i = 0; i < positions.size(); ++i) {
        AtomTableModel::AtomData data;
        data.index = i;
        data.element = (i < elements.size()) ? elements[i] : "?";
        data.x = positions[i].x();
        data.y = positions[i].y();
        data.z = positions[i].z();
        data.charge = (i < charges.size()) ? charges[i] : 0.0f;
        atoms.append(data);
    }

    m_model->setAtomData(atoms);
}

void AtomListPanel::setSelectedAtoms(const QVector<int>& indices)
{
    if (m_updatingSelection) return;

    m_updatingSelection = true;
    m_tableView->selectionModel()->clearSelection();

    for (int idx : indices) {
        if (idx >= 0 && idx < m_model->rowCount()) {
            QModelIndex modelIndex = m_model->index(idx, 0);
            m_tableView->selectionModel()->select(modelIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }

    // Scroll to first selected atom if any
    if (!indices.isEmpty()) {
        m_tableView->scrollTo(m_model->index(indices.first(), 0), QAbstractItemView::PositionAtCenter);
    }

    m_updatingSelection = false;
}

QVector<int> AtomListPanel::getSelectedAtoms() const
{
    QVector<int> selected;
    const auto& modelIndices = m_tableView->selectionModel()->selectedRows();

    for (const QModelIndex& index : modelIndices) {
        selected.append(index.row());
    }

    return selected;
}

void AtomListPanel::clearSelection()
{
    m_tableView->selectionModel()->clearSelection();
}

void AtomListPanel::onTableSelectionChanged()
{
    if (m_updatingSelection) return;

    QVector<int> selectedIndices = getSelectedAtoms();
    emit atomSelectionChanged(selectedIndices);
}

void AtomListPanel::onTableDoubleClicked(const QModelIndex& index)
{
    int atomIndex = index.row();
    emit focusAtom(atomIndex);
}

void AtomListPanel::onContextMenu(const QPoint& pos)
{
    QMenu contextMenu;

    QAction *copyAction = contextMenu.addAction("Copy Atom Data");
    QAction *focusAction = contextMenu.addAction("Focus on Atom");

    QAction *selectedAction = contextMenu.exec(m_tableView->mapToGlobal(pos));

    if (selectedAction == copyAction) {
        copySelectedAtoms();
    } else if (selectedAction == focusAction) {
        focusSelectedAtom();
    }
}

void AtomListPanel::copySelectedAtoms()
{
    QVector<int> selected = getSelectedAtoms();
    if (selected.isEmpty()) return;

    QString text = "Index\tElement\tX\tY\tZ\tCharge\n";

    for (int idx : selected) {
        if (idx < m_model->getAtomData().size()) {
            const auto& atom = m_model->getAtomData()[idx];
            text += QString("%1\t%2\t%3\t%4\t%5\t%6\n")
                .arg(atom.index)
                .arg(atom.element)
                .arg(atom.x, 0, 'f', 3)
                .arg(atom.y, 0, 'f', 3)
                .arg(atom.z, 0, 'f', 3)
                .arg(atom.charge, 0, 'f', 3);
        }
    }

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(text);
}

void AtomListPanel::focusSelectedAtom()
{
    QVector<int> selected = getSelectedAtoms();
    if (!selected.isEmpty()) {
        emit focusAtom(selected.first());
    }
}
