// remotebrowserpanel.cpp - Remote segment of the Project dock, laid out like the local Files segment
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "remotebrowserpanel.h"

#include "directoryfilter.h"
#include "remote/hostprofiles.h"
#include "settings.h"
#include "sftpcache.h"
#include "sftpclient.h"
#include "sshconfig.h"
#include "widgets/breadcrumbbar.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStyle>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

namespace {
const QString kOtherHostId = QStringLiteral("__other__");      // combo entry that opens the host dialog
const QString kUnsavedHostId = QStringLiteral("__unsaved__");  // combo entry of a host that was not saved
constexpr int PathRole = Qt::UserRole + 1;   // absolute remote path of a directory item
constexpr int IndexRole = Qt::UserRole + 2;  // index into the panel's file entries

/// ~/.ssh/config entry whose alias equals @p host (invalid entry when there is none).
SshConfigEntry aliasFor(const QString& host)
{
    return SshConfigParser::configFileExists() ? SshConfigParser::findHost(host) : SshConfigEntry();
}

QIcon folderIcon()
{
    return QFileIconProvider().icon(QFileIconProvider::Folder);
}

/// ".." (unless @p dir is the root) and the subdirectories of @p dir, as items with PathRole.
void fillDirectoryModel(QStandardItemModel* model, const QString& dir, const QList<RemoteEntry>& entries)
{
    model->clear();
    if (dir != QLatin1String("/")) {
        auto* up = new QStandardItem(folderIcon(), QStringLiteral(".."));
        up->setEditable(false);
        up->setData(remotepaths::parent(dir), PathRole);
        up->setToolTip(remotepaths::parent(dir));
        model->appendRow(up);
    }
    for (const RemoteEntry& entry : entries) {
        if (!entry.isDir)
            continue;
        const QString path = remotepaths::join(dir, entry.name);
        auto* item = new QStandardItem(folderIcon(), entry.name);
        item->setEditable(false);
        item->setData(path, PathRole);
        item->setToolTip(entry.isLink ? RemoteBrowserPanel::tr("%1\nsymbolic link").arg(path) : path);
        model->appendRow(item);
    }
}
} // namespace

/** @brief Modal chooser of the remote working directory: breadcrumb, folder list, path field.
 *  Listings come from the panel's SftpClient through @p request; the panel feeds the answers back
 *  with showListing()/showError(). Select checks a typed path by listing it first. Claude Generated 2026 */
class RemoteDirectoryChooser : public QDialog {
public:
    RemoteDirectoryChooser(const QString& startPath, const QString& home,
                           std::function<void(const QString&)> request, QWidget* parent)
        : QDialog(parent)
        , m_home(home)
        , m_request(std::move(request))
    {
        setWindowTitle(RemoteBrowserPanel::tr("Choose Remote Working Directory"));
        resize(480, 420);
        auto* layout = new QVBoxLayout(this);
        m_crumb = new BreadcrumbBar;
        m_crumb->setHomeDirectory(home);
        m_list = new QListView;
        m_model = new QStandardItemModel(this);
        m_list->setModel(m_model);
        m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_pathEdit = new QLineEdit;
        m_pathEdit->setPlaceholderText(RemoteBrowserPanel::tr("/path, ~/path or a name in the current folder"));
        m_status = new QLabel;
        m_status->setWordWrap(true);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::Ok)->setText(RemoteBrowserPanel::tr("Select"));
        auto* pathRow = new QHBoxLayout;
        pathRow->addWidget(new QLabel(RemoteBrowserPanel::tr("Directory:")));
        pathRow->addWidget(m_pathEdit, 1);
        layout->addWidget(m_crumb);
        layout->addWidget(m_list, 1);
        layout->addLayout(pathRow);
        layout->addWidget(m_status);
        layout->addWidget(buttons);

        connect(m_crumb, &BreadcrumbBar::pathSelected, this, [this](const QString& path) { go(path); });
        connect(m_list, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
            go(index.data(PathRole).toString());
        });
        connect(m_list->selectionModel(), &QItemSelectionModel::currentChanged, this,
                [this](const QModelIndex& index) {
                    if (index.isValid())
                        m_pathEdit->setText(index.data(PathRole).toString());
                });
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() { trySelect(); });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        go(startPath.isEmpty() ? home : startPath);
    }

    void showListing(const QString& path, const QList<RemoteEntry>& entries)
    {
        if (path != m_requested)
            return;
        m_current = path;
        m_crumb->setPath(path);
        m_pathEdit->setText(path);
        m_status->setText(remotepaths::displayPath(path, m_home));
        fillDirectoryModel(m_model, path, entries);
        if (m_acceptWhenListed)
            accept();
    }

    void showError(const QString& path, const QString& message)
    {
        if (path != m_requested)
            return;
        m_acceptWhenListed = false;
        m_requested = m_current;
        m_status->setText(message);
    }

    QString selectedPath() const { return m_current; }

private:
    /// Absolute form of a typed path: "~" is the remote home, a relative name lies in the current folder.
    QString resolve(const QString& typed) const
    {
        const QString text = typed.trimmed();
        if (text == QLatin1String("~") || text.startsWith(QLatin1String("~/")))
            return remotepaths::join(m_home, text.mid(1).isEmpty() ? QStringLiteral(".") : text.mid(2));
        return text.startsWith(QLatin1Char('/')) ? remotepaths::normalise(text) : remotepaths::join(m_current, text);
    }

    void go(const QString& path)
    {
        if (path.trimmed().isEmpty())
            return;
        m_requested = resolve(path);
        m_status->setText(RemoteBrowserPanel::tr("Reading %1...").arg(m_requested));
        m_request(m_requested);
    }

    void trySelect()
    {
        const QString typed = m_pathEdit->text().trimmed();
        if (!typed.isEmpty() && resolve(typed) != m_current) {
            m_acceptWhenListed = true;  // check that the typed folder can be listed, then accept
            go(typed);
            return;
        }
        if (!m_current.isEmpty())
            accept();
    }

    QString m_home;
    std::function<void(const QString&)> m_request;
    BreadcrumbBar* m_crumb = nullptr;
    QListView* m_list = nullptr;
    QStandardItemModel* m_model = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QLabel* m_status = nullptr;
    QString m_current;
    QString m_requested;
    bool m_acceptWhenListed = false;
};

RemoteBrowserPanel::RemoteBrowserPanel(Settings* settings, QWidget* parent)
    : QWidget(parent)
    , m_settings(settings)
{
    qRegisterMetaType<RemoteEntry>();
    qRegisterMetaType<QList<RemoteEntry>>();
    setupUi();
    reloadSources();
    updateControls();
}

RemoteBrowserPanel::~RemoteBrowserPanel()
{
    abandonClient(2000);
}

/** @brief Compact connection row, breadcrumb row, directory list with "+ New Directory", and below
 *  the filter row with the file list, in a splitter like the local segment. Claude Generated 2026 */
void RemoteBrowserPanel::setupUi()
{
    // Same icon, size and tooltip pattern as the local button (ProjectDock::setupUI).
    m_chooseButton = new QPushButton(this);
    m_chooseButton->setIcon(QIcon::fromTheme("folder-open", QIcon(":/icons/folder.png")));
    m_chooseButton->setToolTip(tr("Choose the remote working directory"));
    m_chooseButton->setMaximumWidth(34);
    m_chooseButton->setVisible(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* sourceRow = new QHBoxLayout;
    sourceRow->setSpacing(4);
    m_sourceCombo = new QComboBox;
    m_sourceCombo->setPlaceholderText(tr("Choose a computer or host"));
    m_sourceCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_sourceCombo->setMinimumContentsLength(10);
    m_sourceCombo->setToolTip(tr("Computers of the computer list, saved SFTP profiles, or another host. "
                                 "SFTP uses only address, user and port; nothing has to run on the remote computer."));
    sourceRow->addWidget(m_sourceCombo, 1);
    m_connectButton = new QPushButton(tr("Connect"));
    sourceRow->addWidget(m_connectButton);
    layout->addLayout(sourceRow);

    m_statusLabel = new QLabel(tr("Not connected"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_statusLabel);

    m_progress = new QProgressBar;
    m_progress->setRange(0, 0);
    m_progress->setTextVisible(false);
    m_progress->setMaximumHeight(6);
    m_progress->setVisible(false);
    layout->addWidget(m_progress);

    auto* crumbRow = new QHBoxLayout;
    crumbRow->setSpacing(2);
    m_breadcrumb = new BreadcrumbBar;
    m_refreshButton = new QToolButton;
    m_refreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    m_refreshButton->setToolTip(tr("Read the remote folders again"));
    m_refreshButton->setAutoRaise(true);
    m_bookmarkButton = new QToolButton;
    m_bookmarkButton->setIcon(QIcon::fromTheme(QStringLiteral("bookmark-new")));
    m_bookmarkButton->setText(QStringLiteral("★"));
    m_bookmarkButton->setCheckable(true);
    m_bookmarkButton->setAutoRaise(true);
    m_bookmarkMenuButton = new QToolButton;
    m_bookmarkMenuButton->setIcon(QIcon::fromTheme(QStringLiteral("bookmarks")));
    m_bookmarkMenuButton->setText(tr("Bookmarks"));
    m_bookmarkMenuButton->setToolTip(tr("Remote bookmarks: choosing one makes it the remote working directory"));
    m_bookmarkMenuButton->setPopupMode(QToolButton::InstantPopup);
    m_bookmarkMenuButton->setAutoRaise(true);
    m_bookmarkMenu = new QMenu(m_bookmarkMenuButton);
    m_bookmarkMenu->setToolTipsVisible(true);
    m_bookmarkMenuButton->setMenu(m_bookmarkMenu);
    crumbRow->addWidget(m_breadcrumb, 1);
    crumbRow->addWidget(m_refreshButton);
    crumbRow->addWidget(m_bookmarkButton);
    crumbRow->addWidget(m_bookmarkMenuButton);
    layout->addLayout(crumbRow);

    // Upper part: subdirectories of the working directory (with ".."), like m_projectListView.
    auto* dirPage = new QWidget;
    auto* dirLayout = new QVBoxLayout(dirPage);
    dirLayout->setContentsMargins(0, 0, 0, 0);
    m_dirView = new QListView;
    m_dirModel = new QStandardItemModel(this);
    m_dirView->setModel(m_dirModel);
    m_dirView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dirView->setUniformItemSizes(true);
    m_dirView->setContextMenuPolicy(Qt::CustomContextMenu);
    dirLayout->addWidget(m_dirView);
    m_newDirButton = new QPushButton(tr("+ New Directory"));
    m_newDirButton->setIcon(QIcon::fromTheme("folder-new", QIcon()));
    m_newDirButton->setToolTip(tr("Create a directory in the remote working directory"));
    dirLayout->addWidget(m_newDirButton);

    // Lower part: filter row and the files of the directory selected above.
    auto* filePage = new QWidget;
    auto* fileLayout = new QVBoxLayout(filePage);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    m_fileProxy = new DirectoryFilterProxyModel(this);
    m_filterBar = new FileFilterBar(m_fileProxy);
    fileLayout->addWidget(m_filterBar);
    m_fileView = new QListView;
    m_fileModel = new QStandardItemModel(this);
    m_fileProxy->setSourceModel(m_fileModel);
    m_fileView->setModel(m_fileProxy);
    m_fileView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fileView->setUniformItemSizes(true);
    m_fileView->setContextMenuPolicy(Qt::CustomContextMenu);
    fileLayout->addWidget(m_fileView);

    auto* splitter = new QSplitter(Qt::Vertical);
    splitter->addWidget(dirPage);
    splitter->addWidget(filePage);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);

    connect(m_chooseButton, &QPushButton::clicked, this, &RemoteBrowserPanel::chooseWorkingDirectory);
    connect(m_sourceCombo, &QComboBox::activated, this, &RemoteBrowserPanel::onSourceActivated);
    connect(m_sourceCombo, &QComboBox::currentIndexChanged, this, [this]() { updateControls(); });
    connect(m_connectButton, &QPushButton::clicked, this, &RemoteBrowserPanel::onConnectButtonClicked);
    connect(m_refreshButton, &QToolButton::clicked, this, &RemoteBrowserPanel::refresh);
    connect(m_breadcrumb, &BreadcrumbBar::pathSelected, this, &RemoteBrowserPanel::setWorkingDirectory);
    connect(m_bookmarkButton, &QToolButton::clicked, this, &RemoteBrowserPanel::toggleBookmark);
    connect(m_bookmarkMenu, &QMenu::aboutToShow, this, &RemoteBrowserPanel::rebuildBookmarkMenu);
    // Same as the local list (MainWindow::setupConnections): a click and a change of the current
    // item select the directory whose files are shown below; no double-click action.
    connect(m_dirView, &QListView::clicked, this, &RemoteBrowserPanel::onDirectorySelected);
    connect(m_dirView->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& current) { onDirectorySelected(current); });
    connect(m_dirView, &QWidget::customContextMenuRequested, this, &RemoteBrowserPanel::showDirectoryContextMenu);
    connect(m_newDirButton, &QPushButton::clicked, this, &RemoteBrowserPanel::createDirectory);
    connect(m_fileView, &QListView::doubleClicked, this, &RemoteBrowserPanel::onFileDoubleClicked);
    connect(m_fileView, &QWidget::customContextMenuRequested, this, &RemoteBrowserPanel::showFileContextMenu);
}

QPushButton* RemoteBrowserPanel::chooseWorkDirButton() const
{
    return m_chooseButton;
}

void RemoteBrowserPanel::setWorkingDirectoryProvider(std::function<QString()> provider)
{
    m_workingDirectory = std::move(provider);
}

void RemoteBrowserPanel::focusSourceCombo()
{
    m_sourceCombo->setFocus(Qt::OtherFocusReason);
}

void RemoteBrowserPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    reloadSources();  // computers or profiles may have been added elsewhere meanwhile
}

/** @brief Computers, then SFTP profiles, then an unsaved host and "Other host...". Claude Generated 2026 */
void RemoteBrowserPanel::reloadSources()
{
    const QString keep = selectedSourceId();
    const QSignalBlocker blocker(m_sourceCombo);
    m_sourceCombo->clear();

    for (const remote::HostProfile& computer : remote::HostProfiles::load()) {
        m_sourceCombo->addItem(QIcon::fromTheme(QStringLiteral("computer")), computer.name,
                               remotepaths::computerSourceId(computer.name));
        m_sourceCombo->setItemData(m_sourceCombo->count() - 1, computer.target, Qt::ToolTipRole);
    }
    const auto profiles = m_settings ? m_settings->sftpProfiles() : QVector<Settings::SftpConnectionProfile>();
    if (!profiles.isEmpty() && m_sourceCombo->count() > 0)
        m_sourceCombo->insertSeparator(m_sourceCombo->count());
    for (const auto& profile : profiles) {
        m_sourceCombo->addItem(QIcon::fromTheme(QStringLiteral("folder-remote")), profile.name, profile.id);
        QString address = profile.username.isEmpty() ? profile.host : profile.username + QLatin1Char('@') + profile.host;
        if (profile.port > 0)
            address += QLatin1Char(':') + QString::number(profile.port);
        m_sourceCombo->setItemData(m_sourceCombo->count() - 1, address, Qt::ToolTipRole);
    }
    if (m_unsavedHost.isValid()) {
        if (m_sourceCombo->count() > 0)
            m_sourceCombo->insertSeparator(m_sourceCombo->count());
        m_sourceCombo->addItem(m_unsavedHost.label, kUnsavedHostId);
    }
    if (m_sourceCombo->count() > 0)
        m_sourceCombo->insertSeparator(m_sourceCombo->count());
    m_sourceCombo->addItem(QIcon::fromTheme(QStringLiteral("list-add")), tr("Other host..."), kOtherHostId);

    const int index = keep.isEmpty() || keep == kOtherHostId ? -1 : m_sourceCombo->findData(keep);
    m_sourceCombo->setCurrentIndex(index);
    m_lastSourceIndex = index;
    updateControls();
}

QString RemoteBrowserPanel::selectedSourceId() const
{
    return m_sourceCombo->currentIndex() < 0 ? QString() : m_sourceCombo->currentData().toString();
}

/// True when the combo shows the source of the open (or opening) connection.
bool RemoteBrowserPanel::selectionIsConnection() const
{
    const QString selected = selectedSourceId();
    if (selected == kUnsavedHostId)
        return m_connection.sourceId.isEmpty();
    return !selected.isEmpty() && selected == m_connection.sourceId;
}

/** @brief Connection parameters of a combo entry or bookmark source. Claude Generated 2026 */
RemoteConnection RemoteBrowserPanel::connectionForSource(const QString& sourceId) const
{
    if (sourceId == kUnsavedHostId)
        return m_unsavedHost;
    if (remotepaths::isComputerSourceId(sourceId)) {
        const auto computer = remote::HostProfiles::find(remotepaths::computerNameFromSourceId(sourceId));
        if (!computer)
            return {};
        const SshConfigEntry alias = aliasFor(computer->hostPart());
        return remotepaths::connectionForComputer(*computer, alias.isValid() ? &alias : nullptr);
    }
    if (!m_settings)
        return {};
    for (const auto& profile : m_settings->sftpProfiles()) {
        if (profile.id != sourceId)
            continue;
        const SshConfigEntry alias = aliasFor(profile.host);
        return remotepaths::connectionForHost(profile.id, profile.name, profile.host, profile.username,
                                              profile.port, alias.isValid() ? &alias : nullptr);
    }
    return {};
}

QString RemoteBrowserPanel::sourceLabel(const QString& sourceId) const
{
    if (remotepaths::isComputerSourceId(sourceId))
        return remotepaths::computerNameFromSourceId(sourceId);
    if (m_settings)
        for (const auto& profile : m_settings->sftpProfiles())
            if (profile.id == sourceId)
                return profile.name;
    return tr("profile removed");
}

void RemoteBrowserPanel::setStatus(const QString& text, bool error)
{
    m_statusLabel->setText(text);
    m_statusLabel->setToolTip(text);
    m_statusLabel->setStyleSheet(error ? QStringLiteral("color: #d9534f;") : QString());
}

void RemoteBrowserPanel::beginOperation()
{
    ++m_pendingOperations;
    updateControls();
}

void RemoteBrowserPanel::endOperation()
{
    m_pendingOperations = qMax(0, m_pendingOperations - 1);
    if (m_pendingOperations == 0)
        m_progress->setRange(0, 0);
    updateControls();
}

/** @brief Sends a listing request; the answer carries the returned id. Claude Generated 2026 */
int RemoteBrowserPanel::requestList(const QString& path)
{
    if (m_state != State::Connected || !m_client || path.isEmpty())
        return -1;
    const int id = ++m_nextRequestId;
    beginOperation();
    SftpClient* client = m_client;
    QMetaObject::invokeMethod(client, [client, id, path]() { client->list(id, path); }, Qt::QueuedConnection);
    return id;
}

/** @brief Enables buttons and lists for the current state. Claude Generated 2026 */
void RemoteBrowserPanel::updateControls()
{
    const bool connected = m_state == State::Connected;
    const bool isConnection = m_state != State::Disconnected && selectionIsConnection();
    const QString selected = selectedSourceId();
    const bool canConnect = !selected.isEmpty() && selected != kOtherHostId;
    m_connectButton->setText(isConnection ? tr("Disconnect") : tr("Connect"));
    m_connectButton->setEnabled(isConnection || canConnect);
    m_chooseButton->setEnabled(connected || (m_state == State::Disconnected && canConnect));

    const bool hasWorkDir = connected && !m_workDir.isEmpty();
    m_refreshButton->setEnabled(hasWorkDir);
    m_newDirButton->setEnabled(hasWorkDir);
    m_breadcrumb->setEnabled(connected);
    m_dirView->setEnabled(connected);
    m_fileView->setEnabled(connected);
    m_filterBar->setEnabled(connected);

    const bool canBookmark = hasWorkDir && !m_connection.sourceId.isEmpty();
    const bool bookmarked = canBookmark && !bookmarkIdFor(m_workDir).isEmpty();
    m_bookmarkButton->setEnabled(canBookmark);
    {
        const QSignalBlocker blocker(m_bookmarkButton);
        m_bookmarkButton->setChecked(bookmarked);
    }
    if (hasWorkDir && m_connection.sourceId.isEmpty())
        m_bookmarkButton->setToolTip(tr("Save the host as an SFTP profile to bookmark its folders"));
    else
        m_bookmarkButton->setToolTip(bookmarked ? tr("Remove the bookmark of the remote working directory")
                                                : tr("Bookmark the remote working directory"));

    m_progress->setVisible(m_state == State::Connecting || m_pendingOperations > 0);
}

void RemoteBrowserPanel::onSourceActivated(int index)
{
    if (m_sourceCombo->itemData(index).toString() == kOtherHostId) {
        if (!askOtherHost()) {
            const QSignalBlocker blocker(m_sourceCombo);
            m_sourceCombo->setCurrentIndex(m_lastSourceIndex);
            updateControls();
        }
        return;
    }
    m_lastSourceIndex = index;
    updateControls();
}

/** @brief Small dialog for a host that is neither a computer nor a profile; connects on OK.
 *  Claude Generated 2026 */
bool RemoteBrowserPanel::askOtherHost()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Other Host"));
    auto* form = new QFormLayout(&dialog);

    auto* hostCombo = new QComboBox;
    hostCombo->setEditable(true);
    for (const SshConfigEntry& entry : SshConfigParser::configFileExists() ? SshConfigParser::parseFile() : QList<SshConfigEntry>())
        if (!entry.host.contains(QLatin1Char('*')) && !entry.host.contains(QLatin1Char('?')))
            hostCombo->addItem(entry.host);
    hostCombo->setCurrentIndex(-1);
    hostCombo->setEditText(QString());
    hostCombo->lineEdit()->setPlaceholderText(tr("host name, address or ~/.ssh/config alias"));
    auto* userEdit = new QLineEdit;
    userEdit->setPlaceholderText(tr("from ~/.ssh/config, else the local user name"));
    auto* portSpin = new QSpinBox;
    portSpin->setRange(0, 65535);
    portSpin->setSpecialValueText(tr("default"));
    portSpin->setValue(0);
    portSpin->setToolTip(tr("Default: the port of the ~/.ssh/config entry, otherwise 22"));
    auto* saveCheck = new QCheckBox(tr("Save as SFTP profile"));
    auto* nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText(tr("user@host"));
    nameEdit->setEnabled(false);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Connect"));

    form->addRow(tr("Host:"), hostCombo);
    form->addRow(tr("User:"), userEdit);
    form->addRow(tr("Port:"), portSpin);
    form->addRow(QString(), saveCheck);
    form->addRow(tr("Profile name:"), nameEdit);
    form->addRow(buttons);

    connect(saveCheck, &QCheckBox::toggled, nameEdit, &QWidget::setEnabled);
    auto updateOk = [hostCombo, buttons]() {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!hostCombo->currentText().trimmed().isEmpty());
    };
    connect(hostCombo, &QComboBox::editTextChanged, &dialog, updateOk);
    updateOk();
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return false;

    const QString host = hostCombo->currentText().trimmed();
    const QString user = userEdit->text().trimmed();
    const int port = portSpin->value();
    QString label = user.isEmpty() ? host : user + QLatin1Char('@') + host;
    QString sourceId = kUnsavedHostId;
    if (saveCheck->isChecked() && m_settings) {
        Settings::SftpConnectionProfile profile;
        profile.id = QUuid::createUuid().toString();
        profile.name = nameEdit->text().trimmed().isEmpty() ? label : nameEdit->text().trimmed();
        profile.host = host;
        profile.username = user;
        profile.port = port;  // 0 = not given (alias port or 22)
        profile.created = QDateTime::currentDateTime();
        profile.lastUsed = profile.created;
        m_settings->addSftpProfile(profile);
        sourceId = profile.id;
        m_unsavedHost = RemoteConnection();
        emit sftpProfilesChanged();
    } else {
        const SshConfigEntry alias = aliasFor(host);
        m_unsavedHost = remotepaths::connectionForHost(QString(), label, host, user, port,
                                                       alias.isValid() ? &alias : nullptr);
    }

    reloadSources();
    {
        const QSignalBlocker blocker(m_sourceCombo);
        m_sourceCombo->setCurrentIndex(m_sourceCombo->findData(sourceId));
        m_lastSourceIndex = m_sourceCombo->currentIndex();
    }
    startConnection(connectionForSource(sourceId), QString());
    return true;
}

void RemoteBrowserPanel::onConnectButtonClicked()
{
    if (m_state != State::Disconnected && selectionIsConnection()) {
        disconnectNow(tr("Disconnected"));
        return;
    }
    const QString sourceId = selectedSourceId();
    if (sourceId.isEmpty() || sourceId == kOtherHostId)
        return;
    startConnection(connectionForSource(sourceId), QString());
}

/** @brief Public entry (recent-connections menu): select @p sourceId and connect. Claude Generated 2026 */
void RemoteBrowserPanel::connectToSource(const QString& sourceId)
{
    reloadSources();
    const int index = m_sourceCombo->findData(sourceId);
    if (index < 0) {
        setStatus(tr("This computer or SFTP profile no longer exists."), true);
        return;
    }
    {
        const QSignalBlocker blocker(m_sourceCombo);
        m_sourceCombo->setCurrentIndex(index);
        m_lastSourceIndex = index;
    }
    if (m_state != State::Disconnected && m_connection.sourceId == sourceId) {
        updateControls();
        return;
    }
    startConnection(connectionForSource(sourceId), QString());
}

void RemoteBrowserPanel::clearListing()
{
    m_workDir.clear();
    m_requestedWorkDir.clear();
    m_selectedDir.clear();
    m_filesDir.clear();
    m_selectAfterListing.clear();
    m_workEntries.clear();
    m_fileEntries.clear();
    m_workDirRequest = m_filesRequest = m_chooserRequest = -1;
    m_dirModel->clear();
    m_fileModel->clear();
    m_filterBar->setAvailableSuffixes({});
    m_breadcrumb->setPath(QString());
}

/** @brief Starts a fresh SftpClient on its own thread and asks it to connect. Claude Generated 2026 */
void RemoteBrowserPanel::startConnection(const RemoteConnection& connection, const QString& startPath)
{
    abandonClient();
    m_state = State::Disconnected;
    m_chooseAfterConnect = false;
    m_home.clear();
    m_deferredError.clear();
    clearListing();
    if (!connection.isValid()) {
        setStatus(tr("This computer or SFTP profile no longer exists or has no address."), true);
        updateControls();
        return;
    }
    m_connection = connection;
    m_startPath = startPath;

    m_thread = new QThread;
    m_thread->setObjectName(QStringLiteral("SftpClient"));
    m_client = new SftpClient;
    m_client->moveToThread(m_thread);
    connect(m_client, &SftpClient::hostKeyUnknown, this, &RemoteBrowserPanel::onHostKeyUnknown);
    connect(m_client, &SftpClient::passwordRequired, this, &RemoteBrowserPanel::onPasswordRequired);
    connect(m_client, &SftpClient::connected, this, &RemoteBrowserPanel::onConnected);
    connect(m_client, &SftpClient::connectFailed, this, &RemoteBrowserPanel::onConnectFailed);
    connect(m_client, &SftpClient::listed, this, &RemoteBrowserPanel::onListed);
    connect(m_client, &SftpClient::listFailed, this, &RemoteBrowserPanel::onListFailed);
    connect(m_client, &SftpClient::downloadProgress, this, &RemoteBrowserPanel::onDownloadProgress);
    connect(m_client, &SftpClient::downloaded, this, &RemoteBrowserPanel::onDownloaded);
    connect(m_client, &SftpClient::downloadFailed, this, &RemoteBrowserPanel::onDownloadFailed);
    connect(m_client, &SftpClient::directoryCreated, this, &RemoteBrowserPanel::onDirectoryCreated);
    connect(m_client, &SftpClient::makeDirectoryFailed, this, &RemoteBrowserPanel::onMakeDirectoryFailed);
    m_thread->start();

    m_state = State::Connecting;
    setStatus(connection.proxyCommand.isEmpty() ? tr("Connecting to %1...").arg(connection.account())
                                                : tr("Connecting to %1 through a jump host...").arg(connection.account()));
    updateControls();
    SftpClient* client = m_client;
    QMetaObject::invokeMethod(client, [client, connection]() { client->connectTo(connection); }, Qt::QueuedConnection);
}

/** @brief Detaches the running client: its pending results are dropped, its blocking call may finish
 *  in the background; client and thread delete themselves afterwards. Claude Generated 2026 */
void RemoteBrowserPanel::abandonClient(int waitMs)
{
    m_pendingOperations = 0;
    m_downloadOpens.clear();
    if (!m_client)
        return;
    SftpClient* client = m_client;
    QThread* thread = m_thread;
    m_client = nullptr;
    m_thread = nullptr;
    client->requestCancel();
    disconnect(client, nullptr, this, nullptr);
    connect(client, &QObject::destroyed, thread, &QThread::quit, Qt::DirectConnection);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    QMetaObject::invokeMethod(client, [client]() {
        client->disconnectFromHost();
        client->deleteLater();
    }, Qt::QueuedConnection);
    if (waitMs > 0 && thread->wait(waitMs))
        delete thread;
}

void RemoteBrowserPanel::disconnectNow(const QString& status)
{
    abandonClient();
    m_state = State::Disconnected;
    m_chooseAfterConnect = false;
    clearListing();
    setStatus(status);
    updateControls();
}

bool RemoteBrowserPanel::fromCurrentClient() const
{
    return m_client && sender() == m_client;
}

void RemoteBrowserPanel::onHostKeyUnknown(const QString& host, const QString& fingerprint)
{
    if (!fromCurrentClient())
        return;
    SftpClient* client = m_client;
    const auto answer = QMessageBox::question(this, tr("Unknown Host Key"),
        tr("%1 is not listed in ~/.ssh/known_hosts yet.\n\nHost key: %2\n\n"
           "Compare it with the key published for this computer. Trust it and add it to ~/.ssh/known_hosts?")
            .arg(host, fingerprint.isEmpty() ? tr("(not available)") : fingerprint),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (client != m_client)
        return;  // disconnected while the question was open
    if (answer != QMessageBox::Yes) {
        disconnectNow(tr("Connection cancelled: the host key of %1 was not accepted.").arg(host));
        return;
    }
    setStatus(tr("Logging in to %1...").arg(m_connection.account()));
    QMetaObject::invokeMethod(client, [client]() { client->answerHostKey(true); }, Qt::QueuedConnection);
}

void RemoteBrowserPanel::onPasswordRequired(const QString& account)
{
    if (!fromCurrentClient())
        return;
    SftpClient* client = m_client;
    bool ok = false;
    const QString password = QInputDialog::getText(this, tr("SFTP Password"),
        tr("The ssh agent and the keys in ~/.ssh were not accepted.\nPassword for %1:").arg(account),
        QLineEdit::Password, QString(), &ok);
    if (client != m_client)
        return;
    if (!ok) {
        disconnectNow(tr("Connection cancelled."));
        return;
    }
    setStatus(tr("Logging in to %1...").arg(account));
    QMetaObject::invokeMethod(client, [client, password]() { client->authenticateWithPassword(password); },
                              Qt::QueuedConnection);
}

/** @brief Restores the working directory of this source (a bookmark wins, then the stored one,
 *  then the remote home). Claude Generated 2026 */
void RemoteBrowserPanel::onConnected(const QString& homePath)
{
    if (!fromCurrentClient())
        return;
    m_state = State::Connected;
    m_home = homePath;
    m_breadcrumb->setHomeDirectory(homePath);
    setStatus(tr("Connected to %1").arg(m_connection.account()));
    if (m_settings && !m_connection.sourceId.isEmpty() && !remotepaths::isComputerSourceId(m_connection.sourceId)) {
        m_settings->updateSftpProfileLastUsed(m_connection.sourceId);
        emit sftpProfilesChanged();
    }
    QString start = m_startPath;
    m_startPath.clear();
    if (start.isEmpty())
        start = QSettings().value(remotepaths::workDirSettingsKey(remotepaths::sourceKey(m_connection))).toString();
    if (start.isEmpty())
        start = homePath;
    updateControls();
    setWorkingDirectory(start);
    if (m_chooseAfterConnect) {
        m_chooseAfterConnect = false;
        QTimer::singleShot(0, this, &RemoteBrowserPanel::chooseWorkingDirectory);
    }
}

void RemoteBrowserPanel::onConnectFailed(const QString& message)
{
    if (!fromCurrentClient())
        return;
    abandonClient();
    m_state = State::Disconnected;
    m_chooseAfterConnect = false;
    setStatus(message, true);
    updateControls();
}

/** @brief Remote counterpart of MainWindow::switchWorkingDirectory: lists @p path; when that works it
 *  becomes the working directory and is remembered for this source. Claude Generated 2026 */
void RemoteBrowserPanel::setWorkingDirectory(const QString& path)
{
    if (m_state != State::Connected || path.isEmpty())
        return;
    m_requestedWorkDir = remotepaths::normalise(path);
    m_workDirRequest = requestList(m_requestedWorkDir);
}

void RemoteBrowserPanel::refresh()
{
    if (m_workDir.isEmpty())
        return;
    m_selectAfterListing = m_selectedDir != m_workDir ? m_selectedDir : QString();
    setWorkingDirectory(m_workDir);
}

void RemoteBrowserPanel::onListed(int requestId, const QString& path, const QList<RemoteEntry>& entries)
{
    if (!fromCurrentClient())
        return;
    endOperation();
    if (requestId == m_chooserRequest) {
        if (m_chooser)
            m_chooser->showListing(path, entries);
        return;
    }
    if (requestId == m_workDirRequest) {
        m_workDirRequest = -1;
        m_workDir = path;
        m_workEntries = entries;
        QSettings().setValue(remotepaths::workDirSettingsKey(remotepaths::sourceKey(m_connection)), path);
        m_breadcrumb->setPath(path);
        showDirectories();
        m_filesRequest = -1;
        m_selectedDir = path;
        showFiles(path, entries);  // nothing selected: the files of the working directory, as locally
        int folders = 0;
        for (const RemoteEntry& entry : entries)
            folders += entry.isDir ? 1 : 0;
        if (!m_deferredError.isEmpty()) {
            setStatus(m_deferredError, true);
            m_deferredError.clear();
        } else {
            setStatus(tr("%1: %2 (%3 folders, %4 files)")
                          .arg(m_connection.account(), remotepaths::displayPath(path, m_home))
                          .arg(folders)
                          .arg(entries.size() - folders));
        }
        const QString select = m_selectAfterListing;
        m_selectAfterListing.clear();
        if (!select.isEmpty()) {
            for (int row = 0; row < m_dirModel->rowCount(); ++row) {
                const QModelIndex index = m_dirModel->index(row, 0);
                if (index.data(PathRole).toString() == select) {
                    m_dirView->setCurrentIndex(index);  // selects it and lists its files
                    break;
                }
            }
        }
        updateControls();
        return;
    }
    if (requestId == m_filesRequest && path == m_selectedDir) {
        m_filesRequest = -1;
        showFiles(path, entries);
        setStatus(tr("%1: %2").arg(m_connection.account(), remotepaths::displayPath(path, m_home)));
    }
}

void RemoteBrowserPanel::onListFailed(int requestId, const QString& path, const QString& message)
{
    if (!fromCurrentClient())
        return;
    endOperation();
    if (requestId == m_chooserRequest) {
        if (m_chooser)
            m_chooser->showError(path, message);
        return;
    }
    if (requestId == m_workDirRequest) {
        m_workDirRequest = -1;
        if (m_workDir.isEmpty() && !m_home.isEmpty() && path != m_home) {
            // the stored or bookmarked working directory is gone or unreadable: use the home folder
            m_deferredError = message;
            setWorkingDirectory(m_home);
            return;
        }
        m_selectAfterListing.clear();
        setStatus(message, true);  // the previous working directory stays
        return;
    }
    if (requestId == m_filesRequest) {
        m_filesRequest = -1;
        setStatus(message, true);
    }
}

void RemoteBrowserPanel::showDirectories()
{
    fillDirectoryModel(m_dirModel, m_workDir, m_workEntries);
}

/** @brief Fills the lower list with the files of @p dir and reports their suffixes to the filter bar.
 *  Claude Generated 2026 */
void RemoteBrowserPanel::showFiles(const QString& dir, const QList<RemoteEntry>& entries)
{
    m_filesDir = dir;
    m_fileEntries.clear();
    for (const RemoteEntry& entry : entries)
        if (!entry.isDir)
            m_fileEntries.append(entry);
    m_fileModel->clear();
    const QLocale locale;
    for (int i = 0; i < m_fileEntries.size(); ++i) {
        const RemoteEntry& entry = m_fileEntries.at(i);
        auto* item = new QStandardItem(iconFor(entry), entry.name);
        item->setEditable(false);
        item->setData(false, DirectoryFilterProxyModel::IsDirRole);
        item->setData(i, IndexRole);
        QString tip = remotepaths::join(dir, entry.name) + QLatin1Char('\n') + locale.formattedDataSize(entry.size);
        if (entry.modified.isValid())
            tip += QLatin1Char('\n') + locale.toString(entry.modified, QLocale::ShortFormat);
        if (entry.isLink)
            tip += QLatin1Char('\n') + tr("symbolic link");
        item->setToolTip(tip);
        m_fileModel->appendRow(item);
    }
    m_filterBar->setAvailableSuffixes(remotepaths::collectSuffixes(m_fileEntries));
}

/** @brief Remote counterpart of MainWindow::projectSelected: the lower list shows the files of the
 *  clicked directory ("..": of the parent). Claude Generated 2026 */
void RemoteBrowserPanel::onDirectorySelected(const QModelIndex& index)
{
    if (!index.isValid() || m_state != State::Connected)
        return;
    const QString path = index.data(PathRole).toString();
    if (path.isEmpty() || path == m_selectedDir)
        return;
    m_selectedDir = path;
    if (path == m_workDir) {
        m_filesRequest = -1;
        showFiles(path, m_workEntries);
        return;
    }
    m_fileModel->clear();
    m_fileEntries.clear();
    m_filesRequest = requestList(path);
}

/** @brief Same entries as the local directory list: Add to Bookmarks, Set as Working Directory.
 *  Claude Generated 2026 */
void RemoteBrowserPanel::showDirectoryContextMenu(const QPoint& pos)
{
    const QModelIndex index = m_dirView->indexAt(pos);
    if (!index.isValid() || m_state != State::Connected)
        return;
    const QString path = index.data(PathRole).toString();
    QMenu menu(this);
    QAction* bookmark = menu.addAction(tr("Add to Bookmarks"));
    bookmark->setEnabled(!m_connection.sourceId.isEmpty() && bookmarkIdFor(path).isEmpty());
    connect(bookmark, &QAction::triggered, this, [this, path]() { addBookmark(path); });
    QAction* setWorkDir = menu.addAction(tr("Set as Working Directory"));
    connect(setWorkDir, &QAction::triggered, this, [this, path]() { setWorkingDirectory(path); });
    menu.exec(m_dirView->viewport()->mapToGlobal(pos));
}

/** @brief Opens the modal chooser; connects first when the selected source is not connected yet.
 *  Claude Generated 2026 */
void RemoteBrowserPanel::chooseWorkingDirectory()
{
    if (m_state != State::Connected) {
        const QString sourceId = selectedSourceId();
        if (m_state == State::Connecting) {
            m_chooseAfterConnect = true;
            return;
        }
        if (sourceId.isEmpty() || sourceId == kOtherHostId) {
            setStatus(tr("Choose a computer or host first."), true);
            focusSourceCombo();
            return;
        }
        startConnection(connectionForSource(sourceId), QString());
        m_chooseAfterConnect = m_state == State::Connecting;
        return;
    }
    const QString start = m_workDir.isEmpty() ? m_requestedWorkDir : m_workDir;
    SftpClient* client = m_client;
    RemoteDirectoryChooser chooser(start, m_home, [this](const QString& path) { m_chooserRequest = requestList(path); }, this);
    m_chooser = &chooser;
    const int result = chooser.exec();
    m_chooser = nullptr;
    m_chooserRequest = -1;
    if (result == QDialog::Accepted && client == m_client && m_state == State::Connected && !chooser.selectedPath().isEmpty())
        setWorkingDirectory(chooser.selectedPath());
}

/** @brief "+ New Directory": a validated name in the remote working directory (sftp_mkdir).
 *  Claude Generated 2026 */
void RemoteBrowserPanel::createDirectory()
{
    if (m_state != State::Connected || m_workDir.isEmpty() || !m_client)
        return;
    QStringList existing;
    for (const RemoteEntry& entry : m_workEntries)
        existing << entry.name;
    // like the local button: with a subdirectory selected, suggest "<its name>_N"
    QString suggestion;
    if (m_selectedDir != m_workDir && remotepaths::parent(m_selectedDir) == m_workDir)
        suggestion = remotepaths::nextFreeName(m_selectedDir.section(QLatin1Char('/'), -1), existing);
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New Directory"),
        tr("Directory name in %1:").arg(remotepaths::displayPath(m_workDir, m_home)), QLineEdit::Normal, suggestion, &ok);
    if (!ok)
        return;
    QString why;
    if (!remotepaths::isValidNewDirectoryName(name, &why)) {
        QMessageBox::warning(this, tr("New Directory"), why);
        return;
    }
    if (existing.contains(name)) {
        QMessageBox::warning(this, tr("New Directory"), tr("A file or directory with this name already exists."));
        return;
    }
    if (!m_client || m_state != State::Connected)
        return;  // disconnected while the dialog was open
    beginOperation();
    SftpClient* client = m_client;
    const QString path = remotepaths::join(m_workDir, name);
    QMetaObject::invokeMethod(client, [client, path]() { client->makeDirectory(path); }, Qt::QueuedConnection);
}

void RemoteBrowserPanel::onDirectoryCreated(const QString& path)
{
    if (!fromCurrentClient())
        return;
    endOperation();
    setStatus(tr("Created %1").arg(remotepaths::displayPath(path, m_home)));
    if (remotepaths::parent(path) == m_workDir) {
        m_selectAfterListing = path;  // select the new directory, as the local button does
        setWorkingDirectory(m_workDir);
    }
}

void RemoteBrowserPanel::onMakeDirectoryFailed(const QString& path, const QString& message)
{
    Q_UNUSED(path)
    if (!fromCurrentClient())
        return;
    endOperation();
    setStatus(message, true);
}

void RemoteBrowserPanel::onDownloadProgress(const QString& remotePath, qint64 received, qint64 total)
{
    if (!fromCurrentClient())
        return;
    const QLocale locale;
    const QString name = remotePath.section(QLatin1Char('/'), -1);
    if (total > 0) {
        m_progress->setRange(0, 1000);
        m_progress->setValue(static_cast<int>(qBound(qint64(0), received * 1000 / total, qint64(1000))));
        setStatus(tr("Downloading %1: %2 of %3").arg(name, locale.formattedDataSize(received), locale.formattedDataSize(total)));
    } else {
        setStatus(tr("Downloading %1: %2").arg(name, locale.formattedDataSize(received)));
    }
}

void RemoteBrowserPanel::onDownloaded(const QString& remotePath, const QString& localPath)
{
    if (!fromCurrentClient())
        return;
    endOperation();
    const QString name = remotePath.section(QLatin1Char('/'), -1);
    if (m_downloadOpens.take(localPath)) {
        setStatus(tr("Opened %1 from %2").arg(name, m_connection.account()));
        emit openFileRequested(localPath);
    } else {
        setStatus(tr("Saved %1 to %2").arg(name, QDir::toNativeSeparators(localPath)));
    }
}

void RemoteBrowserPanel::onDownloadFailed(const QString& remotePath, const QString& message)
{
    Q_UNUSED(remotePath)
    if (!fromCurrentClient())
        return;
    endOperation();
    setStatus(message, true);
}

const RemoteEntry* RemoteBrowserPanel::fileEntryAt(const QModelIndex& proxyIndex) const
{
    if (!proxyIndex.isValid())
        return nullptr;
    const int i = m_fileProxy->mapToSource(proxyIndex).data(IndexRole).toInt();
    return i >= 0 && i < m_fileEntries.size() ? &m_fileEntries.at(i) : nullptr;
}

void RemoteBrowserPanel::onFileDoubleClicked(const QModelIndex& index)
{
    if (const RemoteEntry* entry = fileEntryAt(index))
        openRemoteFile(*entry, remotepaths::join(m_filesDir, entry->name));
}

/** @brief Opens a remote file: an unchanged cached copy directly, otherwise after a download into
 *  the SftpCache directory (<cache>/<sha256 of host and path>/<file name>). Claude Generated 2026 */
void RemoteBrowserPanel::openRemoteFile(const RemoteEntry& entry, const QString& remotePath)
{
    if (!m_client)
        return;
    SftpCache cache;
    const QString keyDir = cache.prepareCachePath(m_connection.cacheKey(), remotePath, false);
    const QString localPath = QDir(keyDir).filePath(entry.name);
    const QFileInfo cached(localPath);
    if (cached.isFile() && cached.size() == entry.size
        && (!entry.modified.isValid() || cached.lastModified() >= entry.modified)) {
        setStatus(tr("Opened %1 (unchanged since the last download)").arg(entry.name));
        emit openFileRequested(localPath);
        return;
    }
    if (!QDir().mkpath(keyDir)) {
        setStatus(tr("Cannot create the cache folder %1").arg(keyDir), true);
        return;
    }
    m_downloadOpens.insert(localPath, true);
    beginOperation();
    setStatus(tr("Downloading %1...").arg(entry.name));
    SftpClient* client = m_client;
    const qint64 size = entry.size;
    QMetaObject::invokeMethod(client, [client, remotePath, localPath, size]() { client->download(remotePath, localPath, size); },
                              Qt::QueuedConnection);
}

void RemoteBrowserPanel::downloadToWorkingDirectory(const RemoteEntry& entry, const QString& remotePath)
{
    if (!m_client)
        return;
    const QString dir = m_workingDirectory ? m_workingDirectory() : QString();
    if (dir.isEmpty() || !QFileInfo(dir).isDir()) {
        setStatus(tr("Choose a local working directory first (Files segment)."), true);
        return;
    }
    const QString target = QDir(dir).filePath(entry.name);
    if (QFileInfo::exists(target)
        && QMessageBox::question(this, tr("Replace File"),
                                 tr("%1 already exists in the local working directory. Replace it?").arg(entry.name))
            != QMessageBox::Yes)
        return;
    if (!m_client)
        return;  // disconnected while the question was open
    m_downloadOpens.insert(target, false);
    beginOperation();
    setStatus(tr("Downloading %1...").arg(entry.name));
    SftpClient* client = m_client;
    const qint64 size = entry.size;
    QMetaObject::invokeMethod(client, [client, remotePath, target, size]() { client->download(remotePath, target, size); },
                              Qt::QueuedConnection);
}

/** @brief Open, Download to Local Working Directory, Copy Remote Path, Add to Bookmarks. Claude Generated 2026 */
void RemoteBrowserPanel::showFileContextMenu(const QPoint& pos)
{
    if (m_state != State::Connected || m_filesDir.isEmpty())
        return;
    const RemoteEntry* found = fileEntryAt(m_fileView->indexAt(pos));
    const QString folder = m_filesDir;
    QString copyPath = folder;
    QMenu menu(this);
    if (found) {
        const RemoteEntry entry = *found;  // copy: a listing may arrive while the menu is open
        const QString path = remotepaths::join(folder, entry.name);
        copyPath = path;
        QAction* open = menu.addAction(tr("Open"));
        connect(open, &QAction::triggered, this, [this, entry, path]() { openRemoteFile(entry, path); });
        QAction* save = menu.addAction(tr("Download to Local Working Directory"));
        connect(save, &QAction::triggered, this, [this, entry, path]() { downloadToWorkingDirectory(entry, path); });
        menu.addSeparator();
    }
    QAction* copy = menu.addAction(tr("Copy Remote Path"));
    connect(copy, &QAction::triggered, this, [copyPath]() { QGuiApplication::clipboard()->setText(copyPath); });
    QAction* bookmark = menu.addAction(found ? tr("Add This Folder to Bookmarks") : tr("Add to Bookmarks"));
    bookmark->setEnabled(!m_connection.sourceId.isEmpty() && bookmarkIdFor(folder).isEmpty());
    connect(bookmark, &QAction::triggered, this, [this, folder]() { addBookmark(folder); });
    if (!found) {
        menu.addSeparator();
        QAction* refreshAction = menu.addAction(tr("Refresh"));
        connect(refreshAction, &QAction::triggered, this, &RemoteBrowserPanel::refresh);
    }
    menu.exec(m_fileView->viewport()->mapToGlobal(pos));
}

QIcon RemoteBrowserPanel::iconFor(const RemoteEntry& entry)
{
    const QString key = entry.isDir ? QStringLiteral("/") : remotepaths::entrySuffix(entry.name);
    const auto it = m_iconCache.constFind(key);
    if (it != m_iconCache.constEnd())
        return it.value();
    const QFileIconProvider provider;
    QIcon icon;
    if (entry.isDir) {
        icon = provider.icon(QFileIconProvider::Folder);
    } else {
        const QMimeType mime = QMimeDatabase().mimeTypeForFile(entry.name, QMimeDatabase::MatchExtension);
        icon = QIcon::fromTheme(mime.iconName(),
                                QIcon::fromTheme(mime.genericIconName(), provider.icon(QFileIconProvider::File)));
    }
    m_iconCache.insert(key, icon);
    return icon;
}

QString RemoteBrowserPanel::bookmarkIdFor(const QString& path) const
{
    if (!m_settings || m_connection.sourceId.isEmpty() || path.isEmpty())
        return QString();
    const QString normalised = remotepaths::normalise(path);
    for (const auto& mount : m_settings->remoteMounts())
        if (mount.profileId == m_connection.sourceId && remotepaths::normalise(mount.remotePath) == normalised)
            return mount.id;
    return QString();
}

/** @brief Stores @p path of the connected source as a remote bookmark (Settings::RemoteMountPoint).
 *  Claude Generated 2026 */
void RemoteBrowserPanel::addBookmark(const QString& path)
{
    if (!m_settings || m_connection.sourceId.isEmpty() || path.isEmpty() || !bookmarkIdFor(path).isEmpty())
        return;
    Settings::RemoteMountPoint mount;
    mount.id = QUuid::createUuid().toString();
    mount.name = remotepaths::displayPath(path, m_home);
    mount.profileId = m_connection.sourceId;
    mount.remotePath = remotepaths::normalise(path);
    mount.mounted = QDateTime::currentDateTime();
    mount.lastAccessed = mount.mounted;
    m_settings->addRemoteMount(mount);
    setStatus(tr("Bookmarked %1 on %2").arg(mount.name, m_connection.label));
    updateControls();
}

/** @brief The star: bookmark (or un-bookmark) the remote working directory. Claude Generated 2026 */
void RemoteBrowserPanel::toggleBookmark()
{
    const QString id = bookmarkIdFor(m_workDir);
    if (!id.isEmpty() && m_settings) {
        m_settings->removeRemoteMount(id);
        setStatus(tr("Bookmark removed: %1").arg(remotepaths::displayPath(m_workDir, m_home)));
        updateControls();
    } else {
        addBookmark(m_workDir);
    }
}

void RemoteBrowserPanel::rebuildBookmarkMenu()
{
    m_bookmarkMenu->clear();
    const auto mounts = m_settings ? m_settings->remoteMounts() : QVector<Settings::RemoteMountPoint>();
    if (mounts.isEmpty()) {
        m_bookmarkMenu->addAction(tr("No remote bookmarks yet"))->setEnabled(false);
        return;
    }
    QMenu* removeMenu = new QMenu(tr("Remove Bookmark"), m_bookmarkMenu);
    for (const auto& mount : mounts) {
        const QString text = tr("%1 (%2)").arg(mount.name, sourceLabel(mount.profileId));
        const QString id = mount.id;
        QAction* go = m_bookmarkMenu->addAction(QIcon::fromTheme(QStringLiteral("folder-remote")), text);
        go->setToolTip(mount.remotePath);
        connect(go, &QAction::triggered, this, [this, id]() { openBookmark(id); });
        QAction* remove = removeMenu->addAction(text);
        connect(remove, &QAction::triggered, this, [this, id]() {
            if (m_settings)
                m_settings->removeRemoteMount(id);
            updateControls();
        });
    }
    m_bookmarkMenu->addSeparator();
    m_bookmarkMenu->addMenu(removeMenu);
}

/** @brief Makes a bookmark the remote working directory; connects to its source first when needed.
 *  Claude Generated 2026 */
void RemoteBrowserPanel::openBookmark(const QString& bookmarkId)
{
    if (!m_settings)
        return;
    Settings::RemoteMountPoint mount;
    for (const auto& m : m_settings->remoteMounts())
        if (m.id == bookmarkId)
            mount = m;
    if (!mount.isValid())
        return;
    m_settings->updateRemoteMountLastAccessed(bookmarkId);
    if (m_state == State::Connected && mount.profileId == m_connection.sourceId) {
        setWorkingDirectory(mount.remotePath);
        return;
    }
    const RemoteConnection connection = connectionForSource(mount.profileId);
    if (!connection.isValid()) {
        setStatus(tr("The computer or SFTP profile of the bookmark \"%1\" no longer exists.").arg(mount.name), true);
        return;
    }
    {
        const QSignalBlocker blocker(m_sourceCombo);
        m_sourceCombo->setCurrentIndex(m_sourceCombo->findData(mount.profileId));
        m_lastSourceIndex = m_sourceCombo->currentIndex();
    }
    startConnection(connection, mount.remotePath);
}
