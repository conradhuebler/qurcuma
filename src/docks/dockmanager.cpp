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
#include "chartdock.h"
#include "scriptdock.h"  // Claude Generated 2026 - not USE_LLM-gated: a script is a calculation
#ifdef USE_LLM
#include "chatdock.h"  // Claude Generated 2026
#endif
#include "ncidock.h"
#include "celldock.h"

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
QDockWidget* DockManager::chartDock() const { return m_chartDock; }

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

QList<QDockWidget*> DockManager::allDocks() const
{
    QList<QDockWidget*> docks;
    for (QDockWidget* d : { m_projectDock, m_structureDock, m_appearanceDock, m_simulationDock,
                            m_outputViewDock, m_nciDock, m_imageGalleryDock, m_chartDock,
                            m_cellDock, m_chatDock, static_cast<QDockWidget*>(m_scriptDock) })
        if (d)
            docks.append(d);
    return docks;
}

QStringList DockManager::openPanels() const
{
    QStringList names;
    for (QDockWidget* d : allDocks())
        if (!d->isHidden())
            names << d->objectName();
    return names;
}

// Hides first, then shows, so a tab bar never briefly holds only docks that are
// about to disappear; then brings @p front to the front of its tab group.
void DockManager::showPanels(const QStringList& panels, QDockWidget* front)
{
    const QList<QDockWidget*> docks = allDocks();
    for (QDockWidget* d : docks)
        if (!panels.contains(d->objectName()))
            setDockVisible(d, false);
    for (QDockWidget* d : docks)
        if (panels.contains(d->objectName()))
            setDockVisible(d, true);
    if (front && !front->isHidden())
        front->raise();
}

QStringList DockManager::defaultPanels(DockConfig::AppMode mode)
{
    QStringList panels = { DockConfig::ProjectDockObjectName, DockConfig::StructureDockObjectName };
    if (mode != DockConfig::AppMode::Explore)
        panels << DockConfig::SimulationDockObjectName;
    if (mode == DockConfig::AppMode::Compute)
        panels << DockConfig::OutputViewDockObjectName;
    return panels;
}

void DockManager::rememberPanels(DockConfig::AppMode mode)
{
    QSettings().setValue(QStringLiteral("%1/%2").arg(DockConfig::UiModePanelsGroup).arg(int(mode)),
                         openPanels());
}

QStringList DockManager::rememberedPanels(DockConfig::AppMode mode) const
{
    const QVariant stored = QSettings().value(
        QStringLiteral("%1/%2").arg(DockConfig::UiModePanelsGroup).arg(int(mode)));
    return stored.isValid() ? stored.toStringList() : defaultPanels(mode);
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

ChartDock* DockManager::chartDockImpl() const
{
    return qobject_cast<ChartDock*>(m_chartDock);
}

ChatDock* DockManager::chatDockImpl() const
{
#ifdef USE_LLM
    return qobject_cast<ChatDock*>(m_chatDock);
#else
    return nullptr;  // built without USE_LLM
#endif
}

ScriptDock* DockManager::scriptDockImpl() const
{
    return m_scriptDock;
}

CellDock* DockManager::cellDockImpl() const
{
    return qobject_cast<CellDock*>(m_cellDock);
}

NciDock* DockManager::nciDockImpl() const
{
    return qobject_cast<NciDock*>(m_nciDock);
}

void DockManager::setAppMode(DockConfig::AppMode mode, bool reflow)
{
    if (!m_mainWindow)
        return;

    // Claude Generated 2026 - Every mode keeps its own panels: the open ones are stored
    // for the mode being left, and the target mode gets back what it had (defaults:
    // Explore = Project + Structure, Teaching adds Simulation, Compute also Output).
    // Teaching additionally switches the Project panel's browser (MainWindow).
    if (m_modeApplied)
        rememberPanels(m_currentMode);
    const bool explore = (mode == DockConfig::AppMode::Explore);
    showPanels(rememberedPanels(mode), explore ? m_structureDock : m_simulationDock);
    m_currentMode = mode;
    m_modeApplied = true;

    if (reflow) {
        if (mode != DockConfig::AppMode::Compute) {
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
    if (m_modeApplied)
        rememberPanels(m_currentMode);
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
    QSettings().remove(DockConfig::UiModePanelsGroup);
    m_modeApplied = false;  // the next setAppMode opens the defaults without storing this state
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
    redock(m_chartDock, DockConfig::ChartDockArea, m_outputViewDock);
    redock(m_cellDock, DockConfig::CellDockArea, m_structureDock);
    redock(m_chatDock, DockConfig::ChatDockArea, nullptr);
    redock(m_scriptDock, DockConfig::ScriptDockArea, m_outputViewDock);
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
    m_cellDock = new CellDock(m_mainWindow);
    m_chartDock = new ChartDock(m_mainWindow);
    // Claude Generated 2026 - The script dock is not USE_LLM-gated: it is a place to
    // compute, and it is registered with the tool layer later (MainWindow creates the
    // dispatcher after the docks).
    m_scriptDock = new ScriptDock(m_mainWindow);
#ifdef USE_LLM
    // Claude Generated 2026 - The assistant. Hidden by default like the chart and
    // NCI docks; it is opened from View > Dock Panels when wanted.
    m_chatDock = new ChatDock(m_mainWindow);
#endif
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
    // tab group but start closed (Look ▸ Details… or View ▸ Panels opens them).
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
    // table would only take space. View ▸ Panels brings it up.
    // Claude Generated 2026.
    if (m_nciDock) {
        m_mainWindow->addDockWidget(DockConfig::NciDockArea, m_nciDock);
        if (m_structureDock)
            m_mainWindow->tabifyDockWidget(m_structureDock, m_nciDock);
        m_nciDock->hide();
    }

    // The unit-cell dock joins the same right-hand group and starts hidden; it
    // shows itself when a cif is loaded. Claude Generated 2026.
    if (m_cellDock) {
        m_mainWindow->addDockWidget(DockConfig::CellDockArea, m_cellDock);
        if (m_structureDock)
            m_mainWindow->tabifyDockWidget(m_structureDock, m_cellDock);
        m_cellDock->hide();
    }

    // The charts join the bottom area next to the output log and start hidden:
    // before a run the plots are empty, so they would only take space. View > Dock
    // Panels or Molecule > Simulation Charts brings them up. Claude Generated 2026.
    if (m_chartDock) {
        m_mainWindow->addDockWidget(DockConfig::ChartDockArea, m_chartDock);
        if (m_outputViewDock)
            m_mainWindow->tabifyDockWidget(m_outputViewDock, m_chartDock);
        m_chartDock->hide();
    }

#ifdef USE_LLM
    if (m_chatDock) {
        m_chatDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
        m_mainWindow->addDockWidget(DockConfig::ChatDockArea, m_chatDock);
        m_chatDock->hide();
    }
#endif

    // The script dock joins the bottom area next to the output log and starts hidden:
    // a script and its output belong together, and an idle editor would only take
    // space. View ▸ Panels brings it up. Claude Generated 2026.
    if (m_scriptDock) {
        m_mainWindow->addDockWidget(DockConfig::ScriptDockArea, m_scriptDock);
        if (m_outputViewDock)
            m_mainWindow->tabifyDockWidget(m_outputViewDock, m_scriptDock);
        m_scriptDock->hide();
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

