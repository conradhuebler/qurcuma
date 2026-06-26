// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// NavigationDock implementation.
//
// Claude Generated 2026 - Dock system restructuring.

#include "navigationdock.h"

#include "bookmarkwidget.h"
#include "remotedirectoriespanel.h"
#include "workspacepanel.h"
#include "settings.h"

#include <QTabWidget>
#include <QVBoxLayout>

NavigationDock::NavigationDock(Settings* settings, QWidget* parent)
    : QWidget(parent)
{
    setupUI(settings);
}

void NavigationDock::setupUI(Settings* settings)
{
    m_tabs = new QTabWidget(this);
    m_tabs->setTabPosition(QTabWidget::North);
    m_tabs->setDocumentMode(true);

    m_bookmarkWidget = new BookmarkWidget(settings, m_tabs);
    connect(m_bookmarkWidget, &BookmarkWidget::directorySelected,
            this, &NavigationDock::bookmarkDirectorySelected);
    connect(m_bookmarkWidget, &BookmarkWidget::bookmarksChanged,
            this, &NavigationDock::bookmarksChanged);
    m_tabs->addTab(m_bookmarkWidget, tr("Bookmarks"));

    m_workspacePanel = new WorkspacePanel(m_tabs);
    connect(m_workspacePanel, &WorkspacePanel::saveWorkspaceRequested,
            this, &NavigationDock::saveWorkspaceRequested);
    m_tabs->addTab(m_workspacePanel, tr("Workspaces"));

    m_remotePanel = new RemoteDirectoriesPanel(m_tabs);
    connect(m_remotePanel, &RemoteDirectoriesPanel::addRemoteRequested,
            this, &NavigationDock::addRemoteRequested);
    m_tabs->addTab(m_remotePanel, tr("Remote"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tabs);
}

QTabWidget* NavigationDock::tabs() const
{
    return m_tabs;
}

BookmarkWidget* NavigationDock::bookmarkWidget() const
{
    return m_bookmarkWidget;
}

WorkspacePanel* NavigationDock::workspacePanel() const
{
    return m_workspacePanel;
}

RemoteDirectoriesPanel* NavigationDock::remotePanel() const
{
    return m_remotePanel;
}
