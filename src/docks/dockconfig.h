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
    Visualization = 0,  // 3D viewer focus: Project + Structure&Display + Simulation
    Editing,            // Structure editing: Project + Structure&Display
    Calculation,        // Job setup/run: Project + Simulation + Output
    Analysis,           // Balanced: all panels visible
    Teaching            // Interactive demo: Project + Structure&Display + Simulation + Output
};

// Top-level application mode. Explore = viewer focus; Compute = calculation workflow.
enum class AppMode {
    Explore,
    Compute
};

// Stable object names used by QMainWindow::saveState()/restoreState().
// Do NOT change these without a migration plan; they are persisted in QSettings.
inline const QString ProjectDockObjectName = QStringLiteral("ProjectDock");
inline const QString DisplayDockObjectName = QStringLiteral("DisplayDock");
inline const QString SimulationDockObjectName = QStringLiteral("SimulationDock");
inline const QString OutputViewDockObjectName = QStringLiteral("OutputViewDock");
inline const QString ImageGalleryDockObjectName = QStringLiteral("ImageGalleryDock");
inline const QString NciDockObjectName = QStringLiteral("NciDock");
inline const QString CellDockObjectName = QStringLiteral("CellDock");  // Claude Generated 2026
inline const QString ChartDockObjectName = QStringLiteral("ChartDock");
inline const QString ChatDockObjectName = QStringLiteral("ChatDock");  // Claude Generated 2026

// Default dock areas. Kept here so every wrapper class can declare its own.
inline const Qt::DockWidgetArea ProjectDockArea = Qt::LeftDockWidgetArea;
inline const Qt::DockWidgetArea DisplayDockArea = Qt::RightDockWidgetArea;
inline const Qt::DockWidgetArea SimulationDockArea = Qt::RightDockWidgetArea;
inline const Qt::DockWidgetArea OutputViewDockArea = Qt::BottomDockWidgetArea;
inline const Qt::DockWidgetArea ImageGalleryDockArea = Qt::BottomDockWidgetArea;
inline const Qt::DockWidgetArea NciDockArea = Qt::RightDockWidgetArea;
inline const Qt::DockWidgetArea CellDockArea = Qt::RightDockWidgetArea;  // Claude Generated 2026
inline const Qt::DockWidgetArea ChartDockArea = Qt::BottomDockWidgetArea;
inline const Qt::DockWidgetArea ChatDockArea = Qt::RightDockWidgetArea;  // Claude Generated 2026

// Tab labels / dock titles.
inline const QString ProjectDockTitle = QStringLiteral("Project");
inline const QString DisplayDockTitle = QStringLiteral("Display");
inline const QString SimulationDockTitle = QStringLiteral("Simulation");
inline const QString OutputDockTitle = QStringLiteral("Output");
inline const QString ImageGalleryDockTitle = QStringLiteral("Images");
inline const QString NciDockTitle = QStringLiteral("Interactions");
inline const QString CellDockTitle = QStringLiteral("Unit Cell");  // Claude Generated 2026
inline const QString ChartDockTitle = QStringLiteral("Charts");
inline const QString ChatDockTitle = QStringLiteral("Assistant");  // Claude Generated 2026

// Persisted UI-state QSettings keys. Kept next to the enums/objectNames they
// encode so the window geometry, dock layout and app-mode keys live in one place
// instead of scattered magic strings. Do NOT change the values (existing configs
// are stored under them). Claude Generated 2026.
inline const QString UiGeometryKey = QStringLiteral("ui/geometry");
inline const QString UiDockStateKey = QStringLiteral("ui/dockState");
inline const QString UiAppModeKey = QStringLiteral("ui/appMode");

} // namespace DockConfig
