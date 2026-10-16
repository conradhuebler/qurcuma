// remotebrowserpanel.h - Remote segment of the Project dock, laid out like the local Files segment
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026
//
// Compiled only with USE_SFTP. Mirrors the local structure: a "Choose working directory" button (placed
// by ProjectDock in its top row instead of the local one), the breadcrumb of the remote working
// directory, the list of its subdirectories with ".." and "+ New Directory", and below the filter
// row (shared FileFilterBar) and the files of the directory selected above. On top of the local
// structure: a compact row with the source combo (computers of the list, saved SFTP profiles,
// "Other host..."), Connect/Disconnect and a status line, plus Refresh and remote bookmarks.
// The remote working directory is remembered per source (remotepaths::workDirSettingsKey).
// All libssh work runs in an SftpClient on its own thread (sftpclient.h).

#pragma once

#include "remotepaths.h"

#include <QHash>
#include <QIcon>
#include <QWidget>

#include <functional>

class BreadcrumbBar;
class DirectoryFilterProxyModel;
class FileFilterBar;
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
class RemoteDirectoryChooser;
class Settings;
class SftpClient;

class RemoteBrowserPanel : public QWidget {
    Q_OBJECT

public:
    explicit RemoteBrowserPanel(Settings* settings, QWidget* parent = nullptr);
    ~RemoteBrowserPanel() override;

    /// The remote "Choose working directory" button. It is not part of this widget's layout:
    /// ProjectDock puts it into its top row and shows it instead of the local one in this segment.
    QPushButton* chooseWorkDirButton() const;
    /// Where "Download to Local Working Directory" saves files (MainWindow passes its working directory).
    void setWorkingDirectoryProvider(std::function<QString()> provider);
    /// Re-reads the computer list and the SFTP profiles into the source combo (keeps the selection).
    void reloadSources();
    void focusSourceCombo();
    /// Selects the computer ("computer:<name>") or SFTP profile id @p sourceId and connects to it.
    void connectToSource(const QString& sourceId);
    /// Selects the computer @p computerName of the computer list as source, connects if needed and makes
    /// @p remotePath the remote working directory (results of a remote run). Says so in the status line
    /// when the computer is no longer in the list.
    void showLocation(const QString& computerName, const QString& remotePath);

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
    int requestList(const QString& path);

    // sources and connection
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
    void clearListing();

    // results of the SftpClient (queued to the GUI thread)
    void onHostKeyUnknown(const QString& host, const QString& fingerprint);
    void onPasswordRequired(const QString& account);
    void onConnected(const QString& homePath);
    void onConnectFailed(const QString& message);
    void onListed(int requestId, const QString& path, const QList<RemoteEntry>& entries);
    void onListFailed(int requestId, const QString& path, const QString& message);
    void onDownloadProgress(const QString& remotePath, qint64 received, qint64 total);
    void onDownloaded(const QString& remotePath, const QString& localPath);
    void onDownloadFailed(const QString& remotePath, const QString& message);
    void onDirectoryCreated(const QString& path);
    void onMakeDirectoryFailed(const QString& path, const QString& message);
    bool fromCurrentClient() const;

    // working directory and the two lists (as MainWindow::switchWorkingDirectory / projectSelected)
    void setWorkingDirectory(const QString& path);
    void chooseWorkingDirectory();
    void refresh();
    void showDirectories();
    void showFiles(const QString& dir, const QList<RemoteEntry>& entries);
    void onDirectorySelected(const QModelIndex& index);
    void showDirectoryContextMenu(const QPoint& pos);
    void createDirectory();

    // files
    void onFileDoubleClicked(const QModelIndex& index);
    const RemoteEntry* fileEntryAt(const QModelIndex& proxyIndex) const;
    void openRemoteFile(const RemoteEntry& entry, const QString& remotePath);
    void downloadToWorkingDirectory(const RemoteEntry& entry, const QString& remotePath);
    void showFileContextMenu(const QPoint& pos);
    QIcon iconFor(const RemoteEntry& entry);

    // bookmarks (of the remote working directory)
    QString bookmarkIdFor(const QString& path) const;
    void addBookmark(const QString& path);
    void toggleBookmark();
    void rebuildBookmarkMenu();
    void openBookmark(const QString& bookmarkId);

    Settings* m_settings = nullptr;
    QPushButton* m_chooseButton = nullptr;
    QComboBox* m_sourceCombo = nullptr;
    QPushButton* m_connectButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    BreadcrumbBar* m_breadcrumb = nullptr;
    QToolButton* m_refreshButton = nullptr;
    QToolButton* m_bookmarkButton = nullptr;
    QToolButton* m_bookmarkMenuButton = nullptr;
    QMenu* m_bookmarkMenu = nullptr;
    QListView* m_dirView = nullptr;
    QStandardItemModel* m_dirModel = nullptr;
    QPushButton* m_newDirButton = nullptr;
    FileFilterBar* m_filterBar = nullptr;
    DirectoryFilterProxyModel* m_fileProxy = nullptr;
    QListView* m_fileView = nullptr;
    QStandardItemModel* m_fileModel = nullptr;
    RemoteDirectoryChooser* m_chooser = nullptr;  ///< open modal chooser, if any

    QThread* m_thread = nullptr;
    SftpClient* m_client = nullptr;
    State m_state = State::Disconnected;
    int m_pendingOperations = 0;
    int m_lastSourceIndex = -1;
    int m_nextRequestId = 0;
    int m_workDirRequest = -1;
    int m_filesRequest = -1;
    int m_chooserRequest = -1;
    bool m_chooseAfterConnect = false;

    RemoteConnection m_connection;   ///< connected (or connecting) source
    RemoteConnection m_unsavedHost;  ///< last "Other host..." entry that was not saved as a profile
    QString m_home;
    QString m_workDir;               ///< remote working directory (listed in the upper list)
    QString m_requestedWorkDir;
    QString m_selectedDir;           ///< directory whose files the lower list shows
    QString m_filesDir;              ///< directory of m_fileEntries
    QString m_startPath;             ///< working directory right after connecting (bookmark)
    QString m_selectAfterListing;    ///< subdirectory to select once the working directory is listed
    QString m_deferredError;         ///< shown after the fallback listing of the home directory
    QList<RemoteEntry> m_workEntries;
    QList<RemoteEntry> m_fileEntries;
    QHash<QString, bool> m_downloadOpens;  ///< local target -> open in the viewer (else: saved only)
    QHash<QString, QIcon> m_iconCache;
    std::function<QString()> m_workingDirectory;
};
