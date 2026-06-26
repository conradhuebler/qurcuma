// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// NavigationDock — container widget with tabs for Bookmarks, Workspaces and Remote
// Directories. It is embedded as a tab inside ProjectDock instead of being a separate
// QDockWidget, which avoids the Qt tab-bar collapse problem when partner docks are
// toggled independently.
//
// Claude Generated 2026 - Dock system restructuring.

#pragma once

#include "dockconfig.h"

#include <QWidget>

class BookmarkWidget;
class RemoteDirectoriesPanel;
class Settings;
class WorkspacePanel;
class QTabWidget;

class NavigationDock : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationDock(Settings* settings, QWidget* parent = nullptr);

    QTabWidget* tabs() const;
    BookmarkWidget* bookmarkWidget() const;
    WorkspacePanel* workspacePanel() const;
    RemoteDirectoriesPanel* remotePanel() const;

signals:
    void bookmarkDirectorySelected(const QString& path);
    void bookmarksChanged();
    void saveWorkspaceRequested();
    void addRemoteRequested();

private:
    void setupUI(Settings* settings);

    QTabWidget* m_tabs = nullptr;
    BookmarkWidget* m_bookmarkWidget = nullptr;
    WorkspacePanel* m_workspacePanel = nullptr;
    RemoteDirectoriesPanel* m_remotePanel = nullptr;
};
