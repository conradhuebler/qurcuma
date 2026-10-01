// remotefilesdialog.cpp - Browse and fetch files on a computer running qurcuma-server
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute R4)

#include "remotefilesdialog.h"

#include "sshconfig.h"

#include <QCryptographicHash>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>

namespace {
const QStringList kStructureExtensions = { "xyz", "vtf", "pdb", "mol2", "sdf", "mol" };

QString sizeText(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes);
}
}

RemoteFilesDialog::RemoteFilesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Remote Files"));
    resize(640, 480);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    m_hostCombo = new QComboBox(this);
    for (const SshConfigEntry& e : SshConfigParser::parseFile()) {
        if (e.host.contains(QLatin1Char('*')) || e.host.contains(QLatin1Char('?')) || e.host.contains(QLatin1Char('!')))
            continue;
        m_hostCombo->addItem(e.host, e.host);
    }
    const QString lastHost = QSettings().value(QStringLiteral("remote/host")).toString();
    if (m_hostCombo->findData(lastHost) >= 0)
        m_hostCombo->setCurrentIndex(m_hostCombo->findData(lastHost));
    m_serverCommand = new QLineEdit(this);
    m_serverCommand->setToolTip(tr("Command that starts qurcuma-server on the remote computer. Add --browse <directory> "
                                   "to share more than the session directories."));
    auto loadCommand = [this] {
        m_serverCommand->setText(QSettings().value(QStringLiteral("remote/serverCommand/") + m_hostCombo->currentData().toString(),
            QStringLiteral("qurcuma-server")).toString());
    };
    loadCommand();
    connect(m_hostCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, loadCommand);
    form->addRow(tr("Host:"), m_hostCombo);
    form->addRow(tr("Server command:"), m_serverCommand);
    layout->addLayout(form);

    m_connectButton = new QPushButton(tr("Connect"), this);
    connect(m_connectButton, &QPushButton::clicked, this, &RemoteFilesDialog::onConnectClicked);
    layout->addWidget(m_connectButton);

    auto* pathRow = new QHBoxLayout;
    m_upButton = new QPushButton(tr("Up"), this);
    m_pathEdit = new QLineEdit(this);
    pathRow->addWidget(m_upButton);
    pathRow->addWidget(m_pathEdit, 1);
    layout->addLayout(pathRow);
    connect(m_upButton, &QPushButton::clicked, this, &RemoteFilesDialog::onUpClicked);
    connect(m_pathEdit, &QLineEdit::returnPressed, this, [this] { navigate(m_pathEdit->text().trimmed()); });

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(3);
    m_tree->setHeaderLabels({ tr("Name"), tr("Size"), tr("Modified") });
    m_tree->setRootIsDecorated(false);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    layout->addWidget(m_tree, 1);
    connect(m_tree, &QTreeWidget::itemActivated, this, &RemoteFilesDialog::onItemActivated);

    auto* buttons = new QHBoxLayout;
    m_downloadButton = new QPushButton(tr("Download..."), this);
    m_cancelButton = new QPushButton(tr("Cancel transfer"), this);
    buttons->addWidget(m_downloadButton);
    buttons->addWidget(m_cancelButton);
    buttons->addStretch(1);
    layout->addLayout(buttons);
    connect(m_downloadButton, &QPushButton::clicked, this, &RemoteFilesDialog::onDownloadClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, [this] { if (m_files) m_files->cancelDownload(); });

    m_progress = new QProgressBar(this);
    m_progress->setVisible(false);
    layout->addWidget(m_progress);
    m_status = new QLabel(tr("Not connected. Needs ssh access by key or agent and qurcuma-server on the host."), this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    setConnected(false);
}

RemoteFilesDialog::~RemoteFilesDialog()
{
    disconnectFromHost();
}

void RemoteFilesDialog::setConnected(bool on)
{
    m_connectButton->setText(on ? tr("Disconnect") : tr("Connect"));
    m_hostCombo->setEnabled(!on);
    m_serverCommand->setEnabled(!on);
    m_pathEdit->setEnabled(on);
    m_upButton->setEnabled(on);
    m_tree->setEnabled(on);
    m_downloadButton->setEnabled(on);
    m_cancelButton->setEnabled(false);
}

void RemoteFilesDialog::disconnectFromHost()
{
    delete m_files;
    m_files = nullptr;
    m_roots.clear();
    m_currentPath.clear();
}

void RemoteFilesDialog::onConnectClicked()
{
    if (m_files) {
        disconnectFromHost();
        m_tree->clear();
        setConnected(false);
        m_status->setText(tr("Disconnected."));
        return;
    }
    const QString host = m_hostCombo->currentData().toString();
    if (host.isEmpty()) {
        m_status->setText(tr("No host: add one to ~/.ssh/config."));
        return;
    }
    QSettings settings;
    settings.setValue(QStringLiteral("remote/host"), host);
    settings.setValue(QStringLiteral("remote/serverCommand/") + host, m_serverCommand->text().trimmed());

    m_files = new remote::RemoteFiles(host, m_serverCommand->text().trimmed(), this);
    connect(m_files, &remote::RemoteFiles::connected, this, [this](const QStringList& roots) {
        m_roots = roots;
        setConnected(true);
        m_status->setText(tr("Connected to %1.").arg(m_hostCombo->currentText()));
    });
    connect(m_files, &remote::RemoteFiles::listing, this, &RemoteFilesDialog::onListing);
    connect(m_files, &remote::RemoteFiles::listingFailed, this, [this](const QString&, const QString& msg) {
        m_status->setText(tr("Cannot list: %1").arg(msg));
    });
    connect(m_files, &remote::RemoteFiles::downloadProgress, this, [this](quint32, qint64 got, qint64 total) {
        m_progress->setVisible(true);
        m_cancelButton->setEnabled(true);
        m_progress->setRange(0, total > 0 ? int(qMin<qint64>(total / 1024, INT_MAX)) : 0);
        m_progress->setValue(int(qMin<qint64>(got / 1024, INT_MAX)));
    });
    connect(m_files, &remote::RemoteFiles::downloadFinished, this, [this](quint32, const QString& local) {
        m_progress->setVisible(false);
        m_cancelButton->setEnabled(false);
        m_status->setText(tr("Saved %1").arg(local));
        if (m_openAfter)
            emit openFileRequested(local);
        m_openAfter = false;
    });
    connect(m_files, &remote::RemoteFiles::downloadFailed, this, [this](quint32, const QString& msg) {
        m_progress->setVisible(false);
        m_cancelButton->setEnabled(false);
        m_openAfter = false;
        m_status->setText(tr("Download failed: %1").arg(msg));
    });
    connect(m_files, &remote::RemoteFiles::failed, this, [this](const QString& msg) {
        m_status->setText(msg);
        QMetaObject::invokeMethod(this, [this] {
            disconnectFromHost();
            m_tree->clear();
            setConnected(false);
        }, Qt::QueuedConnection);
    });
    m_status->setText(tr("Connecting to %1 ...").arg(host));
    m_connectButton->setEnabled(true);
    m_files->connectToHost();
}

void RemoteFilesDialog::onListing(const QString& path, const QVector<remote::RemoteFiles::Entry>& entries, bool truncated)
{
    m_currentPath = path;
    m_pathEdit->setText(path);
    m_tree->clear();
    for (const remote::RemoteFiles::Entry& e : entries) {
        auto* item = new QTreeWidgetItem(m_tree);
        item->setText(0, e.isDir ? e.name + QLatin1Char('/') : e.name);
        item->setText(1, e.isDir ? QString() : sizeText(e.size));
        item->setText(2, QDateTime::fromSecsSinceEpoch(e.mtime).toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        item->setData(0, Qt::UserRole, e.name);
        item->setData(0, Qt::UserRole + 1, e.isDir);
        item->setData(1, Qt::UserRole, e.size);
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    }
    if (truncated)
        m_status->setText(tr("Only the first entries of a large directory are shown."));
}

void RemoteFilesDialog::navigate(const QString& path)
{
    if (m_files)
        m_files->list(path);
}

void RemoteFilesDialog::onUpClicked()
{
    if (m_currentPath.isEmpty() || m_roots.contains(m_currentPath)) {
        m_status->setText(tr("This is the top of the shared directories."));
        return;
    }
    navigate(QFileInfo(m_currentPath).absolutePath());
}

QString RemoteFilesDialog::selectedRemotePath() const
{
    const QTreeWidgetItem* item = m_tree->currentItem();
    if (!item || item->data(0, Qt::UserRole + 1).toBool())
        return QString();
    return m_currentPath + QLatin1Char('/') + item->data(0, Qt::UserRole).toString();
}

void RemoteFilesDialog::onItemActivated(QTreeWidgetItem* item)
{
    const QString name = item->data(0, Qt::UserRole).toString();
    if (item->data(0, Qt::UserRole + 1).toBool()) {
        navigate(m_currentPath + QLatin1Char('/') + name);
        return;
    }
    const QString remotePath = m_currentPath + QLatin1Char('/') + name;
    if (kStructureExtensions.contains(QFileInfo(name).suffix().toLower()))
        fetch(remotePath, cachePathFor(remotePath), true);
    else
        onDownloadClicked();
}

void RemoteFilesDialog::onDownloadClicked()
{
    const QString remote = selectedRemotePath();
    if (remote.isEmpty()) {
        m_status->setText(tr("Select a file first."));
        return;
    }
    const QString target = QFileDialog::getSaveFileName(this, tr("Save as"),
        QDir::current().absoluteFilePath(QFileInfo(remote).fileName()));
    if (!target.isEmpty())
        fetch(remote, target, false);
}

QString RemoteFilesDialog::cachePathFor(const QString& remotePath) const
{
    const QString key = QString::fromLatin1(QCryptographicHash::hash(remotePath.toUtf8(), QCryptographicHash::Sha1).toHex().left(10));
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/remote/")
        + m_hostCombo->currentText() + QLatin1Char('/') + key + QLatin1Char('/') + QFileInfo(remotePath).fileName();
}

void RemoteFilesDialog::fetch(const QString& remotePath, const QString& localPath, bool openAfter)
{
    if (!m_files)
        return;
    m_openAfter = openAfter;
    m_status->setText(tr("Downloading %1 ...").arg(QFileInfo(remotePath).fileName()));
    m_downloadId = m_files->download(remotePath, localPath);
}
