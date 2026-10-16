// remotebrowserpanel.cpp - Remote segment of the Project dock: browse SFTP directories like local ones
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "remotebrowserpanel.h"

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
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStyle>
#include <QThread>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

namespace {
const QString kOtherHostId = QStringLiteral("__other__");      // combo entry that opens the host dialog
const QString kUnsavedHostId = QStringLiteral("__unsaved__");  // combo entry of a host that was not saved

/// ~/.ssh/config entry whose alias equals @p host (invalid entry when there is none).
SshConfigEntry aliasFor(const QString& host)
{
    return SshConfigParser::configFileExists() ? SshConfigParser::findHost(host) : SshConfigEntry();
}
} // namespace

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

/** @brief Builds the source row, status line, navigation row and listing. Claude Generated 2026 */
void RemoteBrowserPanel::setupUi()
{
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

    auto* navRow = new QHBoxLayout;
    navRow->setSpacing(2);
    m_upButton = new QToolButton;
    m_upButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogToParent));
    m_upButton->setToolTip(tr("Parent folder"));
    m_upButton->setAutoRaise(true);
    m_refreshButton = new QToolButton;
    m_refreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    m_refreshButton->setToolTip(tr("Read the folder again"));
    m_refreshButton->setAutoRaise(true);
    m_breadcrumb = new BreadcrumbBar;
    m_bookmarkButton = new QToolButton;
    m_bookmarkButton->setIcon(QIcon::fromTheme(QStringLiteral("bookmark-new")));
    m_bookmarkButton->setText(QStringLiteral("★"));
    m_bookmarkButton->setCheckable(true);
    m_bookmarkButton->setAutoRaise(true);
    m_bookmarkMenuButton = new QToolButton;
    m_bookmarkMenuButton->setIcon(QIcon::fromTheme(QStringLiteral("bookmarks")));
    m_bookmarkMenuButton->setText(tr("Bookmarks"));
    m_bookmarkMenuButton->setToolTip(tr("Remote bookmarks"));
    m_bookmarkMenuButton->setPopupMode(QToolButton::InstantPopup);
    m_bookmarkMenuButton->setAutoRaise(true);
    m_bookmarkMenu = new QMenu(m_bookmarkMenuButton);
    m_bookmarkMenu->setToolTipsVisible(true);
    m_bookmarkMenuButton->setMenu(m_bookmarkMenu);
    navRow->addWidget(m_upButton);
    navRow->addWidget(m_refreshButton);
    navRow->addWidget(m_breadcrumb, 1);
    navRow->addWidget(m_bookmarkButton);
    navRow->addWidget(m_bookmarkMenuButton);
    layout->addLayout(navRow);

    m_listView = new QListView;
    m_model = new QStandardItemModel(this);
    m_listView->setModel(m_model);
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->setUniformItemSizes(true);
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_listView, 1);

    connect(m_sourceCombo, &QComboBox::activated, this, &RemoteBrowserPanel::onSourceActivated);
    connect(m_sourceCombo, &QComboBox::currentIndexChanged, this, [this]() { updateControls(); });
    connect(m_connectButton, &QPushButton::clicked, this, &RemoteBrowserPanel::onConnectButtonClicked);
    connect(m_upButton, &QToolButton::clicked, this, [this]() { navigateTo(remotepaths::parent(m_currentPath)); });
    connect(m_refreshButton, &QToolButton::clicked, this, [this]() { navigateTo(m_currentPath); });
    connect(m_breadcrumb, &BreadcrumbBar::pathSelected, this, &RemoteBrowserPanel::navigateTo);
    connect(m_bookmarkButton, &QToolButton::clicked, this, &RemoteBrowserPanel::toggleBookmark);
    connect(m_bookmarkMenu, &QMenu::aboutToShow, this, &RemoteBrowserPanel::rebuildBookmarkMenu);
    connect(m_listView, &QListView::doubleClicked, this, &RemoteBrowserPanel::onItemDoubleClicked);
    connect(m_listView, &QWidget::customContextMenuRequested, this, &RemoteBrowserPanel::showContextMenu);
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

/** @brief Enables buttons and the list for the current state. Claude Generated 2026 */
void RemoteBrowserPanel::updateControls()
{
    const bool connected = m_state == State::Connected;
    const bool isConnection = m_state != State::Disconnected && selectionIsConnection();
    const QString selected = selectedSourceId();
    m_connectButton->setText(isConnection ? tr("Disconnect") : tr("Connect"));
    m_connectButton->setEnabled(isConnection || (!selected.isEmpty() && selected != kOtherHostId));

    const bool hasPath = connected && !m_currentPath.isEmpty();
    m_upButton->setEnabled(hasPath && m_currentPath != QLatin1String("/"));
    m_refreshButton->setEnabled(hasPath);
    m_breadcrumb->setEnabled(connected);
    m_listView->setEnabled(connected);

    const bool canBookmark = hasPath && !m_connection.sourceId.isEmpty();
    const bool bookmarked = canBookmark && !bookmarkIdFor(m_currentPath).isEmpty();
    m_bookmarkButton->setEnabled(canBookmark);
    {
        const QSignalBlocker blocker(m_bookmarkButton);
        m_bookmarkButton->setChecked(bookmarked);
    }
    if (hasPath && m_connection.sourceId.isEmpty())
        m_bookmarkButton->setToolTip(tr("Save the host as an SFTP profile to bookmark its folders"));
    else
        m_bookmarkButton->setToolTip(bookmarked ? tr("Remove the bookmark of this folder") : tr("Bookmark this folder"));

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

/** @brief Starts a fresh SftpClient on its own thread and asks it to connect. Claude Generated 2026 */
void RemoteBrowserPanel::startConnection(const RemoteConnection& connection, const QString& startPath)
{
    abandonClient();
    m_state = State::Disconnected;
    m_home.clear();
    m_currentPath.clear();
    m_requestedPath.clear();
    m_deferredError.clear();
    m_entries.clear();
    m_model->clear();
    m_breadcrumb->setPath(QString());
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
    m_currentPath.clear();
    m_requestedPath.clear();
    m_entries.clear();
    m_model->clear();
    m_breadcrumb->setPath(QString());
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
    const QString start = m_startPath.isEmpty() ? homePath : m_startPath;
    m_startPath.clear();
    updateControls();
    navigateTo(start);
}

void RemoteBrowserPanel::onConnectFailed(const QString& message)
{
    if (!fromCurrentClient())
        return;
    abandonClient();
    m_state = State::Disconnected;
    setStatus(message, true);
    updateControls();
}

/** @brief Asks the client for a listing; the answer arrives in onListed/onListFailed. Claude Generated 2026 */
void RemoteBrowserPanel::navigateTo(const QString& path)
{
    if (m_state != State::Connected || !m_client || path.isEmpty())
        return;
    m_requestedPath = remotepaths::normalise(path);
    beginOperation();
    SftpClient* client = m_client;
    const QString requested = m_requestedPath;
    QMetaObject::invokeMethod(client, [client, requested]() { client->list(requested); }, Qt::QueuedConnection);
}

void RemoteBrowserPanel::onListed(const QString& path, const QList<RemoteEntry>& entries)
{
    if (!fromCurrentClient())
        return;
    endOperation();
    if (path != m_requestedPath)
        return;  // answer to an older request
    m_currentPath = path;
    m_entries = entries;
    m_model->clear();
    int folders = 0;
    const QLocale locale;
    for (int i = 0; i < m_entries.size(); ++i) {
        const RemoteEntry& entry = m_entries.at(i);
        auto* item = new QStandardItem(iconFor(entry), entry.name);
        item->setEditable(false);
        item->setData(i, Qt::UserRole);
        QString tip = remotepaths::join(path, entry.name);
        if (!entry.isDir)
            tip += QLatin1Char('\n') + locale.formattedDataSize(entry.size);
        if (entry.modified.isValid())
            tip += QLatin1Char('\n') + locale.toString(entry.modified, QLocale::ShortFormat);
        if (entry.isLink)
            tip += QLatin1Char('\n') + tr("symbolic link");
        item->setToolTip(tip);
        m_model->appendRow(item);
        if (entry.isDir)
            ++folders;
    }
    m_breadcrumb->setPath(path);
    if (!m_deferredError.isEmpty()) {
        setStatus(m_deferredError, true);
        m_deferredError.clear();
    } else {
        setStatus(tr("%1: %2 (%3 folders, %4 files)")
                      .arg(m_connection.account(), remotepaths::displayPath(path, m_home))
                      .arg(folders)
                      .arg(m_entries.size() - folders));
    }
    updateControls();
}

void RemoteBrowserPanel::onListFailed(const QString& path, const QString& message)
{
    if (!fromCurrentClient())
        return;
    endOperation();
    if (path != m_requestedPath)
        return;
    if (m_currentPath.isEmpty() && !m_home.isEmpty() && path != m_home) {
        // the start folder (a bookmark) is gone or unreadable: show the home folder and the reason
        m_deferredError = message;
        navigateTo(m_home);
        return;
    }
    m_requestedPath = m_currentPath;
    setStatus(message, true);
    updateControls();
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

const RemoteEntry* RemoteBrowserPanel::entryAt(const QModelIndex& index) const
{
    if (!index.isValid())
        return nullptr;
    const int i = index.data(Qt::UserRole).toInt();
    return i >= 0 && i < m_entries.size() ? &m_entries.at(i) : nullptr;
}

void RemoteBrowserPanel::onItemDoubleClicked(const QModelIndex& index)
{
    const RemoteEntry* entry = entryAt(index);
    if (!entry)
        return;
    const QString path = remotepaths::join(m_currentPath, entry->name);
    if (entry->isDir)
        navigateTo(path);
    else
        openRemoteFile(*entry, path);
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
        setStatus(tr("Choose a working directory first (Files segment)."), true);
        return;
    }
    const QString target = QDir(dir).filePath(entry.name);
    if (QFileInfo::exists(target)
        && QMessageBox::question(this, tr("Replace File"),
                                 tr("%1 already exists in the working directory. Replace it?").arg(entry.name))
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

/** @brief Open, Download to Working Directory, Copy Remote Path, Add to Bookmarks. Claude Generated 2026 */
void RemoteBrowserPanel::showContextMenu(const QPoint& pos)
{
    if (m_state != State::Connected || m_currentPath.isEmpty())
        return;
    const RemoteEntry* found = entryAt(m_listView->indexAt(pos));
    const bool canBookmark = !m_connection.sourceId.isEmpty();
    QMenu menu(this);
    QString folder = m_currentPath;
    QString copyPath = m_currentPath;
    if (found) {
        const RemoteEntry entry = *found;  // copy: a listing may arrive while the menu is open
        const QString path = remotepaths::join(m_currentPath, entry.name);
        copyPath = path;
        QAction* open = menu.addAction(tr("Open"));
        connect(open, &QAction::triggered, this, [this, entry, path]() {
            if (entry.isDir)
                navigateTo(path);
            else
                openRemoteFile(entry, path);
        });
        if (!entry.isDir) {
            QAction* save = menu.addAction(tr("Download to Working Directory"));
            connect(save, &QAction::triggered, this, [this, entry, path]() { downloadToWorkingDirectory(entry, path); });
        } else {
            folder = path;
        }
        menu.addSeparator();
    }
    QAction* copy = menu.addAction(tr("Copy Remote Path"));
    connect(copy, &QAction::triggered, this, [copyPath]() { QGuiApplication::clipboard()->setText(copyPath); });
    QAction* bookmark = menu.addAction(found && !found->isDir ? tr("Add This Folder to Bookmarks") : tr("Add to Bookmarks"));
    bookmark->setEnabled(canBookmark && bookmarkIdFor(folder).isEmpty());
    connect(bookmark, &QAction::triggered, this, [this, folder]() { addBookmark(folder); });
    if (!found) {
        menu.addSeparator();
        QAction* refresh = menu.addAction(tr("Refresh"));
        connect(refresh, &QAction::triggered, this, [this]() { navigateTo(m_currentPath); });
    }
    menu.exec(m_listView->viewport()->mapToGlobal(pos));
}

QIcon RemoteBrowserPanel::iconFor(const RemoteEntry& entry)
{
    const QString key = entry.isDir ? QStringLiteral("/") : QFileInfo(entry.name).suffix().toLower();
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

void RemoteBrowserPanel::toggleBookmark()
{
    const QString id = bookmarkIdFor(m_currentPath);
    if (!id.isEmpty() && m_settings) {
        m_settings->removeRemoteMount(id);
        setStatus(tr("Bookmark removed: %1").arg(remotepaths::displayPath(m_currentPath, m_home)));
        updateControls();
    } else {
        addBookmark(m_currentPath);
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

/** @brief Goes to a bookmark; connects to its computer or profile first when needed. Claude Generated 2026 */
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
        navigateTo(mount.remotePath);
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
