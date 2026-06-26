// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// ProjectDock implementation.
//
// Claude Generated 2026 - Dock system restructuring.

#include "projectdock.h"

#include "bookmarkwidget.h"
#include "navigationdock.h"
#include "remotedirectoriespanel.h"
#include "workspacepanel.h"
#include "widgets/breadcrumbbar.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDir>
#include <QFileSystemModel>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

ProjectDock::ProjectDock(QWidget* parent)
    : QDockWidget(DockConfig::ProjectDockTitle, parent)
{
    setObjectName(DockConfig::ProjectDockObjectName);
    setupUI();
}

void ProjectDock::setupUI()
{
    QWidget* projectWidget = new QWidget;
    QVBoxLayout* projectLayout = new QVBoxLayout(projectWidget);
    projectLayout->setContentsMargins(4, 4, 4, 4);
    projectLayout->setSpacing(4);

    m_chooseDirectory = new QPushButton(tr("Choose Working Directory"));
    m_chooseDirectory->setIcon(QIcon::fromTheme("folder-open", QIcon(":/icons/folder.png")));
    projectLayout->addWidget(m_chooseDirectory);

    m_breadcrumbBar = new BreadcrumbBar;
    m_breadcrumbBar->setHomeDirectory(QDir::homePath());
    projectLayout->addWidget(m_breadcrumbBar);

    // Splitter: upper = calculation directories list, lower = files inside selected dir
    QSplitter* projectSplitter = new QSplitter(Qt::Vertical);

    QWidget* calcDirsWidget = new QWidget;
    QVBoxLayout* calcDirsLayout = new QVBoxLayout(calcDirsWidget);
    calcDirsLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* dirListLabel = new QLabel(tr("Calculation Directories"));
    dirListLabel->setStyleSheet("font-weight: bold;");
    calcDirsLayout->addWidget(dirListLabel);
    m_projectListView = new QListView;
    m_projectModel = new QFileSystemModel(this);
    m_projectModel->setFilter(QDir::AllDirs | QDir::NoDot);
    m_projectModel->setReadOnly(true);
    m_projectListView->setModel(m_projectModel);
    calcDirsLayout->addWidget(m_projectListView);
    m_newCalculationButton = new QPushButton(tr("+ New Calculation Directory"));
    m_newCalculationButton->setIcon(QIcon::fromTheme("folder-new", QIcon()));
    m_newCalculationButton->setToolTip(tr("Create a new calculation directory (Ctrl+N)"));
    calcDirsLayout->addWidget(m_newCalculationButton);
    projectSplitter->addWidget(calcDirsWidget);

    QWidget* calcFilesWidget = new QWidget;
    QVBoxLayout* calcFilesLayout = new QVBoxLayout(calcFilesWidget);
    calcFilesLayout->setContentsMargins(0, 0, 0, 0);

    QWidget* pathWidget = new QWidget;
    QHBoxLayout* pathLayout = new QHBoxLayout(pathWidget);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    m_currentProjectLabel = new QLabel;
    m_currentProjectLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_currentProjectLabel->setWordWrap(false);
    pathLayout->addWidget(m_currentProjectLabel, 1);
    m_copyPathButton = new QPushButton;
    m_copyPathButton->setIcon(QIcon::fromTheme("edit-copy"));
    m_copyPathButton->setToolTip(tr("Copy current path to clipboard"));
    m_copyPathButton->setMaximumWidth(30);
    pathLayout->addWidget(m_copyPathButton);
    calcFilesLayout->addWidget(pathWidget);

    QWidget* stateWidget = new QWidget;
    QHBoxLayout* stateLayout = new QHBoxLayout(stateWidget);
    stateLayout->setContentsMargins(0, 0, 0, 0);
    m_stateIcon = new QLabel("●");
    m_stateIcon->setStyleSheet("color: grey; font-size: 14px;");
    m_stateIcon->setFixedWidth(20);
    stateLayout->addWidget(m_stateIcon);
    m_stateIndicator = new QLabel(tr("No directory selected"));
    stateLayout->addWidget(m_stateIndicator);
    stateLayout->addStretch();

    QButtonGroup* browserModeGroup = new QButtonGroup(this);
    browserModeGroup->setExclusive(true);
    m_filesModeBtn = new QToolButton;
    m_filesModeBtn->setText(tr("Files"));
    m_filesModeBtn->setCheckable(true);
    m_filesModeBtn->setChecked(true);
    m_lessonModeBtn = new QToolButton;
    m_lessonModeBtn->setText(tr("Lesson (0)"));
    m_lessonModeBtn->setCheckable(true);
    m_lessonModeBtn->setToolTip(tr("Show the in-memory lesson structures and metadata.\n"
                                   "Drop molecule files here to add them to the lesson."));
    m_lessonModeBtn->setAcceptDrops(true); // drop molecule files to add them (handled by MainWindow)
    browserModeGroup->addButton(m_filesModeBtn);
    browserModeGroup->addButton(m_lessonModeBtn);
    QWidget* browserModeWidget = new QWidget;
    QHBoxLayout* bmLayout = new QHBoxLayout(browserModeWidget);
    bmLayout->setContentsMargins(0, 0, 0, 0);
    bmLayout->setSpacing(0);
    bmLayout->addWidget(m_filesModeBtn);
    bmLayout->addWidget(m_lessonModeBtn);
    browserModeWidget->setStyleSheet(QStringLiteral(
        "QToolButton { padding: 2px 10px; border: 1px solid palette(mid); }"
        "QToolButton:checked { background: palette(highlight); color: palette(highlighted-text);"
        " font-weight: bold; }"));
    stateLayout->addWidget(browserModeWidget);
    calcFilesLayout->addWidget(stateWidget);

    m_directoryContentView = new QListView;
    m_directoryContentModel = new QFileSystemModel(this);
    m_directoryContentModel->setFilter(QDir::NoDotAndDotDot | QDir::Files);
    m_directoryContentModel->setNameFilters(QStringList()
        << "*.xyz" << "*.vtf" << "*.pdb" << "*.mol2" << "*.inp" << "*.log" << "*.out"
        << "*.hess" << "*.gbw" << "*.txt" << "*.*" << "input");
    m_directoryContentModel->setNameFilterDisables(false);
    m_directoryContentView->setModel(m_directoryContentModel);
    m_directoryContentView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_directoryContentView->setDragEnabled(true); // drag files onto the Lesson toggle to add them
    calcFilesLayout->addWidget(m_directoryContentView);

    // Lesson metadata widget — above the list, visible only in Lesson mode.
    m_lessonMetaWidget = new QWidget;
    QFormLayout* metaForm = new QFormLayout(m_lessonMetaWidget);
    metaForm->setContentsMargins(2, 2, 2, 2);
    m_lessonTitleEdit = new QLineEdit;
    m_lessonTitleEdit->setPlaceholderText(tr("Lesson title"));
    m_lessonDescEdit = new QLineEdit;
    m_lessonDescEdit->setPlaceholderText(tr("Short description"));
    QWidget* authorsRow = new QWidget;
    QHBoxLayout* authorsRowLayout = new QHBoxLayout(authorsRow);
    authorsRowLayout->setContentsMargins(0, 0, 0, 0);
    m_lessonAuthorsLabel = new QLabel(tr("(none)"));
    m_lessonAuthorsLabel->setWordWrap(true);
    m_editAuthorsButton = new QToolButton;
    m_editAuthorsButton->setText(tr("Authors / License…"));
    m_editAuthorsButton->setToolTip(tr("Edit authors (ORCID/institution), license, keywords"));
    authorsRowLayout->addWidget(m_lessonAuthorsLabel, 1);
    authorsRowLayout->addWidget(m_editAuthorsButton);
    metaForm->addRow(tr("Title:"), m_lessonTitleEdit);
    metaForm->addRow(tr("Desc.:"), m_lessonDescEdit);
    metaForm->addRow(tr("Authors:"), authorsRow);
    m_lessonMetaWidget->setVisible(false);
    calcFilesLayout->addWidget(m_lessonMetaWidget);

    // Per-structure inline detail editor — below the list, visible only in Lesson
    // mode while a structure is selected.
    m_lessonStructWidget = new QWidget;
    QFormLayout* structForm = new QFormLayout(m_lessonStructWidget);
    structForm->setContentsMargins(2, 2, 2, 2);
    m_structNameEdit = new QLineEdit;
    m_structNameEdit->setPlaceholderText(tr("Structure name"));
    m_structDescEdit = new QLineEdit;
    m_structDescEdit->setPlaceholderText(tr("Notes for this structure"));
    m_structRoleCombo = new QComboBox;
    m_structRoleCombo->addItems({ tr("(none)"), QStringLiteral("start"),
        QStringLiteral("intermediate"), QStringLiteral("target") });
    structForm->addRow(tr("Name:"), m_structNameEdit);
    structForm->addRow(tr("Notes:"), m_structDescEdit);
    structForm->addRow(tr("Role:"), m_structRoleCombo);
    m_lessonStructWidget->setVisible(false);
    calcFilesLayout->addWidget(m_lessonStructWidget);

    projectSplitter->addWidget(calcFilesWidget);

    projectSplitter->setStretchFactor(0, 1);
    projectSplitter->setStretchFactor(1, 2);
    projectLayout->addWidget(projectSplitter, 1);

    // Navigation panel embedded as a tab so the left dock has one stable tab bar
    // (Project, Bookmarks, Workspaces, Remote). Avoids the Qt tab-bar collapse that
    // happens when two separate QDockWidgets are tabified and one is hidden.
    m_navigationDock = new NavigationDock(nullptr, this);

    m_mainTabs = new QTabWidget(this);
    m_mainTabs->setTabPosition(QTabWidget::North);
    m_mainTabs->setDocumentMode(true);
    m_mainTabs->addTab(projectWidget, tr("Project"));
    m_mainTabs->addTab(m_navigationDock, tr("Navigation"));

    setWidget(m_mainTabs);
}

QPushButton* ProjectDock::chooseDirectoryButton() const { return m_chooseDirectory; }
BreadcrumbBar* ProjectDock::breadcrumbBar() const { return m_breadcrumbBar; }
QListView* ProjectDock::projectListView() const { return m_projectListView; }
QFileSystemModel* ProjectDock::projectModel() const { return m_projectModel; }
QPushButton* ProjectDock::newCalculationButton() const { return m_newCalculationButton; }
QLabel* ProjectDock::currentProjectLabel() const { return m_currentProjectLabel; }
QLabel* ProjectDock::stateIcon() const { return m_stateIcon; }
QLabel* ProjectDock::stateIndicator() const { return m_stateIndicator; }
QPushButton* ProjectDock::copyPathButton() const { return m_copyPathButton; }
QToolButton* ProjectDock::filesModeButton() const { return m_filesModeBtn; }
QToolButton* ProjectDock::lessonModeButton() const { return m_lessonModeBtn; }
QListView* ProjectDock::directoryContentView() const { return m_directoryContentView; }
QFileSystemModel* ProjectDock::directoryContentModel() const { return m_directoryContentModel; }
QWidget* ProjectDock::lessonMetaWidget() const { return m_lessonMetaWidget; }
QLineEdit* ProjectDock::lessonTitleEdit() const { return m_lessonTitleEdit; }
QLineEdit* ProjectDock::lessonDescEdit() const { return m_lessonDescEdit; }
QLabel* ProjectDock::lessonAuthorsLabel() const { return m_lessonAuthorsLabel; }
QWidget* ProjectDock::lessonStructWidget() const { return m_lessonStructWidget; }
QLineEdit* ProjectDock::structNameEdit() const { return m_structNameEdit; }
QLineEdit* ProjectDock::structDescEdit() const { return m_structDescEdit; }
QComboBox* ProjectDock::structRoleCombo() const { return m_structRoleCombo; }
QToolButton* ProjectDock::editAuthorsButton() const { return m_editAuthorsButton; }
NavigationDock* ProjectDock::navigationDock() const { return m_navigationDock; }
BookmarkWidget* ProjectDock::bookmarkWidget() const {
    return m_navigationDock ? m_navigationDock->bookmarkWidget() : nullptr;
}
WorkspacePanel* ProjectDock::workspacePanel() const {
    return m_navigationDock ? m_navigationDock->workspacePanel() : nullptr;
}
RemoteDirectoriesPanel* ProjectDock::remotePanel() const {
    return m_navigationDock ? m_navigationDock->remotePanel() : nullptr;
}
