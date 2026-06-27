// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// DockManager implementation. Owns all QDockWidget shells, their initial placement,
// layout presets and the Explore/Compute application mode.
//
// Claude Generated 2026 - Dock system restructuring.

#include "dockmanager.h"

#include "outputdock.h"
#include "projectdock.h"
#include "simulationdock.h"
#include "displaydock.h"

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
QDockWidget* DockManager::displayDock() const { return m_displayDock; }
QDockWidget* DockManager::simulationDock() const { return m_simulationDock; }
QDockWidget* DockManager::outputDock() const { return m_outputViewDock; }

QTabWidget* DockManager::simulationTabs() const
{
    if (auto* sd = qobject_cast<SimulationDock*>(m_simulationDock))
        return sd->tabs();
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

// Return every QDockWidget that is tabified with the given dock, including the
// dock itself. Used to keep Qt's shared tab bar stable: tab group members must
// always be toggled together, never individually.
static QList<QDockWidget*> tabGroup(QMainWindow* mainWindow, QDockWidget* dock)
{
    QList<QDockWidget*> group;
    if (!dock)
        return group;
    group.append(dock);
    if (mainWindow)
        group.append(mainWindow->tabifiedDockWidgets(dock));
    return group;
}

// Set visibility of a dock and all of its tab partners at once. This prevents
// the Qt tab-bar collapse bug when one member of a tabified group is hidden
// while the others stay visible.
static void setDockGroupVisible(QMainWindow* mainWindow, QDockWidget* dock, bool visible)
{
    if (!dock)
        return;
    for (QDockWidget* member : tabGroup(mainWindow, dock))
        member->setVisible(visible);
}

OutputDock* DockManager::outputDockImpl() const
{
    return qobject_cast<OutputDock*>(m_outputViewDock);
}

DisplayDock* DockManager::displayDockImpl() const
{
    return qobject_cast<DisplayDock*>(m_displayDock);
}

SimulationDock* DockManager::simulationDockImpl() const
{
    return qobject_cast<SimulationDock*>(m_simulationDock);
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

    // Phase 6 fix, extended: tabified dock groups must be toggled together,
    // otherwise Qt's shared tab bar can collapse when one member is hidden.
    setDockGroupVisible(m_mainWindow, m_projectDock, true);
    setDockGroupVisible(m_mainWindow, m_displayDock, true);
    setDockGroupVisible(m_mainWindow, m_simulationDock, !explore);
    setDockGroupVisible(m_mainWindow, m_outputViewDock, !explore);
    if (explore && m_displayDock)
        m_displayDock->raise();
    if (!explore && m_simulationDock)
        m_simulationDock->raise();

    if (reflow) {
        if (explore) {
            if (m_mainWindow->width() > 0)
                m_mainWindow->resizeDocks({ m_projectDock, m_displayDock },
                                          { int(m_mainWindow->width() * 0.16),
                                            int(m_mainWindow->width() * 0.22) },
                                          Qt::Horizontal);
        } else {
            if (m_mainWindow->width() > 0)
                m_mainWindow->resizeDocks({ m_projectDock, m_simulationDock },
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
    setDockGroupVisible(m_mainWindow, m_projectDock, show);
}

void DockManager::initialize(MoleculeViewer* viewer, Settings* settings)
{
    if (!m_mainWindow || m_outputViewDock)
        return;

    m_outputViewDock = new OutputDock(m_mainWindow);
    m_displayDock = new DisplayDock(viewer, settings, m_mainWindow);
    m_simulationDock = new SimulationDock(m_mainWindow);
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

    if (m_displayDock) {
        m_displayDock->setAllowedAreas(Qt::RightDockWidgetArea);
        m_mainWindow->addDockWidget(DockConfig::DisplayDockArea, m_displayDock);
    }

    if (m_simulationDock) {
        m_simulationDock->setAllowedAreas(Qt::RightDockWidgetArea);
        m_mainWindow->addDockWidget(DockConfig::SimulationDockArea, m_simulationDock);
    }

    // The two right-side docks (Structure&Display and Simulation) are tabified
    // so the user can switch between them via a single tab bar.
    if (m_simulationDock && m_displayDock)
        m_mainWindow->tabifyDockWidget(m_displayDock, m_simulationDock);

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
        setDockGroupVisible(m_mainWindow, m_projectDock, true);
        // Right area: show Structure&Display, hide Simulation.
        setDockGroupVisible(m_mainWindow, m_displayDock, true);
        setDockGroupVisible(m_mainWindow, m_simulationDock, false);
        setDockGroupVisible(m_mainWindow, m_outputViewDock, false);
        if (m_mainWindow && m_mainWindow->width() > 0) {
            m_mainWindow->resizeDocks({ m_projectDock, m_displayDock },
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
        setDockGroupVisible(m_mainWindow, m_projectDock, true);
        // Right area: show Structure&Display, hide Simulation.
        setDockGroupVisible(m_mainWindow, m_displayDock, true);
        setDockGroupVisible(m_mainWindow, m_simulationDock, false);
        setDockGroupVisible(m_mainWindow, m_outputViewDock, false);
        if (auto* sdd = displayDockImpl())
            sdd->setCurrentTopSegment(DisplayDock::TopSegment::Structure);
        if (m_mainWindow && m_mainWindow->width() > 0) {
            m_mainWindow->resizeDocks({ m_projectDock, m_displayDock },
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
        setDockGroupVisible(m_mainWindow, m_projectDock, true);
        // Right area: show Simulation, hide Structure&Display.
        setDockGroupVisible(m_mainWindow, m_displayDock, false);
        setDockGroupVisible(m_mainWindow, m_simulationDock, true);
        setDockGroupVisible(m_mainWindow, m_outputViewDock, true);
        if (auto* sd = simulationDockImpl())
            sd->setCurrentTab(0); // Simulation tab
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
        setDockGroupVisible(m_mainWindow, m_projectDock, true);
        setDockGroupVisible(m_mainWindow, m_displayDock, true);
        setDockGroupVisible(m_mainWindow, m_simulationDock, true);
        setDockGroupVisible(m_mainWindow, m_outputViewDock, true);
        if (m_mainWindow && m_mainWindow->width() > 0) {
            m_mainWindow->resizeDocks({ m_projectDock, m_displayDock },
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
        setDockGroupVisible(m_mainWindow, m_projectDock, true);
        setDockGroupVisible(m_mainWindow, m_displayDock, true);
        setDockGroupVisible(m_mainWindow, m_simulationDock, true);
        setDockGroupVisible(m_mainWindow, m_outputViewDock, true);
        if (auto* tabs = simulationTabs())
            tabs->setCurrentIndex(0);
        if (m_mainWindow && m_mainWindow->width() > 0) {
            m_mainWindow->resizeDocks({ m_projectDock, m_displayDock },
                { int(m_mainWindow->width() * 0.18), int(m_mainWindow->width() * 0.26) },
                Qt::Horizontal);
        }
        if (m_mainWindow && m_mainWindow->height() > 0) {
            m_mainWindow->resizeDocks({ m_outputViewDock },
                { int(m_mainWindow->height() * 0.25) }, Qt::Vertical);
        }
        m_presetStates.insert(key, m_mainWindow ? m_mainWindow->saveState() : QByteArray());
    }
}
