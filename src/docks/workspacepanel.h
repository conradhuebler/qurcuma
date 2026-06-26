// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// WorkspacePanel — list of saved workspaces inside the Navigation dock.
// Thin wrapper around the existing workspace list so MainWindow can keep its
// connection logic during the migration.
//
// Claude Generated 2026 - Dock system restructuring.

#pragma once

#include <QWidget>

class QListWidget;
class QPushButton;

class WorkspacePanel : public QWidget
{
    Q_OBJECT

public:
    explicit WorkspacePanel(QWidget* parent = nullptr);

    QListWidget* listView() const;
    QPushButton* newWorkspaceButton() const;

signals:
    void saveWorkspaceRequested();

private:
    void setupUI();

    QListWidget* m_listView = nullptr;
    QPushButton* m_newButton = nullptr;
};
