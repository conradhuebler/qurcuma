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

class AtomsSimulationDock;
class DisplayDock;
class EditorsDock;
class NavigationDock;
class OutputDock;
class ProjectDock;
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
    EditorsDock* editorsDockImpl() const;
    AtomsSimulationDock* atomsSimulationDockImpl() const;
    NavigationDock* navigationDockImpl() const;
    ProjectDock* projectDockImpl() const;

    // Accessors (return nullptr until the corresponding dock has been created).
    QDockWidget* projectDock() const;
    QDockWidget* editorsDock() const;
    QDockWidget* atomsSimulationDock() const;
    QDockWidget* displayDock() const;
    QDockWidget* outputDock() const;

    // Internal tab widgets, exposed so callers can switch tabs without knowing
    // whether the content lives in a dock wrapper.
    QTabWidget* editorsTabs() const;
    QTabWidget* atomsSimulationTabs() const;
    QTabWidget* navigationTabs() const;  // Returns nullptr: navigation is now a tab inside ProjectDock.

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

    // Restore globally persisted layout.
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
    // Preset helpers
    void applyVisualizationLayout();
    void applyEditingLayout();
    void applyCalculationLayout();
    void applyAnalysisLayout();
    void applyTeachingLayout();

    QMainWindow* m_mainWindow = nullptr;

    QDockWidget* m_projectDock = nullptr;
    QWidget* m_navigationDock = nullptr;
    QDockWidget* m_editorsDock = nullptr;
    QDockWidget* m_atomsSimulationDock = nullptr;
    QDockWidget* m_displayDock = nullptr;
    QDockWidget* m_outputViewDock = nullptr;

    QTabWidget* m_editorsTabs = nullptr;
    QTabWidget* m_atomsSimulationTabs = nullptr;
    QTabWidget* m_navigationTabs = nullptr;

    QByteArray m_defaultState;
    QHash<int, QByteArray> m_presetStates;
};
