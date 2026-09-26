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

// Top-level application mode, the only layout switch (UX stage 4b; the former five
// layout presets are gone, custom layouts are saved as workspaces). The values are
// persisted as ints under UiAppModeKey, so only append.
enum class AppMode {
    Explore = 0,   // viewer focus: Project + Structure
    Compute = 1,   // calculation workflow: + Simulation, Output and the calculation toolbar
    Teaching = 2   // Explore with the Project panel showing the lesson browser
};

// Stable object names used by QMainWindow::saveState()/restoreState().
// Do NOT change these without a migration plan; they are persisted in QSettings.
inline const QString ProjectDockObjectName = QStringLiteral("ProjectDock");
// Claude Generated 2026 - UX stage 4: the former "DisplayDock" is now Structure; the
// display settings live in AppearanceDock. Saved layouts were reset once (ui/layoutVersion).
inline const QString StructureDockObjectName = QStringLiteral("StructureDock");
inline const QString AppearanceDockObjectName = QStringLiteral("AppearanceDock");
inline const QString SimulationDockObjectName = QStringLiteral("SimulationDock");
inline const QString OutputViewDockObjectName = QStringLiteral("OutputViewDock");
inline const QString ImageGalleryDockObjectName = QStringLiteral("ImageGalleryDock");
inline const QString NciDockObjectName = QStringLiteral("NciDock");

// Default dock areas. Kept here so every wrapper class can declare its own.
inline const Qt::DockWidgetArea ProjectDockArea = Qt::LeftDockWidgetArea;
inline const Qt::DockWidgetArea StructureDockArea = Qt::RightDockWidgetArea;
inline const Qt::DockWidgetArea AppearanceDockArea = Qt::RightDockWidgetArea;
inline const Qt::DockWidgetArea SimulationDockArea = Qt::RightDockWidgetArea;
inline const Qt::DockWidgetArea OutputViewDockArea = Qt::BottomDockWidgetArea;
inline const Qt::DockWidgetArea ImageGalleryDockArea = Qt::BottomDockWidgetArea;
inline const Qt::DockWidgetArea NciDockArea = Qt::RightDockWidgetArea;

// Tab labels / dock titles.
inline const QString ProjectDockTitle = QStringLiteral("Project");
inline const QString StructureDockTitle = QStringLiteral("Structure");
inline const QString AppearanceDockTitle = QStringLiteral("Appearance");
inline const QString SimulationDockTitle = QStringLiteral("Simulation");
inline const QString OutputDockTitle = QStringLiteral("Output");
inline const QString ImageGalleryDockTitle = QStringLiteral("Images");
inline const QString NciDockTitle = QStringLiteral("Interactions");

// Persisted UI-state QSettings keys. Kept next to the enums/objectNames they
// encode so the window geometry, dock layout and app-mode keys live in one place
// instead of scattered magic strings. Do NOT change the values (existing configs
// are stored under them). Claude Generated 2026.
inline const QString UiGeometryKey = QStringLiteral("ui/geometry");
inline const QString UiDockStateKey = QStringLiteral("ui/dockState");
inline const QString UiAppModeKey = QStringLiteral("ui/appMode");
// Claude Generated 2026 - Open panels per mode, as dock objectNames under
// ui/modePanels/<int(AppMode)>; written when a mode is left and on close.
inline const QString UiModePanelsGroup = QStringLiteral("ui/modePanels");
// Claude Generated 2026 - Preference: opening a lesson switches to Teaching (default on).
inline const QString UiLessonOpensTeachingKey = QStringLiteral("ui/lessonOpensTeaching");
// Claude Generated 2026 - Bumped when the dock set changes so old layouts are dropped
// once instead of restored half-matching (2 = UX stage 4: Structure/Appearance split).
inline const QString UiLayoutVersionKey = QStringLiteral("ui/layoutVersion");
inline constexpr int UiLayoutVersion = 2;

} // namespace DockConfig
