// Claude Generated - Atom Selection Management
#ifndef SELECTIONMANAGER_H
#define SELECTIONMANAGER_H

#include <QObject>
#include <QVector>

/**
 * @brief Manages atom selection state and notifies observers of changes
 *
 * Handles single and multi-selection of atoms in the molecule viewer,
 * providing signals for selection changes that UI components can observe.
 */
class SelectionManager : public QObject
{
    Q_OBJECT

public:
    explicit SelectionManager(QObject *parent = nullptr);

    // Selection manipulation
    void selectAtom(int index, bool append = false);
    void deselectAtom(int index);
    void toggleAtom(int index);
    void clearSelection();

    // Selection queries
    const QVector<int>& selectedAtoms() const { return m_selectedAtoms; }
    bool isSelected(int index) const { return m_selectedAtoms.contains(index); }
    int selectionCount() const { return m_selectedAtoms.size(); }

    // Focus management
    void setFocusedAtom(int index) { m_focusedAtom = index; }
    int getFocusedAtom() const { return m_focusedAtom; }

signals:
    void selectionChanged(const QVector<int>& selectedAtoms);
    void focusChanged(int atomIndex);
    void atomSelected(int index);
    void atomDeselected(int index);

private:
    QVector<int> m_selectedAtoms;
    int m_focusedAtom = -1;
};

#endif // SELECTIONMANAGER_H
