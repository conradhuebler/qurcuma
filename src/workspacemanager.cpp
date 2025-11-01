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

    // These would be captured from MainWindow in full implementation
    // For now, storing placeholder values that can be filled in during integration
    ws.workingDirectory = "";          // Will be set from MainWindow::m_workingDirectory
    ws.openCalculations = QStringList(); // Will be set from MainWindow state
    ws.windowGeometry = QByteArray();    // Will be set from MainWindow::geometry()
    ws.splitterStates = QByteArray();    // Will be set from m_splitter->saveState()

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
