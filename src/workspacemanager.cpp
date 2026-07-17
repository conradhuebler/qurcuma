#include "workspacemanager.h"

// Claude Generated - Phase 4.2
WorkspaceManager::WorkspaceManager(QObject* parent)
    : QObject(parent)
{
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
