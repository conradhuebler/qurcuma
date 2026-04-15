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
class QComboBox;  // Claude Generated - For profile/SSH config dropdowns
class QCheckBox;  // Claude Generated - For SSH key auth checkbox
class SftpItemModel;

/**
 * Dialog for browsing and opening remote files via SFTP
 * Allows connection to HPC clusters and downloading molecular structure files
 */
class SftpDialog : public QDialog
{
    Q_OBJECT

public:
    // Claude Generated - Remote Directory Mounting
    enum class Mode {
        OpenFile,           // Download and open a single file
        SelectDirectory     // Select a directory to mount as workspace
    };

    explicit SftpDialog(QWidget* parent = nullptr);
    ~SftpDialog();

    // Claude Generated - Remote Directory Mounting
    void setMode(Mode mode);
    QString getSelectedDirectory() const { return m_selectedDirectory; }
    QString getSelectedProfileId() const { return m_selectedProfileId; }

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
    void onProfileSelected(int index);  // Claude Generated - Profile dropdown selection
    void onSSHConfigHostSelected(int index);  // Claude Generated - SSH config host selection
    void onSaveProfileClicked();  // Claude Generated - Save current connection as profile

private:
    void setupUI();
    void connectSignals();
    void updateConnectionState(bool connected);
    void loadProfiles();  // Claude Generated - Load saved profiles into dropdown
    void loadSSHConfigHosts();  // Claude Generated - Load SSH config hosts into dropdown
    void fillFormFromProfile(const QString& profileId);  // Claude Generated - Populate form fields

    // UI Components
    QComboBox* m_profileCombo = nullptr;  // Claude Generated - Saved profiles dropdown
    QComboBox* m_sshConfigCombo = nullptr;  // Claude Generated - SSH config hosts dropdown
    QLineEdit* m_hostEdit = nullptr;
    QLineEdit* m_usernameEdit = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QSpinBox* m_portSpin = nullptr;
    QCheckBox* m_useSSHKeyCheckbox = nullptr;  // Claude Generated - Use SSH key auth
    QPushButton* m_saveProfileButton = nullptr;  // Claude Generated - Save profile button
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

    // Claude Generated - Remote Directory Mounting
    Mode m_mode = Mode::OpenFile;
    QString m_selectedDirectory;
    QString m_selectedProfileId;
};
