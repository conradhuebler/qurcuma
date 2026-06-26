// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// ProjectDock — left-side dock for working-directory navigation, calculation
// directories, file/content browsing, and Lesson authoring controls.
//
// Claude Generated 2026 - Dock system restructuring.

#pragma once

#include "dockconfig.h"

#include <QDockWidget>

class BreadcrumbBar;
class BookmarkWidget;
class QComboBox;
class QFileSystemModel;
class QLabel;
class QLineEdit;
class QListView;
class NavigationDock;
class QPushButton;
class QTabWidget;
class QToolButton;
class RemoteDirectoriesPanel;
class QWidget;
class WorkspacePanel;

class ProjectDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit ProjectDock(QWidget* parent = nullptr);

    // Working directory and breadcrumb
    QPushButton* chooseDirectoryButton() const;
    BreadcrumbBar* breadcrumbBar() const;

    // Calculation directory list
    QListView* projectListView() const;
    QFileSystemModel* projectModel() const;
    QPushButton* newCalculationButton() const;

    // State / path labels
    QLabel* currentProjectLabel() const;
    QLabel* stateIcon() const;
    QLabel* stateIndicator() const;
    QPushButton* copyPathButton() const;

    // Files / Lesson browser toggle
    QToolButton* filesModeButton() const;
    QToolButton* lessonModeButton() const;

    // Content list and its filesystem model
    QListView* directoryContentView() const;
    QFileSystemModel* directoryContentModel() const;

    // Lesson metadata widget (visible in Lesson mode)
    QWidget* lessonMetaWidget() const;
    QLineEdit* lessonTitleEdit() const;
    QLineEdit* lessonDescEdit() const;
    QLabel* lessonAuthorsLabel() const;
    QToolButton* editAuthorsButton() const;

    // Per-structure detail editor (visible in Lesson mode with selection)
    QWidget* lessonStructWidget() const;
    QLineEdit* structNameEdit() const;
    QLineEdit* structDescEdit() const;
    QComboBox* structRoleCombo() const;

    // Navigation tab embedded inside this dock
    NavigationDock* navigationDock() const;
    BookmarkWidget* bookmarkWidget() const;
    WorkspacePanel* workspacePanel() const;
    RemoteDirectoriesPanel* remotePanel() const;

private:
    void setupUI();

    QPushButton* m_chooseDirectory = nullptr;
    BreadcrumbBar* m_breadcrumbBar = nullptr;
    QListView* m_projectListView = nullptr;
    QFileSystemModel* m_projectModel = nullptr;
    QPushButton* m_newCalculationButton = nullptr;
    QLabel* m_currentProjectLabel = nullptr;
    QLabel* m_stateIcon = nullptr;
    QLabel* m_stateIndicator = nullptr;
    QPushButton* m_copyPathButton = nullptr;
    QToolButton* m_filesModeBtn = nullptr;
    QToolButton* m_lessonModeBtn = nullptr;
    QListView* m_directoryContentView = nullptr;
    QFileSystemModel* m_directoryContentModel = nullptr;
    QWidget* m_lessonMetaWidget = nullptr;
    QLineEdit* m_lessonTitleEdit = nullptr;
    QLineEdit* m_lessonDescEdit = nullptr;
    QLabel* m_lessonAuthorsLabel = nullptr;
    QToolButton* m_editAuthorsButton = nullptr;
    QWidget* m_lessonStructWidget = nullptr;
    QLineEdit* m_structNameEdit = nullptr;
    QLineEdit* m_structDescEdit = nullptr;
    QComboBox* m_structRoleCombo = nullptr;

    QTabWidget* m_mainTabs = nullptr;
    NavigationDock* m_navigationDock = nullptr;
};
