#ifndef WORKSPACEMANAGER_H
#define WORKSPACEMANAGER_H

#include <QObject>
#include "settings.h"

class MainWindow;

/**
 * @brief WorkspaceManager - Manages saving/restoring complete application state
 *
 * Captures and restores the complete state of the application including:
 * - Working directory and current calculation directory
 * - Window geometry and splitter layout
 * - List of open calculation directories
 *
 * Claude Generated - Phase 4.2
 */
class WorkspaceManager : public QObject {
    Q_OBJECT

public:
    explicit WorkspaceManager(QObject* parent = nullptr);

    /**
     * @brief Capture the current application state into a Workspace
     * @param window The MainWindow to capture state from
     * @param name Workspace name
     * @param description Optional description
     * @return New Workspace struct with captured state
     */
    Settings::Workspace captureCurrentState(MainWindow* window,
                                           const QString& name,
                                           const QString& description = "");

    /**
     * @brief Restore a workspace's state into the application
     * @param workspace The workspace to restore
     * @param window The MainWindow to restore state to
     * @return true if restore successful, false otherwise
     */
    bool restoreWorkspace(const Settings::Workspace& workspace, MainWindow* window);

    /**
     * @brief Get list of all saved workspaces
     */
    QVector<Settings::Workspace> listWorkspaces() const;

    /**
     * @brief Save a workspace to persistent storage
     */
    void saveWorkspace(const Settings::Workspace& ws);

    /**
     * @brief Delete a workspace
     */
    void deleteWorkspace(const QString& id);

    /**
     * @brief Rename a workspace
     */
    void renameWorkspace(const QString& id, const QString& newName);

    /**
     * @brief Get a workspace by ID
     */
    Settings::Workspace getWorkspace(const QString& id) const;

signals:
    /**
     * @brief Emitted when workspace list changes
     */
    void workspaceListChanged();

    /**
     * @brief Emitted when workspace is restored
     */
    void workspaceRestored(const QString& workspaceName);

private:
    Settings m_settings;
};

#endif // WORKSPACEMANAGER_H
