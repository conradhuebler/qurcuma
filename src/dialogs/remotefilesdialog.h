// remotefilesdialog.h - Browse and fetch files on a computer running qurcuma-server
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 (WP remote compute R4, docs/WP-remote-compute-vr.md). A dialog
// (not a dock) over remote::RemoteFiles: pick a host from ~/.ssh/config, connect, walk the
// directories the server shares, download a file or open a structure in the viewer.
// Downloads of "open" go to the cache directory; "Download" asks for a target.

#pragma once

#include <QDialog>
#include <QVector>

#include "remote/remotefiles.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPlainTextEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class RemoteFilesDialog : public QDialog {
    Q_OBJECT
public:
    explicit RemoteFilesDialog(QWidget* parent = nullptr);
    ~RemoteFilesDialog() override;

signals:
    /// A file was fetched for opening; the caller loads it into the viewer.
    void openFileRequested(const QString& localPath);

private slots:
    void onConnectClicked();
    void onListing(const QString& path, const QVector<remote::RemoteFiles::Entry>& entries, bool truncated);
    void onItemActivated(QTreeWidgetItem* item);
    void onDownloadClicked();
    void onUpClicked();

private:
    void disconnectFromHost();
    void navigate(const QString& path);
    void fetch(const QString& remotePath, const QString& localPath, bool openAfter);
    QString cachePathFor(const QString& remotePath) const;
    QString selectedRemotePath() const;
    void setConnected(bool on);

    QComboBox* m_hostCombo = nullptr;
    QLineEdit* m_serverCommand = nullptr;
    QPushButton* m_connectButton = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QPushButton* m_upButton = nullptr;
    QTreeWidget* m_tree = nullptr;
    QPushButton* m_downloadButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_status = nullptr;
    QPlainTextEdit* m_log = nullptr;

    remote::RemoteFiles* m_files = nullptr;
    QString m_currentPath;
    QStringList m_roots;
    bool m_openAfter = false;
    quint32 m_downloadId = 0;
};
