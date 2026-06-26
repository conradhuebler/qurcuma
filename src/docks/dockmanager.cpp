// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// DockManager implementation. Phase 3: OutputDock, DisplayDock, EditorsDock,
// AtomsSimulationDock and NavigationDock are owned here; ProjectDock is still
// adopted from MainWindow during the migration.
//
// Claude Generated 2026 - Dock system restructuring.

#include "dockmanager.h"

#include "atomssimulationdock.h"
#include "displaydock.h"
#include "editorsdock.h"
#include "outputdock.h"
#include "projectdock.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QSettings>
#include <QTabWidget>

DockManager::DockManager(QMainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
}

QDockWidget* DockManager::projectDock() const { return m_projectDock; }
QDockWidget* DockManager::editorsDock() const { return m_editorsDock; }
QDockWidget* DockManager::atomsSimulationDock() const { return m_atomsSimulationDock; }
QDockWidget* DockManager::displayDock() const { return m_displayDock; }
QDockWidget* DockManager::outputDock() const { return m_outputViewDock; }

QTabWidget* DockManager::editorsTabs() const
{
    if (auto* ed = qobject_cast<EditorsDock*>(m_editorsDock))
        return ed->editorsTabs();
    return nullptr;
}

QTabWidget* DockManager::atomsSimulationTabs() const
{
    if (auto* ad = qobject_cast<AtomsSimulationDock*>(m_atomsSimulationDock))
        return ad->tabs();
    return nullptr;
}

QTabWidget* DockManager::navigationTabs() const
{
    // Phase 6 redesign: navigation is embedded as a tab inside ProjectDock, so
    // there is no standalone navigation tab widget at the dock-manager level.
    return nullptr;
}

bool DockManager::dockVisible(QDockWidget* dock) const
{
    return dock && dock->isVisible();
}

static inline void applyPresetVisibility(QDockWidget* dock, bool visible)
{
    if (dock)
        dock->setVisible(visible);
}

// Phase 6 fix: tabified partners must share visibility. If one member of a tab
// group is shown, show the whole group so the tab bar stays usable. If the group
// is meant to be hidden, hide all members.
static void applyGroupVisibility(QWidget* primary, QWidget* partner, bool visible)
{
    if (!primary)
        return;
    primary->setVisible(visible);
    if (partner)
        partner->setVisible(visible);
}

OutputDock* DockManager::outputDockImpl() const
{
    return qobject_cast<OutputDock*>(m_outputViewDock);
}

DisplayDock* DockManager::displayDockImpl() const
{
    return qobject_cast<DisplayDock*>(m_displayDock);
}

EditorsDock* DockManager::editorsDockImpl() const
{
    return qobject_cast<EditorsDock*>(m_editorsDock);
}

AtomsSimulationDock* DockManager::atomsSimulationDockImpl() const
{
    return qobject_cast<AtomsSimulationDock*>(m_atomsSimulationDock);
}

ProjectDock* DockManager::projectDockImpl() const
{
    return qobject_cast<ProjectDock*>(m_projectDock);
}

void DockManager::applyPreset(DockConfig::LayoutPreset preset)
{
    switch (preset) {
    case DockConfig::LayoutPreset::Visualization:
        applyVisualizationLayout();
        break;
    case DockConfig::LayoutPreset::Editing:
        applyEditingLayout();
        break;
    case DockConfig::LayoutPreset::Calculation:
        applyCalculationLayout();
        break;
    case DockConfig::LayoutPreset::Analysis:
        applyAnalysisLayout();
        break;
    case DockConfig::LayoutPreset::Teaching:
        applyTeachingLayout();
        break;
    }
    emit presetApplied(preset);
}

void DockManager::setAppMode(DockConfig::AppMode mode, bool reflow)
{
    if (!m_mainWindow)
        return;

    const bool explore = (mode == DockConfig::AppMode::Explore);

    auto vis = [](QDockWidget* d, bool on) { if (d) d->setVisible(on); };
    vis(m_projectDock, true);
    // Phase 6 fix: tabified docks must stay visible in the mode switch, otherwise Qt
    // can collapse the tab bar so the user can never switch back. Visibility of
    // individual docks is governed by layout presets and View ▸ Dock Panels toggles.
    vis(m_editorsDock, true);
    vis(m_displayDock, true);
    vis(m_atomsSimulationDock, explore);
    vis(m_outputViewDock, !explore);
    if (explore && m_displayDock)
        m_displayDock->raise();
    if (!explore && m_editorsDock)
        m_editorsDock->raise();

    if (reflow) {
        if (explore) {
            if (m_mainWindow->width() > 0)
                m_mainWindow->resizeDocks({ m_projectDock, m_atomsSimulationDock },
                                          { int(m_mainWindow->width() * 0.16),
                                            int(m_mainWindow->width() * 0.22) },
                                          Qt::Horizontal);
        } else {
            if (m_mainWindow->width() > 0)
                m_mainWindow->resizeDocks({ m_projectDock, m_editorsDock },
                                          { int(m_mainWindow->width() * 0.20),
                                            int(m_mainWindow->width() * 0.30) },
                                          Qt::Horizontal);
            if (m_mainWindow->height() > 0)
                m_mainWindow->resizeDocks({ m_outputViewDock },
                                          { int(m_mainWindow->height() * 0.32) },
                                          Qt::Vertical);
        }
    }

    emit appModeChanged(mode);
}

void DockManager::captureBaselineState()
{
    if (m_mainWindow)
        m_defaultState = m_mainWindow->saveState();
}

void DockManager::restoreSavedLayout()
{
    if (!m_mainWindow)
        return;
    QSettings uiSettings;
    const QByteArray savedGeometry = uiSettings.value("ui/geometry").toByteArray();
    const QByteArray savedState = uiSettings.value("ui/dockState").toByteArray();
    if (!savedGeometry.isEmpty())
        m_mainWindow->restoreGeometry(savedGeometry);
    if (!savedState.isEmpty())
        m_mainWindow->restoreState(savedState);
    else
        applyPreset(DockConfig::LayoutPreset::Analysis);
}

void DockManager::resetToBaseline()
{
    if (!m_mainWindow || m_defaultState.isEmpty())
        return;
    m_presetStates.clear();
    m_mainWindow->restoreState(m_defaultState);
}

void DockManager::toggleLeftPanel()
{
    if (!m_projectDock)
        return;
    const bool show = !m_projectDock->isVisible();
    m_projectDock->setVisible(show);
}

void DockManager::initialize(MoleculeViewer* viewer, Settings* settings)
{
    if (!m_mainWindow || m_outputViewDock)
        return;

    m_outputViewDock = new OutputDock(m_mainWindow);
    m_displayDock = new DisplayDock(viewer, settings, m_mainWindow);
    m_editorsDock = new EditorsDock(m_mainWindow);
    m_atomsSimulationDock = new AtomsSimulationDock(m_mainWindow);
    m_projectDock = new ProjectDock(settings, m_mainWindow);
}

void DockManager::placeDocks()
{
    if (!m_mainWindow)
        return;

    if (m_projectDock) {
        m_projectDock->setAllowedAreas(Qt::LeftDockWidgetArea);
        m_mainWindow->addDockWidget(DockConfig::ProjectDockArea, m_projectDock);
        m_projectDock->raise();
    }

    if (m_editorsDock && m_atomsSimulationDock) {
        // Keep both editors and sim dock in the right area so the vertical splitter
        // does not drift to another area on drag.
        m_editorsDock->setAllowedAreas(Qt::RightDockWidgetArea);
        m_atomsSimulationDock->setAllowedAreas(Qt::RightDockWidgetArea);
        m_mainWindow->addDockWidget(DockConfig::EditorsDockArea, m_editorsDock);
        m_mainWindow->addDockWidget(DockConfig::AtomsSimulationDockArea, m_atomsSimulationDock);
        m_mainWindow->splitDockWidget(m_editorsDock, m_atomsSimulationDock, Qt::Vertical);
    }

    if (m_displayDock && m_editorsDock) {
        // Same area constraint: Display and Editors are tabified; prevent drift.
        m_displayDock->setAllowedAreas(Qt::RightDockWidgetArea);
        m_mainWindow->tabifyDockWidget(m_editorsDock, m_displayDock);
    }

    if (m_outputViewDock)
        m_mainWindow->addDockWidget(DockConfig::OutputViewDockArea, m_outputViewDock);
}

void DockManager::applyVisualizationLayout()
{
    const int key = static_cast<int>(DockConfig::LayoutPreset::Visualization);
    auto it = m_presetStates.find(key);
    if (it != m_presetStates.end()) {
        m_mainWindow->restoreState(*it);
    } else {
        applyPresetVisibility(m_projectDock, true);
        // Right group: hide Editors+Display together; show only Atoms&Simulation.
        applyGroupVisibility(m_editorsDock, m_displayDock, false);
        applyPresetVisibility(m_atomsSimulationDock, true);
        applyPresetVisibility(m_outputViewDock, false);
        if (auto* tabs = atomsSimulationTabs())
            tabs->setCurrentIndex(0);
        if (m_mainWindow && m_mainWindow->width() > 0) {
            m_mainWindow->resizeDocks({ m_projectDock, m_atomsSimulationDock },
                { int(m_mainWindow->width() * 0.18), int(m_mainWindow->width() * 0.22) },
                Qt::Horizontal);
        }
        m_presetStates.insert(key, m_mainWindow ? m_mainWindow->saveState() : QByteArray());
    }
}

void DockManager::applyEditingLayout()
{
    const int key = static_cast<int>(DockConfig::LayoutPreset::Editing);
    auto it = m_presetStates.find(key);
    if (it != m_presetStates.end()) {
        m_mainWindow->restoreState(*it);
    } else {
        applyPresetVisibility(m_projectDock, true);
        applyGroupVisibility(m_editorsDock, m_displayDock, true);
        applyPresetVisibility(m_atomsSimulationDock, false);
        applyPresetVisibility(m_outputViewDock, false);
        if (auto* tabs = editorsTabs())
            tabs->setCurrentIndex(0);
        if (m_mainWindow && m_mainWindow->width() > 0) {
            m_mainWindow->resizeDocks({ m_projectDock, m_editorsDock },
                { int(m_mainWindow->width() * 0.22), int(m_mainWindow->width() * 0.32) },
                Qt::Horizontal);
        }
        m_presetStates.insert(key, m_mainWindow ? m_mainWindow->saveState() : QByteArray());
    }
}

void DockManager::applyCalculationLayout()
{
    const int key = static_cast<int>(DockConfig::LayoutPreset::Calculation);
    auto it = m_presetStates.find(key);
    if (it != m_presetStates.end()) {
        m_mainWindow->restoreState(*it);
    } else {
        applyPresetVisibility(m_projectDock, true);
        applyGroupVisibility(m_editorsDock, m_displayDock, true);
        applyPresetVisibility(m_atomsSimulationDock, false);
        applyPresetVisibility(m_outputViewDock, true);
        if (m_mainWindow && m_mainWindow->height() > 0) {
            m_mainWindow->resizeDocks({ m_outputViewDock },
                { int(m_mainWindow->height() * 0.35) }, Qt::Vertical);
        }
        m_presetStates.insert(key, m_mainWindow ? m_mainWindow->saveState() : QByteArray());
    }
}

void DockManager::applyAnalysisLayout()
{
    const int key = static_cast<int>(DockConfig::LayoutPreset::Analysis);
    auto it = m_presetStates.find(key);
    if (it != m_presetStates.end()) {
        m_mainWindow->restoreState(*it);
    } else {
        applyPresetVisibility(m_projectDock, true);
        applyGroupVisibility(m_editorsDock, m_displayDock, true);
        applyPresetVisibility(m_atomsSimulationDock, true);
        applyPresetVisibility(m_outputViewDock, true);
        if (m_mainWindow && m_mainWindow->width() > 0) {
            m_mainWindow->resizeDocks({ m_projectDock, m_editorsDock },
                { int(m_mainWindow->width() * 0.22), int(m_mainWindow->width() * 0.28) },
                Qt::Horizontal);
        }
        if (m_mainWindow && m_mainWindow->height() > 0) {
            m_mainWindow->resizeDocks({ m_outputViewDock },
                { int(m_mainWindow->height() * 0.22) }, Qt::Vertical);
        }
        m_presetStates.insert(key, m_mainWindow ? m_mainWindow->saveState() : QByteArray());
    }
}

void DockManager::applyTeachingLayout()
{
    const int key = static_cast<int>(DockConfig::LayoutPreset::Teaching);
    auto it = m_presetStates.find(key);
    if (it != m_presetStates.end()) {
        m_mainWindow->restoreState(*it);
    } else {
        applyPresetVisibility(m_projectDock, true);
        applyGroupVisibility(m_editorsDock, m_displayDock, true);
        applyPresetVisibility(m_atomsSimulationDock, true);
        applyPresetVisibility(m_outputViewDock, true);
        if (auto* tabs = editorsTabs())
            tabs->setCurrentIndex(0);
        if (m_mainWindow && m_mainWindow->width() > 0) {
            m_mainWindow->resizeDocks({ m_projectDock, m_editorsDock, m_atomsSimulationDock },
                { int(m_mainWindow->width() * 0.18), int(m_mainWindow->width() * 0.26), int(m_mainWindow->width() * 0.22) },
                Qt::Horizontal);
        }
        if (m_mainWindow && m_mainWindow->height() > 0) {
            m_mainWindow->resizeDocks({ m_outputViewDock },
                { int(m_mainWindow->height() * 0.25) }, Qt::Vertical);
        }
        m_presetStates.insert(key, m_mainWindow ? m_mainWindow->saveState() : QByteArray());
    }
}
