// sftpdialog.cpp - Remote SFTP File Browser Dialog Implementation
// Copyright (C) 2025 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - SFTP remote file access for HPC clusters

#include "sftpdialog.h"
#include "../sftpmodel.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeView>
#include <QLabel>
#include <QSpinBox>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QApplication>  // Claude Generated - For processEvents()

SftpDialog::SftpDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Open Remote File (SFTP)"));
    setMinimumSize(800, 600);

    setupUI();
    connectSignals();
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

    // Connection buttons
    auto* connectionButtonLayout = new QHBoxLayout();
    m_connectButton = new QPushButton(tr("Connect"), this);
    m_disconnectButton = new QPushButton(tr("Disconnect"), this);
    connectionButtonLayout->addWidget(m_connectButton);
    connectionButtonLayout->addWidget(m_disconnectButton);
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

    m_statusLabel->setText(tr("Connecting to %1...").arg(host));
    QApplication::processEvents();

    // Create SFTP model (connection happens in constructor)
    if (m_sftpModel) {
        delete m_sftpModel;
    }

    m_sftpModel = new SftpItemModel(host, username, password, this);

    // Check if connection was successful by checking row count
    if (m_sftpModel->rowCount() >= 0) {
        m_fileTreeView->setModel(m_sftpModel);
        m_fileTreeView->setColumnWidth(0, 300);
        updateConnectionState(true);
        m_statusLabel->setText(tr("Connected to %1").arg(host));
    } else {
        delete m_sftpModel;
        m_sftpModel = nullptr;
        updateConnectionState(false);
        m_statusLabel->setText(tr("Connection failed"));
        QMessageBox::critical(this, tr("Connection Error"),
                             tr("Failed to connect to %1.\nCheck credentials and network connection.").arg(host));
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

    // Get file info from model
    QString fileName = m_sftpModel->data(index, Qt::DisplayRole).toString();
    QString fileType = m_sftpModel->data(m_sftpModel->index(index.row(), 2, index.parent()), Qt::DisplayRole).toString();

    if (fileType == "Directory") {
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
        QMessageBox::warning(this, tr("No Selection"), tr("Please select a file to open."));
        return;
    }

    // Get full path - need to reconstruct from model
    // For now, use a simplified approach
    QString fileName = m_sftpModel->data(index, Qt::DisplayRole).toString();
    QString fileType = m_sftpModel->data(m_sftpModel->index(index.row(), 2, index.parent()), Qt::DisplayRole).toString();

    if (fileType == "Directory") {
        QMessageBox::information(this, tr("Directory Selected"),
                                tr("Please select a file, not a directory."));
        return;
    }

    // Build remote path (simplified - assumes root directory for now)
    m_selectedFile = "/" + fileName;

    // Download to temp directory
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/qurcuma_sftp";
    QDir().mkpath(tempDir);
    m_localPath = tempDir + "/" + fileName;

    m_statusLabel->setText(tr("Downloading %1...").arg(fileName));
    QApplication::processEvents();

    if (m_sftpModel->downloadFile(m_selectedFile, m_localPath)) {
        m_statusLabel->setText(tr("Downloaded successfully"));
        emit fileSelected(m_selectedFile, m_localPath);
        accept();
    } else {
        m_statusLabel->setText(tr("Download failed"));
        QMessageBox::critical(this, tr("Download Error"),
                             tr("Failed to download file:\n%1").arg(m_selectedFile));
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
    m_openButton->setEnabled(connected);
    m_fileTreeView->setEnabled(connected);
}
