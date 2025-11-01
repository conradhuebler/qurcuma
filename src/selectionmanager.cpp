// Claude Generated - Atom Selection Management Implementation
#include "selectionmanager.h"

SelectionManager::SelectionManager(QObject *parent)
    : QObject(parent)
    , m_focusedAtom(-1)
{
}

void SelectionManager::selectAtom(int index, bool append)
{
    if (!append && !m_selectedAtoms.isEmpty()) {
        clearSelection();
    }

    if (!m_selectedAtoms.contains(index)) {
        m_selectedAtoms.append(index);
        emit atomSelected(index);
        emit selectionChanged(m_selectedAtoms);
    }
}

void SelectionManager::deselectAtom(int index)
{
    if (m_selectedAtoms.contains(index)) {
        m_selectedAtoms.removeAll(index);
        emit atomDeselected(index);
        emit selectionChanged(m_selectedAtoms);
    }
}

void SelectionManager::toggleAtom(int index)
{
    if (m_selectedAtoms.contains(index)) {
        deselectAtom(index);
    } else {
        selectAtom(index, true);
    }
}

void SelectionManager::clearSelection()
{
    if (!m_selectedAtoms.isEmpty()) {
        for (int index : m_selectedAtoms) {
            emit atomDeselected(index);
        }
        m_selectedAtoms.clear();
        emit selectionChanged(m_selectedAtoms);
    }
}
