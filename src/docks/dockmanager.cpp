// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// DockManager implementation. Owns all QDockWidget shells, their initial placement
// and the dock side of the application mode (Explore/Compute/Teaching).
//
// Claude Generated 2026 - Dock system restructuring.

#include "dockmanager.h"

#include "outputdock.h"
#include "projectdock.h"
#include "simulationdock.h"
#include "structuredock.h"
#include "appearancedock.h"
#include "imagegallerydock.h"
#include "ncidock.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QSettings>
#include <QTabWidget>

#include <utility>

DockManager::DockManager(QMainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
}

QDockWidget* DockManager::projectDock() const { return m_projectDock; }
QDockWidget* DockManager::structureDock() const { return m_structureDock; }
QDockWidget* DockManager::appearanceDock() const { return m_appearanceDock; }
QDockWidget* DockManager::simulationDock() const { return m_simulationDock; }
QDockWidget* DockManager::outputDock() const { return m_outputViewDock; }
QDockWidget* DockManager::imageGalleryDock() const { return m_imageGalleryDock; }
QDockWidget* DockManager::nciDock() const { return m_nciDock; }

QTabWidget* DockManager::simulationTabs() const
{
    if (auto* sd = qobject_cast<SimulationDock*>(m_simulationDock))
        return sd->tabs();
    return nullptr;
}

bool DockManager::dockVisible(QDockWidget* dock) const
{
    return dock && dock->isVisible();
}

// Claude Generated 2026 - Show or hide ONE dock, exactly like its
// QDockWidget::toggleViewAction() does. Tab partners are left alone: QMainWindow
// rebuilds the shared tab bar by itself when a member appears or disappears.
//
// The previous helper toggled the whole tabified group (tabifiedDockWidgets()).
// With Display, Simulation and Interactions sharing one tab bar on the right
// (and Output + Images at the bottom) that was wrong in both directions:
// hiding Simulation for Explore mode also hid
// Display, and showing Display pulled the hidden-by-default Interactions and
// Images docks into the tab bar. The "tab-bar collapse" it was meant to avoid
// was the native 3D window (createWindowContainer) painting over the freshly
// rebuilt tab bar; the viewer is a QQuickWidget now, so plain per-dock
// visibility is the correct, drift-free path.
static void setDockVisible(QDockWidget* dock, bool visible)
{
    // isHidden() is the explicit flag (isVisible() is false for every dock while
    // the main window itself is not shown yet).
    if (dock && dock->isHidden() == visible)
        dock->setVisible(visible);
}

// Apply a visibility set to the four "layout" docks (hides first, then shows,
// so a tab bar never briefly holds only docks that are about to disappear) and
// bring one right-hand dock to the front of its tab group. The Interactions and
// Images docks are never touched here: they show themselves on demand.
static void applyDockVisibility(QDockWidget* project, bool showProject,
    QDockWidget* display, bool showDisplay,
    QDockWidget* simulation, bool showSimulation,
    QDockWidget* output, bool showOutput,
    QDockWidget* raiseDock)
{
    const std::pair<QDockWidget*, bool> docks[] = {
        { project, showProject }, { display, showDisplay },
        { simulation, showSimulation }, { output, showOutput }
    };
    for (const auto& [dock, show] : docks)
        if (!show)
            setDockVisible(dock, false);
    for (const auto& [dock, show] : docks)
        if (show)
            setDockVisible(dock, true);
    if (raiseDock && !raiseDock->isHidden())
        raiseDock->raise();
}

OutputDock* DockManager::outputDockImpl() const
{
    return qobject_cast<OutputDock*>(m_outputViewDock);
}

StructureDock* DockManager::structureDockImpl() const
{
    return qobject_cast<StructureDock*>(m_structureDock);
}

AppearanceDock* DockManager::appearanceDockImpl() const
{
    return qobject_cast<AppearanceDock*>(m_appearanceDock);
}

SimulationDock* DockManager::simulationDockImpl() const
{
    return qobject_cast<SimulationDock*>(m_simulationDock);
}

ProjectDock* DockManager::projectDockImpl() const
{
    return qobject_cast<ProjectDock*>(m_projectDock);
}

ImageGalleryDock* DockManager::imageGalleryDockImpl() const
{
    return qobject_cast<ImageGalleryDock*>(m_imageGalleryDock);
}

NciDock* DockManager::nciDockImpl() const
{
    return qobject_cast<NciDock*>(m_nciDock);
}

void DockManager::setAppMode(DockConfig::AppMode mode, bool reflow)
{
    if (!m_mainWindow)
        return;

    // Explore and Teaching: Project + Structure, viewer focus (Teaching differs only in
    // the Project panel's browser, which MainWindow switches). Compute: all four, with
    // the Simulation tab in front. Per-dock visibility (see setDockVisible).
    const bool explore = (mode != DockConfig::AppMode::Compute);
    applyDockVisibility(m_projectDock, true, m_structureDock, true,
        m_simulationDock, !explore, m_outputViewDock, !explore,
        explore ? m_structureDock : m_simulationDock);

    if (reflow) {
        if (explore) {
            if (m_mainWindow->width() > 0)
                m_mainWindow->resizeDocks({ m_projectDock, m_structureDock },
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

void DockManager::saveLayout()
{
    if (!m_mainWindow)
        return;
    QSettings uiSettings;
    uiSettings.setValue(DockConfig::UiGeometryKey, m_mainWindow->saveGeometry());
    uiSettings.setValue(DockConfig::UiDockStateKey, m_mainWindow->saveState());
}

bool DockManager::restoreSavedLayout()
{
    if (!m_mainWindow)
        return false;
    QSettings uiSettings;
    const QByteArray savedGeometry = uiSettings.value(DockConfig::UiGeometryKey).toByteArray();
    const QByteArray savedState = uiSettings.value(DockConfig::UiDockStateKey).toByteArray();
    if (!savedGeometry.isEmpty())
        m_mainWindow->restoreGeometry(savedGeometry);
    // Claude Generated 2026 - A layout saved for an older dock set is dropped once
    // (operator decision for UX stage 4) instead of being restored half-matching.
    const bool current = uiSettings.value(DockConfig::UiLayoutVersionKey, 0).toInt() >= DockConfig::UiLayoutVersion;
    const bool restored = !savedState.isEmpty() && current && m_mainWindow->restoreState(savedState);
    uiSettings.setValue(DockConfig::UiLayoutVersionKey, DockConfig::UiLayoutVersion);
    return restored;
}

void DockManager::resetToBaseline()
{
    if (!m_mainWindow || m_defaultState.isEmpty())
        return;
    m_mainWindow->restoreState(m_defaultState);
}

void DockManager::toggleLeftPanel()
{
    if (!m_projectDock)
        return;
    setDockVisible(m_projectDock, !m_projectDock->isVisible());
}

void DockManager::redockFloating()
{
    if (!m_mainWindow)
        return;

    // addDockWidget() docks (and un-floats) in one call; it is a plain API call
    // that computes nothing from the cursor or window positions, so it works
    // regardless of platform. Order matters: Display must be re-docked before
    // Simulation/Interactions try to tabify onto it.
    auto redock = [this](QDockWidget* dock, Qt::DockWidgetArea area, QDockWidget* tabWith) {
        if (!dock || !dock->isFloating())
            return;
        m_mainWindow->addDockWidget(area, dock);
        if (tabWith)
            m_mainWindow->tabifyDockWidget(tabWith, dock);
    };

    redock(m_projectDock, DockConfig::ProjectDockArea, nullptr);
    redock(m_structureDock, DockConfig::StructureDockArea, nullptr);
    redock(m_appearanceDock, DockConfig::AppearanceDockArea, m_structureDock);
    redock(m_simulationDock, DockConfig::SimulationDockArea, m_structureDock);
    redock(m_nciDock, DockConfig::NciDockArea, m_structureDock);
    redock(m_outputViewDock, DockConfig::OutputViewDockArea, nullptr);
    redock(m_imageGalleryDock, DockConfig::ImageGalleryDockArea, m_outputViewDock);
}

void DockManager::initialize(MoleculeViewer* viewer, Settings* settings)
{
    if (!m_mainWindow || m_outputViewDock)
        return;

    m_outputViewDock = new OutputDock(m_mainWindow);
    m_structureDock = new StructureDock(m_mainWindow);
    m_appearanceDock = new AppearanceDock(viewer, settings, m_mainWindow);
    m_simulationDock = new SimulationDock(m_mainWindow);
    m_projectDock = new ProjectDock(settings, m_mainWindow);
    m_imageGalleryDock = new ImageGalleryDock(m_mainWindow);
    m_nciDock = new NciDock(m_mainWindow);
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

    if (m_structureDock) {
        m_structureDock->setAllowedAreas(Qt::RightDockWidgetArea);
        m_mainWindow->addDockWidget(DockConfig::StructureDockArea, m_structureDock);
    }

    if (m_simulationDock) {
        m_simulationDock->setAllowedAreas(Qt::RightDockWidgetArea);
        m_mainWindow->addDockWidget(DockConfig::SimulationDockArea, m_simulationDock);
    }

    // The two right-side docks (Structure and Simulation) are tabified
    // so the user can switch between them via a single tab bar.
    if (m_simulationDock && m_structureDock)
        m_mainWindow->tabifyDockWidget(m_structureDock, m_simulationDock);

    // Claude Generated 2026 - UX stage 4: the detailed display settings join the right
    // tab group but start closed (Look ▸ Details… or View ▸ Dock Panels opens them).
    if (m_appearanceDock) {
        m_appearanceDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        m_mainWindow->addDockWidget(DockConfig::AppearanceDockArea, m_appearanceDock);
        if (m_structureDock)
            m_mainWindow->tabifyDockWidget(m_structureDock, m_appearanceDock);
        m_appearanceDock->hide();
    }

    if (m_outputViewDock)
        m_mainWindow->addDockWidget(DockConfig::OutputViewDockArea, m_outputViewDock);

    // The interaction dock joins the right-hand tab group (Display / Simulation)
    // and starts hidden: the NCI overlay is off by default, so an empty contact
    // table would only take space. View > Dock Panels brings it up.
    // Claude Generated 2026.
    if (m_nciDock) {
        m_mainWindow->addDockWidget(DockConfig::NciDockArea, m_nciDock);
        if (m_structureDock)
            m_mainWindow->tabifyDockWidget(m_structureDock, m_nciDock);
        m_nciDock->hide();
    }

    // The image-gallery dock shares the bottom area (tabified with Output) and
    // stays hidden until the first image is exported (ImageGalleryDock shows
    // itself in addExportedImage). Claude Generated 2026.
    if (m_imageGalleryDock) {
        m_mainWindow->addDockWidget(DockConfig::ImageGalleryDockArea, m_imageGalleryDock);
        if (m_outputViewDock)
            m_mainWindow->tabifyDockWidget(m_outputViewDock, m_imageGalleryDock);
        m_imageGalleryDock->hide();
    }
}

