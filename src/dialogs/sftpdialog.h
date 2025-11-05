// sftpdialog.h - Remote SFTP File Browser Dialog
// Copyright (C) 2025 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - SFTP remote file access for HPC clusters

#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QPushButton;
class QTreeView;
class QLabel;
class QSpinBox;
class SftpItemModel;

/**
 * Dialog for browsing and opening remote files via SFTP
 * Allows connection to HPC clusters and downloading molecular structure files
 */
class SftpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SftpDialog(QWidget* parent = nullptr);
    ~SftpDialog();

    /**
     * Get the selected remote file path
     * @return Full path to selected file on remote server
     */
    QString getSelectedFile() const { return m_selectedFile; }

    /**
     * Get the local path where file was downloaded
     * @return Local temp file path, empty if not downloaded
     */
    QString getLocalPath() const { return m_localPath; }

signals:
    void fileSelected(const QString& remotePath, const QString& localPath);

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onFileDoubleClicked(const QModelIndex& index);
    void onSelectionChanged();

private:
    void setupUI();
    void connectSignals();
    void updateConnectionState(bool connected);

    // UI Components
    QLineEdit* m_hostEdit = nullptr;
    QLineEdit* m_usernameEdit = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QSpinBox* m_portSpin = nullptr;
    QPushButton* m_connectButton = nullptr;
    QPushButton* m_disconnectButton = nullptr;
    QTreeView* m_fileTreeView = nullptr;
    QPushButton* m_openButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QLabel* m_statusLabel = nullptr;

    // SFTP Model
    SftpItemModel* m_sftpModel = nullptr;

    // State
    QString m_selectedFile;
    QString m_localPath;
    bool m_isConnected = false;
};
