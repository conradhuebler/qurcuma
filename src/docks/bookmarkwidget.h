// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// BookmarkWidget — hierarchical bookmark tree for switching working directories.
// Extracted from MainWindow so the navigation dock owns a self-contained component.
//
// Claude Generated 2026 - Dock system restructuring.

#pragma once

#include "settings.h"

#include <QTreeWidget>
#include <QWidget>

class QToolButton;

class BookmarkWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BookmarkWidget(Settings* settings, QWidget* parent = nullptr);

    /// Rebuild the tree from Settings.
    void refresh();

    /// Raw access to the tree view during the migration.
    QTreeWidget* treeView() const;

    /// Currently selected bookmark path, empty if none.
    QString selectedPath() const;

signals:
    /// User clicked a bookmark that has a directory path.
    void directorySelected(const QString& path);
    /// Bookmarks changed (added, removed, renamed, moved, tags edited).
    void bookmarksChanged();

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);
    void onBookmarkCurrentDirectory();

private:
    void setupUI();
    void buildTree();

    Settings* m_settings = nullptr;
    QTreeWidget* m_treeView = nullptr;
    QToolButton* m_bookmarkButton = nullptr;
};
