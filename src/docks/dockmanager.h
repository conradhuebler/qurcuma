// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// DockManager — owns all QDockWidgets, their initial placement and the dock side
// of the application mode (Explore/Compute/Teaching). MainWindow coordinates via signals,
// the manager handles the spatial/presentational side.
//
// Claude Generated 2026 - Dock system restructuring.

#pragma once

#include "dockconfig.h"

#include <QByteArray>
#include <QList>
#include <QStringList>
#include <QObject>

class SimulationDock;
class StructureDock;
class AppearanceDock;
class OutputDock;
class ProjectDock;
class ImageGalleryDock;
class NciDock;
class CellDock;  // Claude Generated 2026 - cif cell info + repeats
class ChartDock;
class ChatDock;  // Claude Generated 2026
class ScriptDock;  // Claude Generated 2026
class Settings;
class QDockWidget;
class QMainWindow;
class QTabWidget;

class DockManager : public QObject
{
    Q_OBJECT

public:
    explicit DockManager(QMainWindow* mainWindow, QObject* parent = nullptr);

    // Phase 2+ initialization. Must be called after MoleculeViewer and Settings exist.
    void initialize(class MoleculeViewer* viewer, class Settings* settings);

    // Typed accessors for the docks already migrated into wrappers.
    OutputDock* outputDockImpl() const;
    StructureDock* structureDockImpl() const;
    AppearanceDock* appearanceDockImpl() const;
    SimulationDock* simulationDockImpl() const;
    ProjectDock* projectDockImpl() const;
    ImageGalleryDock* imageGalleryDockImpl() const;
    NciDock* nciDockImpl() const;
    CellDock* cellDockImpl() const;
    ChartDock* chartDockImpl() const;
    /// Claude Generated 2026 - the assistant dock (null when built without USE_LLM).
    ChatDock* chatDockImpl() const;
    /// Claude Generated 2026 - the script dock (always present, USE_LLM or not).
    ScriptDock* scriptDockImpl() const;

    // Accessors (return nullptr until the corresponding dock has been created).
    QDockWidget* projectDock() const;
    QDockWidget* structureDock() const;
    QDockWidget* appearanceDock() const;
    QDockWidget* simulationDock() const;
    QDockWidget* outputDock() const;
    QDockWidget* imageGalleryDock() const;
    QDockWidget* nciDock() const;
    QDockWidget* chartDock() const;

    // Internal tab widgets, exposed so callers can switch tabs without knowing
    // whether the content lives in a dock wrapper.
    QTabWidget* simulationTabs() const;

    // State
    bool dockVisible(QDockWidget* dock) const;

    // Claude Generated 2026 - Open panels as dock objectNames (the stable identifiers
    // of DockConfig). A dock counts as open when it is not explicitly hidden, also as
    // a background tab or floating.
    QStringList openPanels() const;
    // Open exactly the named panels (unknown names are ignored) and bring @p front to
    // the front of its tab group if it is open.
    void showPanels(const QStringList& panels, QDockWidget* front = nullptr);
    // Panels a mode opens until the user has changed them in that mode.
    static QStringList defaultPanels(DockConfig::AppMode mode);

public slots:
    // Apply an application mode: first remember the open panels of the mode being
    // left, then open the panels remembered for @p mode (or its defaults). When
    // reflow is false only visibility is set, preserving restored sizes on startup.
    void setAppMode(DockConfig::AppMode mode, bool reflow = true);

    // Capture the current state as baseline (call once after the event loop starts).
    void captureBaselineState();

    // Persist the window geometry + dock layout and the current mode's panels
    // to QSettings (call on close).
    void saveLayout();

    // Restore globally persisted layout (geometry + dock state). Returns false when
    // no current dock state was stored, so the caller lays out the mode from scratch.
    bool restoreSavedLayout();

    // Reset to the baseline layout captured at startup and forget the panels
    // remembered per mode (the next setAppMode opens the defaults).
    void resetToBaseline();

    // Toggle the left panel group (Project + Navigation).
    void toggleLeftPanel();

    // Claude Generated 2026 - Re-dock every currently floating panel to its default
    // area via QMainWindow::addDockWidget(), a pure API call independent of screen
    // coordinates and of drag-and-drop, so it works on every platform. A fallback to
    // dragging a panel back (src/docks/CLAUDE.md, "Wayland"). Skips docks inside a
    // floating tab group (isFloating() is false for those). No-op for docked docks.
    void redockFloating();

signals:
    void appModeChanged(DockConfig::AppMode mode);

public:
    // Phase 2+: place all adopted/created docks in their default areas.
    void placeDocks();

private:
    QMainWindow* m_mainWindow = nullptr;

    QDockWidget* m_projectDock = nullptr;
    QDockWidget* m_structureDock = nullptr;
    QDockWidget* m_appearanceDock = nullptr;   // Claude Generated 2026 - UX stage 4
    QDockWidget* m_simulationDock = nullptr;
    QDockWidget* m_outputViewDock = nullptr;
    QDockWidget* m_imageGalleryDock = nullptr;
    QDockWidget* m_nciDock = nullptr;
    QDockWidget* m_cellDock = nullptr;
    QDockWidget* m_chartDock = nullptr;
    QDockWidget* m_chatDock = nullptr;  // Claude Generated 2026
    ScriptDock* m_scriptDock = nullptr;  // Claude Generated 2026

    QTabWidget* m_simulationTabs = nullptr;

    QByteArray m_defaultState;

    // Claude Generated 2026 - Panel memory per mode (see setAppMode).
    void rememberPanels(DockConfig::AppMode mode);
    QStringList rememberedPanels(DockConfig::AppMode mode) const;
    QList<QDockWidget*> allDocks() const;
    bool m_modeApplied = false;  // false until the first setAppMode and after a reset
    DockConfig::AppMode m_currentMode = DockConfig::AppMode::Explore;
};
