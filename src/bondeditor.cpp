// BondEditor Implementation - Claude Generated
// Phase 4B: Bond editing with validation and auto-save integration

#include "bondeditor.h"
#include <QDebug>
#include <cmath>

BondEditor::BondEditor(QObject *parent)
    : QObject(parent), m_editMode(EditMode::None)
{
}

bool BondEditor::addBond(int atom1, int atom2, int order)
{
    // Validate bond parameters
    if (!isValidBond(atom1, atom2)) {
        emit validationError("Invalid bond: Atom indices out of range");
        return false;
    }

    if (atom1 == atom2) {
        emit validationError("Invalid bond: Cannot bond atom to itself");
        return false;
    }

    if (order < 1 || order > 3) {
        emit validationError("Invalid bond order: Must be 1, 2, or 3");
        return false;
    }

    // Check if bond already exists
    int existingBondIdx = findBond(atom1, atom2);
    if (existingBondIdx >= 0) {
        emit validationError("Bond already exists between these atoms");
        return false;
    }

    // Valence check
    if (!canAddBond(atom1, atom2)) {
        emit validationError("Cannot add bond: Exceeds maximum valence");
        return false;
    }

    // Distance check (atoms should be reasonably close)
    if (m_atoms.size() > atom1 && m_atoms.size() > atom2) {
        float distance = (m_atoms[atom1].position - m_atoms[atom2].position).length();

        if (distance < MIN_BOND_DISTANCE) {
            emit validationError(QString("Atoms too close: %.3f Å (minimum: %.1f Å)")
                               .arg(distance).arg(MIN_BOND_DISTANCE));
            return false;
        }

        if (distance > MAX_BOND_DISTANCE) {
            emit validationError(QString("Atoms too far: %.3f Å (maximum: %.1f Å)")
                               .arg(distance).arg(MAX_BOND_DISTANCE));
            return false;
        }
    }

    // Add the bond
    MoleculeViewer::Bond newBond;
    newBond.atom1 = atom1;
    newBond.atom2 = atom2;
    newBond.bondOrder = order;

    m_bonds.append(newBond);
    int bondIndex = m_bonds.size() - 1;

    qDebug() << "Bond added:" << atom1 << "-" << atom2 << "order:" << order;
    emit bondAdded(bondIndex);
    emit structureChanged();

    return true;
}

bool BondEditor::removeBond(int bondIndex)
{
    if (bondIndex < 0 || bondIndex >= m_bonds.size()) {
        emit validationError("Invalid bond index");
        return false;
    }

    const auto& bond = m_bonds[bondIndex];
    qDebug() << "Bond removed:" << bond.atom1 << "-" << bond.atom2;

    m_bonds.removeAt(bondIndex);

    // Remove from selection if it was selected
    m_selectedBonds.removeAll(bondIndex);

    // Update selected bond indices (shift down if after removed index)
    for (int& idx : m_selectedBonds) {
        if (idx > bondIndex) {
            idx--;
        }
    }

    emit bondRemoved(bondIndex);
    emit structureChanged();

    return true;
}

bool BondEditor::changeBondOrder(int bondIndex, int newOrder)
{
    if (bondIndex < 0 || bondIndex >= m_bonds.size()) {
        emit validationError("Invalid bond index");
        return false;
    }

    if (newOrder < 1 || newOrder > 3) {
        emit validationError("Invalid bond order: Must be 1, 2, or 3");
        return false;
    }

    if (m_bonds[bondIndex].bondOrder == newOrder) {
        return false;  // No change
    }

    // Check valence change is valid (account for new bond order)
    int atom1 = m_bonds[bondIndex].atom1;
    int atom2 = m_bonds[bondIndex].atom2;
    int oldOrder = m_bonds[bondIndex].bondOrder;
    int orderDifference = newOrder - oldOrder;

    // Temporarily check if atoms can support the new order
    int currentValence1 = getValence(atom1) - oldOrder;  // Remove old bond
    int currentValence2 = getValence(atom2) - oldOrder;

    if (currentValence1 + newOrder > getMaxValence(m_atoms[atom1].element) ||
        currentValence2 + newOrder > getMaxValence(m_atoms[atom2].element)) {
        emit validationError("Bond order change exceeds valence limits");
        return false;
    }

    m_bonds[bondIndex].bondOrder = newOrder;
    qDebug() << "Bond modified:" << atom1 << "-" << atom2 << "new order:" << newOrder;

    emit bondModified(bondIndex);
    emit structureChanged();

    return true;
}

void BondEditor::selectBond(int bondIndex)
{
    if (bondIndex >= 0 && bondIndex < m_bonds.size()) {
        if (!m_selectedBonds.contains(bondIndex)) {
            m_selectedBonds.append(bondIndex);
            emit selectionChanged(m_selectedBonds);
        }
    }
}

void BondEditor::deselectBond(int bondIndex)
{
    if (m_selectedBonds.removeAll(bondIndex) > 0) {
        emit selectionChanged(m_selectedBonds);
    }
}

void BondEditor::clearSelection()
{
    if (!m_selectedBonds.isEmpty()) {
        m_selectedBonds.clear();
        emit selectionChanged(m_selectedBonds);
    }
}

int BondEditor::findBond(int atom1, int atom2) const
{
    // Search for bond between atom1 and atom2 (either direction)
    for (int i = 0; i < m_bonds.size(); ++i) {
        if ((m_bonds[i].atom1 == atom1 && m_bonds[i].atom2 == atom2) ||
            (m_bonds[i].atom1 == atom2 && m_bonds[i].atom2 == atom1)) {
            return i;
        }
    }
    return -1;
}

bool BondEditor::isValidBond(int atom1, int atom2) const
{
    return atom1 >= 0 && atom1 < m_atoms.size() &&
           atom2 >= 0 && atom2 < m_atoms.size();
}

bool BondEditor::canAddBond(int atom1, int atom2) const
{
    if (!isValidBond(atom1, atom2)) {
        return false;
    }

    // Check if atoms would exceed valence
    int valence1 = getValence(atom1);
    int maxValence1 = getMaxValence(m_atoms[atom1].element);

    int valence2 = getValence(atom2);
    int maxValence2 = getMaxValence(m_atoms[atom2].element);

    return (valence1 < maxValence1) && (valence2 < maxValence2);
}

int BondEditor::getValence(int atomIndex) const
{
    if (atomIndex < 0 || atomIndex >= m_atoms.size()) {
        return 0;
    }

    int valence = 0;
    for (const auto& bond : m_bonds) {
        if (bond.atom1 == atomIndex) {
            valence += bond.bondOrder;
        } else if (bond.atom2 == atomIndex) {
            valence += bond.bondOrder;
        }
    }
    return valence;
}

float BondEditor::getCovalentRadius(const QString& element)
{
    // Covalent radii in Angstroms (from NIST database)
    static const QMap<QString, float> radii = {
        {"H", 0.31f}, {"C", 0.76f}, {"N", 0.71f}, {"O", 0.66f}, {"F", 0.57f},
        {"P", 1.07f}, {"S", 1.05f}, {"Cl", 1.02f}, {"Br", 1.20f}, {"I", 1.39f},
        {"Si", 1.11f}, {"B", 0.87f}, {"As", 1.21f}, {"Se", 1.22f},
    };
    return radii.value(element, 1.0f);  // Default to 1.0 if not found
}

int BondEditor::getMaxValence(const QString& element)
{
    // Maximum valence for common elements
    // Primary valences (most common oxidation states)
    static const QMap<QString, int> maxValences = {
        {"H", 1},  {"C", 4},  {"N", 3},  {"O", 2},  {"F", 1},
        {"P", 5},  {"S", 6},  {"Cl", 1}, {"Br", 1}, {"I", 1},
        {"Si", 4}, {"B", 3},  {"As", 5}, {"Se", 2},
    };
    return maxValences.value(element, 2);  // Default to 2 if not found
}
