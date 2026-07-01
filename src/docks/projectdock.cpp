// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// ProjectDock implementation.
//
// Claude Generated 2026 - Dock system restructuring.

#include "projectdock.h"

#include "bookmarkwidget.h"
#include "workspacepanel.h"
#ifdef USE_SFTP
#include "remotedirectoriespanel.h"
#endif
#include "settings.h"
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
#include <QMenu>
#include <QPushButton>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

/** @brief Proxy model that combines a live name search with an extension
 *  include/exclude filter. Owned and used only by ProjectDock.
 *  Claude Generated 2026. */
class DirectoryFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit DirectoryFilterProxyModel(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
    }

    void setExtensionPatterns(const QStringList& patterns, bool hasSelection)
    {
        m_patterns = patterns;
        m_hasExtensionSelection = hasSelection;
        invalidate();
    }

    void setNamePattern(const QString& pattern)
    {
        m_namePattern = pattern.trimmed();
        invalidate();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        auto* fsModel = qobject_cast<QFileSystemModel*>(sourceModel());
        if (!fsModel)
            return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);

        const QModelIndex index = fsModel->index(sourceRow, 0, sourceParent);
        if (!index.isValid())
            return false;

        const QFileInfo info = fsModel->fileInfo(index);

        // Always keep directories in the proxy mapping; they are needed for
        // the view root index and are not shown as list items anyway (the
        // source QFileSystemModel is set to QDir::Files). Claude Generated 2026.
        if (info.isDir())
            return true;

        const QString suffix = info.suffix().toLower();

        // Extension subset filter: if the user made a selection (even an empty
        // one), only files whose suffix is in the selected set are kept. When
        // no selection exists at all, every file passes. Claude Generated 2026.
        if (m_hasExtensionSelection && !m_patterns.contains(suffix))
            return false;

        // Live name search (substring, case-insensitive)
        if (!m_namePattern.isEmpty()) {
            const QString name = fsModel->fileName(index).toLower();
            if (!name.contains(m_namePattern.toLower()))
                return false;
        }

        return true;
    }

private:
    QStringList m_patterns;
    QString m_namePattern;
    bool m_hasExtensionSelection = false;
};

ProjectDock::ProjectDock(Settings* settings, QWidget* parent)
    : QDockWidget(DockConfig::ProjectDockTitle, parent)
    , m_settings(settings)
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

    // Top row: choose directory + segment switcher
    QWidget* topRow = new QWidget;
    QHBoxLayout* topLayout = new QHBoxLayout(topRow);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(4);

    m_chooseDirectory = new QPushButton;
    m_chooseDirectory->setIcon(QIcon::fromTheme("folder-open", QIcon(":/icons/folder.png")));
    m_chooseDirectory->setToolTip(tr("Choose working directory"));
    m_chooseDirectory->setMaximumWidth(34);
    topLayout->addWidget(m_chooseDirectory);

    QButtonGroup* segmentGroup = new QButtonGroup(this);
    segmentGroup->setExclusive(true);
    auto makeSegmentBtn = [this, segmentGroup](const QString& text, const QString& tip) {
        QToolButton* btn = new QToolButton;
        btn->setText(text);
        btn->setCheckable(true);
        btn->setToolTip(tip);
        segmentGroup->addButton(btn);
        return btn;
    };
    m_filesSegmentBtn = makeSegmentBtn(tr("Files"), tr("Show directories in the working directory"));
    m_filesSegmentBtn->setChecked(true);
    m_bookmarksSegmentBtn = makeSegmentBtn(tr("Bookmarks"), tr("Show bookmarked directories"));
    m_workspacesSegmentBtn = makeSegmentBtn(tr("Workspaces"), tr("Show saved workspaces"));
#ifdef USE_SFTP
    m_remoteSegmentBtn = makeSegmentBtn(tr("Remote"), tr("Show remote (SFTP) directories"));
#else
    m_remoteSegmentBtn = nullptr;
#endif

    QWidget* segmentWidget = new QWidget;
    QHBoxLayout* segmentLayout = new QHBoxLayout(segmentWidget);
    segmentLayout->setContentsMargins(0, 0, 0, 0);
    segmentLayout->setSpacing(0);
    segmentLayout->addWidget(m_filesSegmentBtn);
    segmentLayout->addWidget(m_bookmarksSegmentBtn);
    segmentLayout->addWidget(m_workspacesSegmentBtn);
#ifdef USE_SFTP
    segmentLayout->addWidget(m_remoteSegmentBtn);
#endif
    segmentWidget->setStyleSheet(QStringLiteral(
        "QToolButton { padding: 2px 10px; border: 1px solid palette(mid); }"
        "QToolButton:checked { background: palette(highlight); color: palette(highlighted-text);"
        " font-weight: bold; }"));
    topLayout->addWidget(segmentWidget);
    topLayout->addStretch();
    projectLayout->addWidget(topRow);

    m_breadcrumbBar = new BreadcrumbBar;
    m_breadcrumbBar->setHomeDirectory(QDir::homePath());
    projectLayout->addWidget(m_breadcrumbBar);

    // Upper stacked widget: Files / Bookmarks / Workspaces / Remote
    m_segmentStack = new QStackedWidget;

    // Files page
    QWidget* filesPage = new QWidget;
    QVBoxLayout* filesPageLayout = new QVBoxLayout(filesPage);
    filesPageLayout->setContentsMargins(0, 0, 0, 0);
    m_projectListView = new QListView;
    m_projectModel = new QFileSystemModel(this);
    m_projectModel->setFilter(QDir::AllDirs | QDir::NoDot);
    m_projectModel->setReadOnly(true);
    m_projectListView->setModel(m_projectModel);
    filesPageLayout->addWidget(m_projectListView);
    m_newCalculationButton = new QPushButton(tr("+ New Calculation Directory"));
    m_newCalculationButton->setIcon(QIcon::fromTheme("folder-new", QIcon()));
    m_newCalculationButton->setToolTip(tr("Create a new calculation directory (Ctrl+N)"));
    filesPageLayout->addWidget(m_newCalculationButton);
    m_segmentStack->addWidget(filesPage);

    // Bookmarks page
    m_bookmarkWidget = new BookmarkWidget(m_settings, this);
    connect(m_bookmarkWidget, &BookmarkWidget::directorySelected,
            this, [this](const QString& path) {
                emit bookmarkDirectorySelected(path);
                setCurrentSegment(ProjectSegment::Files);
            });
    connect(m_bookmarkWidget, &BookmarkWidget::bookmarksChanged,
            this, &ProjectDock::bookmarksChanged);
    m_segmentStack->addWidget(m_bookmarkWidget);

    // Workspaces page
    m_workspacePanel = new WorkspacePanel(this);
    connect(m_workspacePanel, &WorkspacePanel::saveWorkspaceRequested,
            this, &ProjectDock::saveWorkspaceRequested);
    m_segmentStack->addWidget(m_workspacePanel);

#ifdef USE_SFTP
    // Remote page
    m_remotePanel = new RemoteDirectoriesPanel(this);
    connect(m_remotePanel, &RemoteDirectoriesPanel::addRemoteRequested,
            this, &ProjectDock::addRemoteRequested);
    m_segmentStack->addWidget(m_remotePanel);
#endif

    connect(m_filesSegmentBtn, &QToolButton::clicked, this,
            [this]() { setCurrentSegment(ProjectSegment::Files); });
    connect(m_bookmarksSegmentBtn, &QToolButton::clicked, this,
            [this]() { setCurrentSegment(ProjectSegment::Bookmarks); });
    connect(m_workspacesSegmentBtn, &QToolButton::clicked, this,
            [this]() { setCurrentSegment(ProjectSegment::Workspaces); });
#ifdef USE_SFTP
    connect(m_remoteSegmentBtn, &QToolButton::clicked, this,
            [this]() { setCurrentSegment(ProjectSegment::Remote); });
#endif

    // Splitter: upper = segment stack, lower = files inside selected dir
    QSplitter* projectSplitter = new QSplitter(Qt::Vertical);
    projectSplitter->addWidget(m_segmentStack);

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

    // Filter / search panel above the content list (session-only, no persistence).
    QWidget* filterWidget = new QWidget;
    QHBoxLayout* filterLayout = new QHBoxLayout(filterWidget);
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->setSpacing(4);

    // Compact filter bar: search field + extension popup + clear button.
    // No labels or large list widgets to keep the dock clean. Claude Generated 2026.
    m_fileSearchEdit = new QLineEdit;
    m_fileSearchEdit->setPlaceholderText(tr("Search files…"));
    m_fileSearchEdit->setClearButtonEnabled(true);
    m_fileSearchEdit->setToolTip(tr("Filter files by name (substring, case-insensitive)"));
    m_fileSearchEdit->setMinimumWidth(200);
    filterLayout->addWidget(m_fileSearchEdit, 1);

    // Extension filter as a compact popup menu: all suffixes in the current
    // directory are shown as checkable actions. Checked = visible.
    m_extensionFilterButton = new QToolButton;
    m_extensionFilterButton->setText(tr("Extensions"));
    m_extensionFilterButton->setPopupMode(QToolButton::InstantPopup);
    m_extensionFilterButton->setToolTip(tr("Show/hide file extensions in the current directory"));
    m_extensionFilterButton->setMinimumWidth(90);

    m_extensionMenu = new QMenu(m_extensionFilterButton);
    m_extensionFilterButton->setMenu(m_extensionMenu);
    filterLayout->addWidget(m_extensionFilterButton);

    m_clearFilterButton = new QToolButton;
    m_clearFilterButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear")));
    m_clearFilterButton->setToolTip(tr("Clear search and extension filter"));
    m_clearFilterButton->setAutoRaise(true);
    filterLayout->addWidget(m_clearFilterButton);

    calcFilesLayout->addWidget(filterWidget);

    m_directoryContentView = new QListView;
    m_directoryContentModel = new QFileSystemModel(this);
    m_directoryContentModel->setFilter(QDir::NoDotAndDotDot | QDir::Files);
    // All files are read from the source model; the visible subset is decided
    // by the DirectoryFilterProxyModel (search + extension chooser).
    m_directoryContentModel->setNameFilters(QStringList() << "*");
    m_directoryContentModel->setNameFilterDisables(false);

    m_directoryContentProxyModel = new DirectoryFilterProxyModel(this);
    m_directoryContentProxyModel->setSourceModel(m_directoryContentModel);
    m_directoryContentView->setModel(m_directoryContentProxyModel);
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

    // Wire up the filter / search panel (session-only).
    connect(m_directoryContentModel, &QFileSystemModel::directoryLoaded,
            this, &ProjectDock::refreshExtensionMenu);

    connect(m_fileSearchEdit, &QLineEdit::textChanged,
            this, [this](const QString& text) {
                if (auto* proxy = qobject_cast<DirectoryFilterProxyModel*>(m_directoryContentProxyModel)) {
                    proxy->setNamePattern(text);
                    resetContentRoot();
                }
            });
    connect(m_clearFilterButton, &QToolButton::clicked, this, [this]() {
        m_fileSearchEdit->clear();
        setAllExtensionsChecked(true);
    });

    projectSplitter->addWidget(calcFilesWidget);

    projectSplitter->setStretchFactor(0, 1);
    projectSplitter->setStretchFactor(1, 2);
    projectLayout->addWidget(projectSplitter, 1);

    setWidget(projectWidget);
}

void ProjectDock::resetContentRoot()
{
    if (!m_directoryContentProxyModel || !m_directoryContentModel || !m_directoryContentView)
        return;
    const QString rootPath = m_directoryContentModel->rootPath();
    const QModelIndex sourceRoot = m_directoryContentModel->setRootPath(rootPath);
    const QModelIndex proxyRoot = m_directoryContentProxyModel->mapFromSource(sourceRoot);
    m_directoryContentView->setRootIndex(proxyRoot);
}

void ProjectDock::refreshExtensionMenu()
{
    if (!m_directoryContentModel || !m_extensionMenu)
        return;

    const QString rootPath = m_directoryContentModel->rootPath();
    const QModelIndex rootIndex = m_directoryContentModel->index(rootPath);
    QSet<QString> suffixes;
    const int rowCount = m_directoryContentModel->rowCount(rootIndex);
    for (int row = 0; row < rowCount; ++row) {
        const QModelIndex idx = m_directoryContentModel->index(row, 0, rootIndex);
        const QFileInfo info = m_directoryContentModel->fileInfo(idx);
        if (info.isDir())
            continue;
        QString suffix = info.suffix().toLower();
        if (suffix.isEmpty())
            suffix = tr("<no extension>");
        suffixes.insert(suffix);
    }

    // Remember newly discovered suffixes as checked unless the user previously
    // unchecked them. Suffixes that disappear are dropped from both sets.
    for (const QString& suffix : suffixes) {
        if (!m_uncheckedExtensions.contains(suffix))
            m_checkedExtensions.insert(suffix);
    }
    m_checkedExtensions.intersect(suffixes);
    m_uncheckedExtensions.intersect(suffixes);

    m_extensionMenu->clear();

    auto* allAction = m_extensionMenu->addAction(tr("Select all"));
    auto* noneAction = m_extensionMenu->addAction(tr("Select none"));
    m_extensionMenu->addSeparator();

    connect(allAction, &QAction::triggered, this, [this]() { setAllExtensionsChecked(true); });
    connect(noneAction, &QAction::triggered, this, [this]() { setAllExtensionsChecked(false); });

    QStringList sorted = suffixes.values();
    std::sort(sorted.begin(), sorted.end());
    for (const QString& suffix : sorted) {
        const bool checked = m_checkedExtensions.contains(suffix);
        QAction* action = m_extensionMenu->addAction(suffix);
        action->setCheckable(true);
        action->setChecked(checked);
        action->setData(suffix);
        connect(action, &QAction::toggled, this, &ProjectDock::updateExtensionFilterFromMenu);
    }

    updateExtensionFilterFromMenu();
}

void ProjectDock::updateExtensionFilterFromMenu()
{
    if (!m_extensionMenu || !m_directoryContentProxyModel)
        return;

    QStringList patterns;
    const bool menuHasCheckableActions = std::any_of(
        m_extensionMenu->actions().cbegin(), m_extensionMenu->actions().cend(),
        [](const QAction* a) { return a && a->isCheckable(); });

    for (QAction* action : m_extensionMenu->actions()) {
        if (!action->isCheckable())
            continue;
        const QString suffix = action->data().toString();
        if (suffix.isEmpty())
            continue;
        if (action->isChecked()) {
            patterns << suffix;
            m_checkedExtensions.insert(suffix);
            m_uncheckedExtensions.remove(suffix);
        } else {
            m_uncheckedExtensions.insert(suffix);
            m_checkedExtensions.remove(suffix);
        }
    }

    if (auto* proxy = qobject_cast<DirectoryFilterProxyModel*>(m_directoryContentProxyModel)) {
        proxy->setExtensionPatterns(patterns, menuHasCheckableActions);
        resetContentRoot();
    }

    const int visible = patterns.size();
    const int total = qMax(0, m_extensionMenu->actions().size() - 3); // header actions + separator
    m_extensionFilterButton->setText(tr("Extensions (%1/%2)").arg(visible).arg(total));
}

void ProjectDock::setAllExtensionsChecked(bool checked)
{
    if (!m_extensionMenu)
        return;

    for (QAction* action : m_extensionMenu->actions()) {
        if (action->isCheckable())
            action->setChecked(checked);
    }
    // updateExtensionFilterFromMenu is triggered by the toggled signals.
}

void ProjectDock::setCurrentSegment(ProjectSegment segment)
{
    if (!m_segmentStack)
        return;
    int index = 0;
    switch (segment) {
    case ProjectSegment::Files: index = 0; break;
    case ProjectSegment::Bookmarks: index = 1; break;
    case ProjectSegment::Workspaces: index = 2; break;
#ifdef USE_SFTP
    case ProjectSegment::Remote: index = 3; break;
#else
    case ProjectSegment::Remote: return;
#endif
    }
    if (index < m_segmentStack->count()) {
        m_segmentStack->setCurrentIndex(index);
        if (m_filesSegmentBtn) m_filesSegmentBtn->setChecked(segment == ProjectSegment::Files);
        if (m_bookmarksSegmentBtn) m_bookmarksSegmentBtn->setChecked(segment == ProjectSegment::Bookmarks);
        if (m_workspacesSegmentBtn) m_workspacesSegmentBtn->setChecked(segment == ProjectSegment::Workspaces);
#ifdef USE_SFTP
        if (m_remoteSegmentBtn) m_remoteSegmentBtn->setChecked(segment == ProjectSegment::Remote);
#endif
    }
}

ProjectDock::ProjectSegment ProjectDock::currentSegment() const
{
    if (!m_segmentStack)
        return ProjectSegment::Files;
    switch (m_segmentStack->currentIndex()) {
    case 1: return ProjectSegment::Bookmarks;
    case 2: return ProjectSegment::Workspaces;
#ifdef USE_SFTP
    case 3: return ProjectSegment::Remote;
#endif
    default: return ProjectSegment::Files;
    }
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
QSortFilterProxyModel* ProjectDock::directoryContentProxyModel() const { return m_directoryContentProxyModel; }
QWidget* ProjectDock::lessonMetaWidget() const { return m_lessonMetaWidget; }
QLineEdit* ProjectDock::lessonTitleEdit() const { return m_lessonTitleEdit; }
QLineEdit* ProjectDock::lessonDescEdit() const { return m_lessonDescEdit; }
QLabel* ProjectDock::lessonAuthorsLabel() const { return m_lessonAuthorsLabel; }
QToolButton* ProjectDock::editAuthorsButton() const { return m_editAuthorsButton; }
QWidget* ProjectDock::lessonStructWidget() const { return m_lessonStructWidget; }
QLineEdit* ProjectDock::structNameEdit() const { return m_structNameEdit; }
QLineEdit* ProjectDock::structDescEdit() const { return m_structDescEdit; }
QComboBox* ProjectDock::structRoleCombo() const { return m_structRoleCombo; }

QToolButton* ProjectDock::filesSegmentButton() const { return m_filesSegmentBtn; }
QToolButton* ProjectDock::bookmarksSegmentButton() const { return m_bookmarksSegmentBtn; }
QToolButton* ProjectDock::workspacesSegmentButton() const { return m_workspacesSegmentBtn; }
QToolButton* ProjectDock::remoteSegmentButton() const { return m_remoteSegmentBtn; }
QStackedWidget* ProjectDock::segmentStack() const { return m_segmentStack; }
BookmarkWidget* ProjectDock::bookmarkWidget() const { return m_bookmarkWidget; }
WorkspacePanel* ProjectDock::workspacePanel() const { return m_workspacePanel; }
#ifdef USE_SFTP
RemoteDirectoriesPanel* ProjectDock::remotePanel() const { return m_remotePanel; }
#endif

#include "projectdock.moc"
