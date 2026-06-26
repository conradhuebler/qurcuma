// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// RemoteDirectoriesPanel — SFTP remote mount list inside the Navigation dock.
// Thin wrapper; connection logic stays in MainWindow guarded by USE_SFTP.
//
// Claude Generated 2026 - Dock system restructuring.

#pragma once

#include <QWidget>

class QPushButton;
class QTreeWidget;

class RemoteDirectoriesPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteDirectoriesPanel(QWidget* parent = nullptr);

    QTreeWidget* treeView() const;
    QPushButton* addRemoteButton() const;

signals:
    void addRemoteRequested();

private:
    void setupUI();

    QTreeWidget* m_treeView = nullptr;
    QPushButton* m_addButton = nullptr;
};
