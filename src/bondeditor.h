// BondEditor - Claude Generated
// Phase 4B: Bond creation, deletion, and modification with validation
// Integrates with MoleculeViewer for interactive bond editing and XYZ auto-save

#pragma once

#include "view.h"
#include <QObject>
#include <QVector>
#include <QString>
#include <QMap>

class BondEditor : public QObject
{
    Q_OBJECT

public:
    explicit BondEditor(QObject *parent = nullptr);

    // Bond manipulation methods
    bool addBond(int atom1, int atom2, int order = 1);
    bool removeBond(int bondIndex);
    bool changeBondOrder(int bondIndex, int newOrder);

    // Selection management
    void selectBond(int bondIndex);
    void deselectBond(int bondIndex);
    void clearSelection();
    const QVector<int>& getSelectedBonds() const { return m_selectedBonds; }

    // Query methods
    int findBond(int atom1, int atom2) const;
    int getBondCount() const { return m_bonds.size(); }
    const MoleculeViewer::Bond& getBond(int index) const { return m_bonds.at(index); }
    const QVector<MoleculeViewer::Bond>& getAllBonds() const { return m_bonds; }

    // Validation and utility
    bool isValidBond(int atom1, int atom2) const;
    bool canAddBond(int atom1, int atom2) const;  // Checks valence rules
    int getValence(int atomIndex) const;
    void setAtoms(const QVector<MoleculeViewer::Atom>& atoms) { m_atoms = atoms; }

    // Mode management
    enum class EditMode {
        None,           // No editing mode
        AddBondMode,    // Click 2 atoms to create bond
        DeleteBondMode, // Click bond to delete
        ChangeBondMode  // Click bond to cycle order
    };

    void setEditMode(EditMode mode) { m_editMode = mode; }
    EditMode getEditMode() const { return m_editMode; }

    // State management for undo/redo (deferred)
    void saveState() { m_savedBonds = m_bonds; }
    bool canUndo() const { return !m_savedBonds.isEmpty(); }

signals:
    void bondAdded(int bondIndex);
    void bondRemoved(int bondIndex);
    void bondModified(int bondIndex);
    void selectionChanged(const QVector<int>& selectedBonds);
    void structureChanged();  // Emitted when bonds change (for auto-save hook)
    void editModeChanged(EditMode newMode);
    void validationError(const QString& message);

private:
    // Covalent radii lookup (used for bond validation)
    static float getCovalentRadius(const QString& element);

    // Maximum valence for common elements
    static int getMaxValence(const QString& element);

    QVector<MoleculeViewer::Bond> m_bonds;
    QVector<MoleculeViewer::Bond> m_savedBonds;
    QVector<MoleculeViewer::Atom> m_atoms;
    QVector<int> m_selectedBonds;
    EditMode m_editMode = EditMode::None;

    // Constants for bond validation
    static constexpr float BOND_LENGTH_TOLERANCE = 0.5f;  // Angstroms
    static constexpr float MIN_BOND_DISTANCE = 0.5f;      // Minimum allowed distance
    static constexpr float MAX_BOND_DISTANCE = 2.0f;      // Maximum allowed distance
};
