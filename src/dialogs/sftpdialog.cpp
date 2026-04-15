// sftpdialog.cpp - Remote SFTP File Browser Dialog Implementation
// Copyright (C) 2025 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - SFTP remote file access for HPC clusters

#include "sftpdialog.h"
#include "../sftpmodel.hpp"
#include "../settings.h"
#include "../sshconfig.h"
#include "../sftpcache.h"  // Claude Generated - Cache integration

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeView>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>  // Claude Generated - For dropdowns
#include <QCheckBox>  // Claude Generated - For SSH key checkbox
#include <QInputDialog>  // Claude Generated - For profile name input
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QProgressDialog>  // Claude Generated - For connection/download progress feedback
#include <QUuid>  // Claude Generated - For profile IDs

SftpDialog::SftpDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Open Remote File (SFTP)"));
    setMinimumSize(800, 600);

    setupUI();
    connectSignals();
    loadProfiles();  // Claude Generated - Load saved profiles
    loadSSHConfigHosts();  // Claude Generated - Load SSH config hosts
    updateConnectionState(false);
}

SftpDialog::~SftpDialog()
{
    if (m_sftpModel) {
        delete m_sftpModel;
    }
}

void SftpDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Connection settings group
    auto* connectionGroup = new QGroupBox(tr("Connection Settings"), this);
    auto* connectionLayout = new QFormLayout(connectionGroup);

    // Saved profiles dropdown (Claude Generated - Phase SFTP Integration)
    m_profileCombo = new QComboBox(this);
    m_profileCombo->setEditable(false);
    connectionLayout->addRow(tr("Saved Profiles:"), m_profileCombo);

    // SSH Config hosts dropdown (Claude Generated - Phase SFTP Integration)
    m_sshConfigCombo = new QComboBox(this);
    m_sshConfigCombo->setEditable(false);
    connectionLayout->addRow(tr("SSH Config Hosts:"), m_sshConfigCombo);

    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setPlaceholderText("e.g., hpc.cluster.edu");
    connectionLayout->addRow(tr("Host:"), m_hostEdit);

    m_usernameEdit = new QLineEdit(this);
    connectionLayout->addRow(tr("Username:"), m_usernameEdit);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    connectionLayout->addRow(tr("Password:"), m_passwordEdit);

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(22);
    connectionLayout->addRow(tr("Port:"), m_portSpin);

    // SSH Key authentication checkbox (Claude Generated - Phase SFTP Integration)
    m_useSSHKeyCheckbox = new QCheckBox(tr("Use SSH Key (auto-detect)"), this);
    m_useSSHKeyCheckbox->setToolTip(tr("Try SSH key authentication (looks for ~/.ssh/id_rsa, id_ed25519, etc.)"));
    connectionLayout->addRow("", m_useSSHKeyCheckbox);

    // Connection buttons
    auto* connectionButtonLayout = new QHBoxLayout();
    m_connectButton = new QPushButton(tr("Connect"), this);
    m_disconnectButton = new QPushButton(tr("Disconnect"), this);
    m_saveProfileButton = new QPushButton(tr("Save Profile"), this);  // Claude Generated
    connectionButtonLayout->addWidget(m_connectButton);
    connectionButtonLayout->addWidget(m_disconnectButton);
    connectionButtonLayout->addWidget(m_saveProfileButton);
    connectionButtonLayout->addStretch();
    connectionLayout->addRow("", connectionButtonLayout);

    mainLayout->addWidget(connectionGroup);

    // Status label
    m_statusLabel = new QLabel(tr("Not connected"), this);
    mainLayout->addWidget(m_statusLabel);

    // File tree view
    auto* fileGroup = new QGroupBox(tr("Remote Files"), this);
    auto* fileLayout = new QVBoxLayout(fileGroup);

    m_fileTreeView = new QTreeView(this);
    m_fileTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fileTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fileTreeView->setAlternatingRowColors(true);
    fileLayout->addWidget(m_fileTreeView);

    mainLayout->addWidget(fileGroup);

    // Dialog buttons
    auto* buttonLayout = new QHBoxLayout();
    m_openButton = new QPushButton(tr("Open Selected File"), this);
    m_openButton->setDefault(true);
    m_cancelButton = new QPushButton(tr("Cancel"), this);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_openButton);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);
}

void SftpDialog::connectSignals()
{
    connect(m_connectButton, &QPushButton::clicked, this, &SftpDialog::onConnectClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &SftpDialog::onDisconnectClicked);
    connect(m_fileTreeView, &QTreeView::doubleClicked, this, &SftpDialog::onFileDoubleClicked);
    connect(m_openButton, &QPushButton::clicked, this, &SftpDialog::onSelectionChanged);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    // Claude Generated - Phase SFTP Integration: New signal connections
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SftpDialog::onProfileSelected);
    connect(m_sshConfigCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SftpDialog::onSSHConfigHostSelected);
    connect(m_saveProfileButton, &QPushButton::clicked, this, &SftpDialog::onSaveProfileClicked);
}

void SftpDialog::onConnectClicked()
{
    QString host = m_hostEdit->text().trimmed();
    QString username = m_usernameEdit->text().trimmed();
    QString password = m_passwordEdit->text();

    if (host.isEmpty() || username.isEmpty()) {
        QMessageBox::warning(this, tr("Connection Error"),
                            tr("Please provide host and username."));
        return;
    }

    // Show progress dialog during connection (Claude Generated - Phase SFTP Integration)
    QProgressDialog* progressDialog = new QProgressDialog(
        tr("Connecting to %1...\nAuthenticating and loading directory...").arg(host),
        tr("Cancel"), 0, 0, this);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->setMinimumDuration(0);  // Show immediately
    progressDialog->setValue(0);
    progressDialog->show();

    m_statusLabel->setText(tr("Connecting to %1...").arg(host));

    // Log connection attempt (Claude Generated - Improved Logging)
    int port = m_portSpin->value();
    bool useKeyAuth = m_useSSHKeyCheckbox->isChecked();
    qDebug() << "[DIALOG] [SFTP] ============================================";
    qDebug() << "[DIALOG] [SFTP] Connection Attempt:";
    qDebug() << "[DIALOG] [SFTP]   Host:" << host;
    qDebug() << "[DIALOG] [SFTP]   User:" << username;
    qDebug() << "[DIALOG] [SFTP]   Port:" << port;
    qDebug() << "[DIALOG] [SFTP]   SSH Key Auth:" << (useKeyAuth ? "enabled" : "disabled");
    qDebug() << "[DIALOG] [SFTP] ============================================";

    // Create SFTP model (connection happens in constructor)
    if (m_sftpModel) {
        delete m_sftpModel;
    }

    // Create SFTP model with port parameter (Claude Generated - Phase SFTP Integration)
    m_sftpModel = new SftpItemModel(host, username, password, port, this);

    // Enable SSH key authentication if checkbox is checked (Claude Generated)
    if (useKeyAuth) {
        qDebug() << "[DIALOG] [SFTP] Enabling SSH key authentication";
        m_sftpModel->setUseKeyAuth(true);
    }

    // Set ProxyCommand if SSH config host was selected (Claude Generated - ProxyJump Support)
    if (m_sshConfigCombo->currentIndex() > 0) {
        QString hostAlias = m_sshConfigCombo->currentData().toString();
        if (!hostAlias.isEmpty()) {
            SshConfigEntry entry = SshConfigParser::findHost(hostAlias);
            if (!entry.proxyCommand.isEmpty()) {
                qDebug() << "[DIALOG] [SFTP] Using ProxyCommand from SSH config:" << entry.proxyCommand;
                m_sftpModel->setProxyCommand(entry.proxyCommand);
            }
        }
    }

    // Check if connection was successful (Claude Generated - SFTP Bug Fix)
    // NOTE: rowCount() >= 0 is ALWAYS true, even on failed connections!
    // Must use isConnected() instead.
    if (m_sftpModel->isConnected()) {
        progressDialog->deleteLater();
        int itemCount = m_sftpModel->rowCount();
        qDebug() << "[DIALOG] [SFTP] ✓ Connection successful!";
        qDebug() << "[DIALOG] [SFTP] Remote directory contains" << itemCount << "items";

        m_fileTreeView->setModel(m_sftpModel);
        m_fileTreeView->setColumnWidth(0, 300);
        updateConnectionState(true);

        // Enhanced feedback: Show file count or empty directory (Claude Generated - SFTP Bug Fix)
        if (itemCount == 0) {
            m_statusLabel->setText(tr("Connected to %1 (directory is empty)").arg(host));
        } else {
            m_statusLabel->setText(tr("Connected to %1 (%2 items)").arg(host).arg(itemCount));
        }

        // Update lastUsed timestamp for profile if used (Claude Generated)
        if (m_profileCombo->currentIndex() > 0) {
            QString profileId = m_profileCombo->currentData().toString();
            if (!profileId.isEmpty()) {
                Settings settings;
                settings.updateSftpProfileLastUsed(profileId);
            }
        }
    } else {
        // Connection failed (Claude Generated - SFTP Bug Fix)
        progressDialog->deleteLater();  // Delete here too!

        // Get detailed error from SSH (Claude Generated - Phase SFTP Integration)
        QString errorDetails = m_sftpModel->getLastError();
        delete m_sftpModel;
        m_sftpModel = nullptr;
        updateConnectionState(false);
        m_statusLabel->setText(tr("Connection failed"));

        // Enhanced error dialog with debugging tips (Claude Generated - Improved Logging)
        qCritical() << "[DIALOG] [SFTP] ✗ Connection failed!";
        qWarning() << "[DIALOG] [SFTP] Error details:" << errorDetails;
        qWarning() << "[DIALOG] [SFTP] Check console output for detailed diagnostic logs";

        QMessageBox::critical(this, tr("Connection Error"),
                             tr("Failed to connect to %1@%2:%3\n\n"
                                "Error details:\n%4\n\n"
                                "Debugging tips:\n"
                                "  • Check console output for detailed logs (see Application Output)\n"
                                "  • Verify host name/IP is correct\n"
                                "  • Verify credentials (user/password or SSH key)\n"
                                "  • Check firewall and network connectivity\n"
                                "  • Ensure SFTP is enabled on the server\n"
                                "  • For ProxyJump: verify gateway connectivity first")
                             .arg(username, host, QString::number(port), errorDetails));
    }
}

void SftpDialog::onDisconnectClicked()
{
    if (m_sftpModel) {
        delete m_sftpModel;
        m_sftpModel = nullptr;
    }

    m_fileTreeView->setModel(nullptr);
    updateConnectionState(false);
    m_statusLabel->setText(tr("Disconnected"));
}

void SftpDialog::onFileDoubleClicked(const QModelIndex& index)
{
    if (!m_sftpModel || !index.isValid()) {
        return;
    }

    // Check if it's a directory (Claude Generated - Phase SFTP Integration)
    if (m_sftpModel->isDirectory(index)) {
        // Expand/collapse directory
        if (m_fileTreeView->isExpanded(index)) {
            m_fileTreeView->collapse(index);
        } else {
            m_fileTreeView->expand(index);
        }
        return;
    }

    // It's a file - download and open
    onSelectionChanged();
}

void SftpDialog::onSelectionChanged()
{
    if (!m_sftpModel) {
        return;
    }

    QModelIndex index = m_fileTreeView->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, tr("No Selection"), tr("Please select a %1.")
                            .arg(m_mode == Mode::SelectDirectory ? "directory" : "file to open"));
        return;
    }

    // Claude Generated - Remote Directory Mounting: Handle SelectDirectory mode
    if (m_mode == Mode::SelectDirectory) {
        // In SelectDirectory mode, we want directories, not files
        if (!m_sftpModel->isDirectory(index)) {
            QMessageBox::information(this, tr("File Selected"),
                                    tr("Please select a directory, not a file."));
            return;
        }

        // Get directory path and profile ID
        m_selectedDirectory = m_sftpModel->getItemPath(index);

        // Get current profile ID
        if (m_profileCombo->currentIndex() > 0) {
            m_selectedProfileId = m_profileCombo->currentData().toString();
        }

        if (m_selectedDirectory.isEmpty() || m_selectedProfileId.isEmpty()) {
            QMessageBox::warning(this, tr("Invalid Selection"),
                                tr("Could not determine directory path or connection profile."));
            return;
        }

        // Accept dialog with selected directory
        accept();
        return;
    }

    // Original file download mode (Claude Generated - Phase SFTP Integration)
    // Fixes bug: Previously only used filename, now gets complete path from model
    if (m_sftpModel->isDirectory(index)) {
        QMessageBox::information(this, tr("Directory Selected"),
                                tr("Please select a file, not a directory."));
        return;
    }

    m_selectedFile = m_sftpModel->getItemPath(index);
    QString fileName = m_sftpModel->getItemName(index);

    if (m_selectedFile.isEmpty() || fileName.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Selection"), tr("Invalid file selection."));
        return;
    }

    // Check cache first (Claude Generated - Phase SFTP Integration)
    SftpCache cache;
    QString host = m_hostEdit->text().trimmed();

    if (cache.isCached(host, m_selectedFile)) {
        m_localPath = cache.getCachedPath(host, m_selectedFile);
        m_statusLabel->setText(tr("Using cached file"));
        qDebug() << "Using cached file:" << m_localPath;
        emit fileSelected(m_selectedFile, m_localPath);
        accept();
        return;
    }

    // Prepare cache path for download
    m_localPath = cache.prepareCachePath(host, m_selectedFile, true);

    // Show progress during download (Claude Generated - Phase SFTP Integration)
    QProgressDialog* downloadProgress = new QProgressDialog(
        tr("Downloading %1...\nPlease wait...").arg(fileName),
        tr("Cancel"), 0, 0, this);
    downloadProgress->setWindowModality(Qt::WindowModal);
    downloadProgress->setMinimumDuration(0);  // Show immediately
    downloadProgress->setValue(0);
    downloadProgress->show();

    m_statusLabel->setText(tr("Downloading %1...").arg(fileName));

    bool downloadSuccess = m_sftpModel->downloadFile(m_selectedFile, m_localPath);
    downloadProgress->deleteLater();

    // Mark as cached if successful (Claude Generated)
    if (downloadSuccess) {
        cache.markCached(host, m_selectedFile, m_localPath);
    }

    if (downloadSuccess) {
        m_statusLabel->setText(tr("Downloaded successfully"));
        emit fileSelected(m_selectedFile, m_localPath);
        accept();
    } else {
        // Show detailed error information (Claude Generated - Phase SFTP Integration)
        QString errorDetails = m_sftpModel->getLastError();
        m_statusLabel->setText(tr("Download failed"));
        QMessageBox::critical(this, tr("Download Error"),
                             tr("Failed to download file:\n%1\n\nError details:\n%2").arg(m_selectedFile, errorDetails));
    }
}

void SftpDialog::updateConnectionState(bool connected)
{
    m_isConnected = connected;
    m_connectButton->setEnabled(!connected);
    m_disconnectButton->setEnabled(connected);
    m_hostEdit->setEnabled(!connected);
    m_usernameEdit->setEnabled(!connected);
    m_passwordEdit->setEnabled(!connected);
    m_portSpin->setEnabled(!connected);
    m_useSSHKeyCheckbox->setEnabled(!connected);  // Claude Generated
    m_profileCombo->setEnabled(!connected);  // Claude Generated
    m_sshConfigCombo->setEnabled(!connected);  // Claude Generated
    m_openButton->setEnabled(connected);
    m_fileTreeView->setEnabled(connected);
}

// Claude Generated - Phase SFTP Integration: Profile and SSH config management
void SftpDialog::loadProfiles()
{
    m_profileCombo->clear();
    m_profileCombo->addItem(tr("-- Select Profile --"), "");

    Settings settings;
    QVector<Settings::SftpConnectionProfile> profiles = settings.sftpProfiles();

    for (const auto& profile : profiles) {
        m_profileCombo->addItem(profile.name, profile.id);
    }
}

void SftpDialog::loadSSHConfigHosts()
{
    m_sshConfigCombo->clear();
    m_sshConfigCombo->addItem(tr("-- Select SSH Config Host --"), "");

    if (!SshConfigParser::configFileExists()) {
        m_sshConfigCombo->setEnabled(false);
        m_sshConfigCombo->setToolTip(tr("No ~/.ssh/config file found"));
        return;
    }

    QList<SshConfigEntry> entries = SshConfigParser::parseFile();
    for (const auto& entry : entries) {
        m_sshConfigCombo->addItem(entry.host, entry.host);
    }
}

void SftpDialog::onProfileSelected(int index)
{
    if (index <= 0) {
        // "-- Select Profile --" was selected, do nothing
        return;
    }

    QString profileId = m_profileCombo->itemData(index).toString();
    if (!profileId.isEmpty()) {
        fillFormFromProfile(profileId);
    }
}

void SftpDialog::onSSHConfigHostSelected(int index)
{
    if (index <= 0) {
        return;
    }

    QString hostAlias = m_sshConfigCombo->itemData(index).toString();
    if (hostAlias.isEmpty()) {
        return;
    }

    SshConfigEntry entry = SshConfigParser::findHost(hostAlias);
    if (entry.isValid()) {
        m_hostEdit->setText(entry.hostName);
        if (!entry.user.isEmpty()) {
            m_usernameEdit->setText(entry.user);
        }
        if (entry.port != 22) {
            m_portSpin->setValue(entry.port);
        }
        if (!entry.identityFile.isEmpty()) {
            m_useSSHKeyCheckbox->setChecked(true);
        }
        // Claude Generated - ProxyJump Support: Log if ProxyCommand will be used
        if (!entry.proxyCommand.isEmpty()) {
            qDebug() << "SSH Config entry has ProxyCommand:" << entry.proxyCommand;
            m_statusLabel->setText(tr("SSH Config loaded (with proxy jump)"));
        }
    }
}

void SftpDialog::fillFormFromProfile(const QString& profileId)
{
    Settings settings;
    QVector<Settings::SftpConnectionProfile> profiles = settings.sftpProfiles();

    for (const auto& profile : profiles) {
        if (profile.id == profileId) {
            m_hostEdit->setText(profile.host);
            m_usernameEdit->setText(profile.username);
            m_portSpin->setValue(profile.port);
            m_useSSHKeyCheckbox->setChecked(profile.useKeyAuth);
            break;
        }
    }
}

void SftpDialog::onSaveProfileClicked()
{
    QString host = m_hostEdit->text().trimmed();
    QString username = m_usernameEdit->text().trimmed();

    if (host.isEmpty() || username.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Profile"),
                            tr("Please provide at least a host and username to save a profile."));
        return;
    }

    // Ask user for profile name
    bool ok;
    QString profileName = QInputDialog::getText(this, tr("Save Connection Profile"),
                                               tr("Profile Name:"), QLineEdit::Normal,
                                               username + "@" + host, &ok);

    if (!ok || profileName.isEmpty()) {
        return;
    }

    Settings settings;
    Settings::SftpConnectionProfile profile;
    profile.id = QUuid::createUuid().toString();
    profile.name = profileName;
    profile.host = host;
    profile.username = username;
    profile.port = m_portSpin->value();
    profile.useKeyAuth = m_useSSHKeyCheckbox->isChecked();
    profile.created = QDateTime::currentDateTime();
    profile.lastUsed = QDateTime::currentDateTime();

    settings.addSftpProfile(profile);

    QMessageBox::information(this, tr("Profile Saved"),
                            tr("Connection profile \"%1\" has been saved.").arg(profileName));

    // Reload profile dropdown
    loadProfiles();

    // Select the newly created profile
    int newIndex = m_profileCombo->findData(profile.id);
    if (newIndex >= 0) {
        m_profileCombo->setCurrentIndex(newIndex);
    }
}

// Claude Generated - Remote Directory Mounting
void SftpDialog::setMode(Mode mode)
{
    m_mode = mode;

    if (m_mode == Mode::SelectDirectory) {
        setWindowTitle(tr("Select Remote Directory"));
        m_openButton->setText(tr("Select Directory"));
        m_statusLabel->setText(tr("Select a remote directory to add to workspace"));
    } else {
        setWindowTitle(tr("Open Remote File (SFTP)"));
        m_openButton->setText(tr("Open Selected File"));
        m_statusLabel->setText(tr("Not connected"));
    }
}
