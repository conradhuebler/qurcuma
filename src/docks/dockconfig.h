// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// DockConfig — shared enums, object names and default areas for the dock
// architecture. Used by DockManager and all dock wrappers to keep identifiers in
// one place and preserve saveState()/restoreState() compatibility.
//
// Claude Generated 2026 - Dock system restructuring.

#pragma once

#include <QDockWidget>
#include <QString>

namespace DockConfig {

// Layout presets supported by DockManager. The first four are bound to
// Ctrl+Alt+1..4; Teaching is used by the Lesson / interactive-demo workflow.
enum class LayoutPreset {
    Visualization = 0,  // 3D viewer focus: Project + Atoms&Simulation + Display
    Editing,            // Structure editing: Project + Navigation + Editors
    Calculation,        // Job setup/run: Project + Editors + Output
    Analysis,           // Balanced: all panels visible
    Teaching            // Interactive demo: Project + Editors + Atoms&Simulation + Output
};

// Top-level application mode. Explore = viewer focus; Compute = calculation workflow.
enum class AppMode {
    Explore,
    Compute
};

// Stable object names used by QMainWindow::saveState()/restoreState().
// Do NOT change these without a migration plan; they are persisted in QSettings.
inline const QString ProjectDockObjectName = QStringLiteral("ProjectDock");
inline const QString EditorsDockObjectName = QStringLiteral("EditorsDock");
inline const QString AtomsSimulationDockObjectName = QStringLiteral("AtomsSimulationDock");
inline const QString DisplayDockObjectName = QStringLiteral("DisplayDock");
inline const QString OutputViewDockObjectName = QStringLiteral("OutputViewDock");

// Default dock areas. Kept here so every wrapper class can declare its own.
inline const Qt::DockWidgetArea ProjectDockArea = Qt::LeftDockWidgetArea;
inline const Qt::DockWidgetArea EditorsDockArea = Qt::RightDockWidgetArea;
inline const Qt::DockWidgetArea AtomsSimulationDockArea = Qt::RightDockWidgetArea;
inline const Qt::DockWidgetArea DisplayDockArea = Qt::RightDockWidgetArea;
inline const Qt::DockWidgetArea OutputViewDockArea = Qt::BottomDockWidgetArea;

// Tab labels / dock titles.
inline const QString ProjectDockTitle = QStringLiteral("Project");
inline const QString EditorsDockTitle = QStringLiteral("Editors");
inline const QString AtomsSimulationDockTitle = QStringLiteral("Atoms && Simulation");
inline const QString DisplayDockTitle = QStringLiteral("Display");
inline const QString OutputDockTitle = QStringLiteral("Output");

} // namespace DockConfig
