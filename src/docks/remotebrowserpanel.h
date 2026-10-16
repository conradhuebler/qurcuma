// remotebrowserpanel.h - Remote segment of the Project dock: browse SFTP directories like local ones
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026
//
// Compiled only with USE_SFTP. Top row: source combo (computers of the list, saved SFTP profiles,
// "Other host..."), Connect/Disconnect, one status line. Below: breadcrumb of the remote path, Up,
// Refresh, bookmark star and bookmark menu, and the listing of the current directory (folders first).
// Double click enters a folder or downloads a file through SftpCache and emits openFileRequested().
// All libssh work runs in an SftpClient on its own thread (sftpclient.h); this widget only asks
// for the host key decision and the password when the client needs them.

#pragma once

#include "remotepaths.h"

#include <QHash>
#include <QIcon>
#include <QWidget>

#include <functional>

class BreadcrumbBar;
class QComboBox;
class QLabel;
class QListView;
class QMenu;
class QModelIndex;
class QProgressBar;
class QPushButton;
class QStandardItemModel;
class QThread;
class QToolButton;
class Settings;
class SftpClient;

class RemoteBrowserPanel : public QWidget {
    Q_OBJECT

public:
    explicit RemoteBrowserPanel(Settings* settings, QWidget* parent = nullptr);
    ~RemoteBrowserPanel() override;

    /// Where "Download to Working Directory" saves files (MainWindow passes its working directory).
    void setWorkingDirectoryProvider(std::function<QString()> provider);
    /// Re-reads the computer list and the SFTP profiles into the source combo (keeps the selection).
    void reloadSources();
    void focusSourceCombo();
    /// Selects the computer ("computer:<name>") or SFTP profile id @p sourceId and connects to it.
    void connectToSource(const QString& sourceId);

signals:
    /// A remote file was downloaded (or found unchanged in the cache) and should be opened.
    void openFileRequested(const QString& localPath);
    /// A profile was added or its last use changed (recent-connections menu).
    void sftpProfilesChanged();

protected:
    void showEvent(QShowEvent* event) override;

private:
    enum class State { Disconnected, Connecting, Connected };

    void setupUi();
    void updateControls();
    void setStatus(const QString& text, bool error = false);
    void beginOperation();
    void endOperation();

    QString selectedSourceId() const;
    bool selectionIsConnection() const;
    RemoteConnection connectionForSource(const QString& sourceId) const;
    QString sourceLabel(const QString& sourceId) const;
    void onSourceActivated(int index);
    bool askOtherHost();
    void onConnectButtonClicked();
    void startConnection(const RemoteConnection& connection, const QString& startPath);
    void abandonClient(int waitMs = 0);
    void disconnectNow(const QString& status);

    // results of the SftpClient (queued to the GUI thread)
    void onHostKeyUnknown(const QString& host, const QString& fingerprint);
    void onPasswordRequired(const QString& account);
    void onConnected(const QString& homePath);
    void onConnectFailed(const QString& message);
    void onListed(const QString& path, const QList<RemoteEntry>& entries);
    void onListFailed(const QString& path, const QString& message);
    void onDownloadProgress(const QString& remotePath, qint64 received, qint64 total);
    void onDownloaded(const QString& remotePath, const QString& localPath);
    void onDownloadFailed(const QString& remotePath, const QString& message);
    bool fromCurrentClient() const;

    void navigateTo(const QString& path);
    void onItemDoubleClicked(const QModelIndex& index);
    const RemoteEntry* entryAt(const QModelIndex& index) const;
    void openRemoteFile(const RemoteEntry& entry, const QString& remotePath);
    void downloadToWorkingDirectory(const RemoteEntry& entry, const QString& remotePath);
    void showContextMenu(const QPoint& pos);
    QIcon iconFor(const RemoteEntry& entry);

    QString bookmarkIdFor(const QString& path) const;
    void addBookmark(const QString& path);
    void toggleBookmark();
    void rebuildBookmarkMenu();
    void openBookmark(const QString& bookmarkId);

    Settings* m_settings = nullptr;
    QComboBox* m_sourceCombo = nullptr;
    QPushButton* m_connectButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    QToolButton* m_upButton = nullptr;
    QToolButton* m_refreshButton = nullptr;
    QToolButton* m_bookmarkButton = nullptr;
    QToolButton* m_bookmarkMenuButton = nullptr;
    QMenu* m_bookmarkMenu = nullptr;
    BreadcrumbBar* m_breadcrumb = nullptr;
    QListView* m_listView = nullptr;
    QStandardItemModel* m_model = nullptr;

    QThread* m_thread = nullptr;
    SftpClient* m_client = nullptr;
    State m_state = State::Disconnected;
    int m_pendingOperations = 0;
    int m_lastSourceIndex = -1;

    RemoteConnection m_connection;   ///< connected (or connecting) source
    RemoteConnection m_unsavedHost;  ///< last "Other host..." entry that was not saved as a profile
    QString m_home;
    QString m_currentPath;
    QString m_requestedPath;
    QString m_startPath;             ///< directory to show right after connecting (bookmark)
    QString m_deferredError;         ///< shown after the fallback listing of the home directory
    QList<RemoteEntry> m_entries;
    QHash<QString, bool> m_downloadOpens;  ///< local target -> open in the viewer (else: saved only)
    QHash<QString, QIcon> m_iconCache;
    std::function<QString()> m_workingDirectory;
};
