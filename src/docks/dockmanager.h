// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// DockManager — owns all QDockWidgets, their initial placement, layout presets
// and the Explore/Compute application mode. MainWindow coordinates via signals,
// the manager handles the spatial/presentational side.
//
// Claude Generated 2026 - Dock system restructuring.

#pragma once

#include "dockconfig.h"

#include <QByteArray>
#include <QHash>
#include <QObject>

class SimulationDock;
class DisplayDock;
class OutputDock;
class ProjectDock;
class ImageGalleryDock;
class NciDock;
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
    DisplayDock* displayDockImpl() const;
    SimulationDock* simulationDockImpl() const;
    ProjectDock* projectDockImpl() const;
    ImageGalleryDock* imageGalleryDockImpl() const;
    NciDock* nciDockImpl() const;
    ChartDock* chartDockImpl() const;
    /// Claude Generated 2026 - the assistant dock (null when built without USE_LLM).
    ChatDock* chatDockImpl() const;
    /// Claude Generated 2026 - the script dock (always present, USE_LLM or not).
    ScriptDock* scriptDockImpl() const;

    // Accessors (return nullptr until the corresponding dock has been created).
    QDockWidget* projectDock() const;
    QDockWidget* displayDock() const;
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

public slots:
    // Apply a named layout preset. Uses saveState()/restoreState() caching.
    void applyPreset(DockConfig::LayoutPreset preset);

    // Apply the top-level Explore/Compute mode. When reflow is false only
    // visibility is toggled, preserving restored sizes on startup.
    void setAppMode(DockConfig::AppMode mode, bool reflow = true);

    // Capture the current state as baseline (call once after the event loop starts).
    void captureBaselineState();

    // Persist the window geometry + dock layout to QSettings (call on close).
    void saveLayout();

    // Restore globally persisted layout (geometry + dock state).
    void restoreSavedLayout();

    // Reset to the baseline layout (clears preset caches).
    void resetToBaseline();

    // Toggle the left panel group (Project + Navigation).
    void toggleLeftPanel();

signals:
    void appModeChanged(DockConfig::AppMode mode);
    void presetApplied(DockConfig::LayoutPreset preset);

public:
    // Phase 2+: place all adopted/created docks in their default areas.
    void placeDocks();

private:
    QMainWindow* m_mainWindow = nullptr;

    QDockWidget* m_projectDock = nullptr;
    QDockWidget* m_displayDock = nullptr;
    QDockWidget* m_simulationDock = nullptr;
    QDockWidget* m_outputViewDock = nullptr;
    QDockWidget* m_imageGalleryDock = nullptr;
    QDockWidget* m_nciDock = nullptr;
    QDockWidget* m_chartDock = nullptr;
    QDockWidget* m_chatDock = nullptr;  // Claude Generated 2026
    ScriptDock* m_scriptDock = nullptr;  // Claude Generated 2026

    QTabWidget* m_simulationTabs = nullptr;

    QByteArray m_defaultState;
    QHash<int, QByteArray> m_presetStates;
};
