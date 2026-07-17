#ifndef WORKSPACEMANAGER_H
#define WORKSPACEMANAGER_H

#include <QObject>
#include "settings.h"

/**
 * @brief WorkspaceManager - persistence facade for the saved-workspace list.
 *
 * Thin wrapper over Settings for the named-workspace records (list/get/save/
 * delete/rename/last-used). Capturing and restoring the actual application state
 * lives in MainWindow (restoreWorkspaceState / the workspace save path).
 *
 * Claude Generated - Phase 4.2
 */
class WorkspaceManager : public QObject {
    Q_OBJECT

public:
    explicit WorkspaceManager(QObject* parent = nullptr);

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

    /**
     * @brief Update workspace last used timestamp
     */
    void updateWorkspaceLastUsed(const QString& id);

signals:
    /**
     * @brief Emitted when workspace list changes
     */
    void workspaceListChanged();

private:
    Settings m_settings;
};

#endif // WORKSPACEMANAGER_H
