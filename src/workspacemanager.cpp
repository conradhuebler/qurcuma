#include "workspacemanager.h"
#include "mainwindow.h"

#include <QUuid>
#include <QDateTime>

// Claude Generated - Phase 4.2
WorkspaceManager::WorkspaceManager(QObject* parent)
    : QObject(parent)
{
}

Settings::Workspace WorkspaceManager::captureCurrentState(MainWindow* window,
                                                        const QString& name,
                                                        const QString& description)
{
    Settings::Workspace ws;
    ws.id = QUuid::createUuid().toString();
    ws.name = name;
    ws.description = description;
    ws.created = QDateTime::currentDateTime();
    ws.lastUsed = QDateTime::currentDateTime();

    // Capture actual state from MainWindow
    // Note: These member variables would need public accessors or friend declaration
    // For now, we store empty values - the mainwindow.cpp implementation will fill them
    ws.workingDirectory = "";
    ws.openCalculations = QStringList();
    ws.windowGeometry = QByteArray();
    ws.splitterStates = QByteArray();

    return ws;
}

bool WorkspaceManager::restoreWorkspace(const Settings::Workspace& workspace, MainWindow* window)
{
    if (!workspace.isValid()) {
        return false;
    }

    // Update last used timestamp
    m_settings.updateWorkspaceLastUsed(workspace.id);

    // Restoration logic would be implemented here
    // Would call MainWindow methods to:
    // - Switch to workspace.workingDirectory
    // - Restore window geometry
    // - Restore splitter states
    // - Set current calculation directories

    emit workspaceRestored(workspace.name);
    return true;
}

QVector<Settings::Workspace> WorkspaceManager::listWorkspaces() const
{
    return m_settings.workspaces();
}

void WorkspaceManager::saveWorkspace(const Settings::Workspace& ws)
{
    m_settings.saveWorkspace(ws);
    emit workspaceListChanged();
}

void WorkspaceManager::deleteWorkspace(const QString& id)
{
    m_settings.deleteWorkspace(id);
    emit workspaceListChanged();
}

void WorkspaceManager::renameWorkspace(const QString& id, const QString& newName)
{
    Settings::Workspace ws = m_settings.loadWorkspace(id);
    if (ws.isValid()) {
        ws.name = newName;
        m_settings.saveWorkspace(ws);
        emit workspaceListChanged();
    }
}

Settings::Workspace WorkspaceManager::getWorkspace(const QString& id) const
{
    return m_settings.loadWorkspace(id);
}

void WorkspaceManager::updateWorkspaceLastUsed(const QString& id)
{
    m_settings.updateWorkspaceLastUsed(id);
}
