// sftpmodel.hpp
#pragma once

#include <QAbstractItemModel>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <fcntl.h>  // Claude Generated - For O_RDONLY, O_WRONLY, etc.
#include <sys/stat.h>  // Claude Generated - For S_IRWXU

class SftpItemModel : public QAbstractItemModel {
    Q_OBJECT

private:
    struct SftpItem {
        QString m_name;
        QString m_path;
        bool m_isDir;
        qint64 m_size;
        QDateTime m_lastModified;
        SftpItem* m_parent;
        QVector<SftpItem*> m_children;
        bool m_isLoaded{ false };

        SftpItem(const QString& name = QString(),
            const QString& path = QString(),
            SftpItem* parent = nullptr)
            : m_name(name)
            , m_path(path)
            , m_isDir(false)
            , m_size(0)
            , m_parent(parent)
        {
        }

        ~SftpItem()
        {
            qDeleteAll(m_children);
        }
    };

    QString m_host;
    QString m_username;
    QString m_password;
    int m_port{ 22 };
    QString m_keyPath;  // Claude Generated - Path to SSH private key
    bool m_useKeyAuth{ false };  // Claude Generated - Try key auth before password
    QString m_proxyCommand;  // Claude Generated - ProxyCommand for jump hosts

    ssh_session m_sshSession{ nullptr };
    sftp_session m_sftpSession{ nullptr };
    bool m_isConnected{ false };
    SftpItem* m_rootItem{ nullptr };

public:
    SftpItemModel(const QString& host,
        const QString& username,
        const QString& password,
        int port = 22,  // Claude Generated - Port parameter
        QObject* parent = nullptr)
        : QAbstractItemModel(parent)
        , m_host(host)
        , m_username(username)
        , m_password(password)
        , m_port(port)
    {

        m_rootItem = new SftpItem("/", "/");
        m_rootItem->m_isDir = true;

        if (connectToHost()) {
            loadDirectory(m_rootItem);
            m_rootItem->m_isLoaded = true;  // Claude Generated - Mark root as loaded
        }
    }

    ~SftpItemModel()
    {
        if (m_isConnected) {
            sftp_free(m_sftpSession);
            ssh_disconnect(m_sshSession);
            ssh_free(m_sshSession);
        }
        delete m_rootItem;
    }

    // Helper methods for accessing item data (Claude Generated - Phase SFTP Integration)
    QString getItemPath(const QModelIndex& index) const
    {
        if (!index.isValid())
            return QString();
        SftpItem* item = static_cast<SftpItem*>(index.internalPointer());
        return item ? item->m_path : QString();
    }

    QString getItemName(const QModelIndex& index) const
    {
        if (!index.isValid())
            return QString();
        SftpItem* item = static_cast<SftpItem*>(index.internalPointer());
        return item ? item->m_name : QString();
    }

    bool isDirectory(const QModelIndex& index) const
    {
        if (!index.isValid())
            return false;
        SftpItem* item = static_cast<SftpItem*>(index.internalPointer());
        return item ? item->m_isDir : false;
    }

    QString getLastError() const
    {
        if (m_sshSession) {
            return QString::fromUtf8(ssh_get_error(m_sshSession));
        }
        return tr("No error information available");
    }

    // SSH Key authentication methods (Claude Generated - Phase SFTP Integration)
    void setKeyFile(const QString& keyPath)
    {
        m_keyPath = keyPath;
        m_useKeyAuth = !keyPath.isEmpty();
    }

    void setUseKeyAuth(bool enabled)
    {
        m_useKeyAuth = enabled;
    }

    bool isUsingKeyAuth() const
    {
        return m_useKeyAuth;
    }

    // ProxyCommand/ProxyJump support (Claude Generated - ProxyJump Support)
    void setProxyCommand(const QString& command)
    {
        m_proxyCommand = command;
    }

    QString getProxyCommand() const
    {
        return m_proxyCommand;
    }

    // Connection status check (Claude Generated - SFTP Bug Fix)
    bool isConnected() const
    {
        return m_isConnected;
    }

    // File transfer methods
    bool downloadFile(const QString& remotePath, const QString& localPath)
    {
        if (!m_isConnected)
            return false;

        sftp_file file = sftp_open(m_sftpSession,
            remotePath.toUtf8().constData(),
            O_RDONLY, 0);
        if (!file) {
            qWarning() << "Failed to open remote file:" << remotePath;
            return false;
        }

        QFile localFile(localPath);
        if (!localFile.open(QIODevice::WriteOnly)) {
            sftp_close(file);
            return false;
        }

        char buffer[4096];
        ssize_t bytesRead;
        while ((bytesRead = sftp_read(file, buffer, sizeof(buffer))) > 0) {
            localFile.write(buffer, bytesRead);
        }

        sftp_close(file);
        localFile.close();
        return bytesRead >= 0;
    }

    bool uploadFile(const QString& localPath, const QString& remotePath)
    {
        if (!m_isConnected)
            return false;

        QFile localFile(localPath);
        if (!localFile.open(QIODevice::ReadOnly)) {
            return false;
        }

        sftp_file file = sftp_open(m_sftpSession,
            remotePath.toUtf8().constData(),
            O_WRONLY | O_CREAT | O_TRUNC,
            S_IRWXU);
        if (!file) {
            localFile.close();
            return false;
        }

        char buffer[4096];
        qint64 bytesRead;
        while ((bytesRead = localFile.read(buffer, sizeof(buffer))) > 0) {
            ssize_t written = sftp_write(file, buffer, bytesRead);
            if (written != bytesRead) {
                sftp_close(file);
                localFile.close();
                return false;
            }
        }

        sftp_close(file);
        localFile.close();
        return true;
    }

    // Required QAbstractItemModel overrides
    QModelIndex index(int row, int column,
        const QModelIndex& parent = QModelIndex()) const override
    {
        if (!hasIndex(row, column, parent))
            return QModelIndex();

        SftpItem* parentItem = parent.isValid()
            ? static_cast<SftpItem*>(parent.internalPointer())
            : m_rootItem;

        SftpItem* childItem = parentItem->m_children.value(row);
        if (childItem)
            return createIndex(row, column, childItem);

        return QModelIndex();
    }

    QModelIndex parent(const QModelIndex& index) const override
    {
        if (!index.isValid())
            return QModelIndex();

        SftpItem* childItem = static_cast<SftpItem*>(index.internalPointer());
        SftpItem* parentItem = childItem->m_parent;

        if (parentItem == m_rootItem)
            return QModelIndex();

        int row = parentItem->m_parent->m_children.indexOf(parentItem);
        return createIndex(row, 0, parentItem);
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        SftpItem* parentItem = parent.isValid()
            ? static_cast<SftpItem*>(parent.internalPointer())
            : m_rootItem;

        return parentItem->m_children.count();
    }

    int columnCount(const QModelIndex& = QModelIndex()) const override
    {
        return 4; // Name, Size, Type, Last Modified
    }

    // Lazy loading support (Claude Generated - Phase SFTP Integration)
    bool hasChildren(const QModelIndex& parent = QModelIndex()) const override
    {
        if (!parent.isValid())
            return m_rootItem && m_rootItem->m_children.count() > 0;

        SftpItem* item = static_cast<SftpItem*>(parent.internalPointer());
        // Directories can have children (even if not loaded yet)
        return item && item->m_isDir;
    }

    bool canFetchMore(const QModelIndex& parent) const override
    {
        if (!parent.isValid())
            return false;

        SftpItem* item = static_cast<SftpItem*>(parent.internalPointer());
        // Can fetch if it's a directory and not yet loaded
        return item && item->m_isDir && !item->m_isLoaded;
    }

    void fetchMore(const QModelIndex& parent) override
    {
        if (!parent.isValid())
            return;

        SftpItem* item = static_cast<SftpItem*>(parent.internalPointer());
        if (item && item->m_isDir && !item->m_isLoaded) {
            loadDirectory(item);
            item->m_isLoaded = true;
        }
    }

    QVariant data(const QModelIndex& index,
        int role = Qt::DisplayRole) const override
    {
        if (!index.isValid())
            return QVariant();

        SftpItem* item = static_cast<SftpItem*>(index.internalPointer());

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
            case 0:
                return item->m_name;
            case 1:
                return item->m_isDir ? QVariant() : item->m_size;
            case 2:
                return item->m_isDir ? "Directory" : "File";
            case 3:
                return item->m_lastModified;
            default:
                return QVariant();
            }
        }

        return QVariant();
    }

private:
    bool connectToHost()
    {
        m_sshSession = ssh_new();
        if (!m_sshSession) {
            qCritical() << "[SFTP] Failed to create SSH session object";
            return false;
        }

        qDebug() << "[SFTP] [INIT] Creating SSH session";
        qDebug() << "[SFTP] [CONFIG] Host:" << m_host << "User:" << m_username << "Port:" << m_port;

        // Set connection options (Claude Generated - Phase SFTP Integration)
        ssh_options_set(m_sshSession, SSH_OPTIONS_HOST,
            m_host.toUtf8().constData());
        ssh_options_set(m_sshSession, SSH_OPTIONS_USER,
            m_username.toUtf8().constData());

        // Set port if not default
        if (m_port != 22) {
            unsigned int port = m_port;
            ssh_options_set(m_sshSession, SSH_OPTIONS_PORT, &port);
        }

        // Set ProxyCommand if provided (Claude Generated - ProxyJump Support)
        if (!m_proxyCommand.isEmpty()) {
            qDebug() << "[SFTP] [PROXY] Using ProxyCommand:" << m_proxyCommand;
            ssh_options_set(m_sshSession, SSH_OPTIONS_PROXYCOMMAND,
                           m_proxyCommand.toUtf8().constData());
        }

        // Connect to server
        qDebug() << "[SFTP] [CONNECT] Attempting SSH connection...";
        if (ssh_connect(m_sshSession) != SSH_OK) {
            QString errorMsg = QString::fromUtf8(ssh_get_error(m_sshSession));
            qCritical() << "[SFTP] [CONNECT] SSH connection failed:" << errorMsg;
            qWarning() << "[SFTP] [CONNECT] Check: hostname reachable? firewall? port" << m_port << "open?";
            ssh_free(m_sshSession);
            return false;
        }

        qDebug() << "[SFTP] [CONNECT] SSH connected to" << m_host << "port" << m_port;

        // Authentication: Try key first, then password (Claude Generated - Phase SFTP Integration)
        bool authenticated = false;
        qDebug() << "[SFTP] [AUTH] Starting authentication process...";

        if (m_useKeyAuth) {
            // Try SSH key authentication
            if (!m_keyPath.isEmpty()) {
                // Use specific key file
                qDebug() << "[SFTP] [AUTH] Trying SSH key file:" << m_keyPath;
                int rc = ssh_userauth_publickey_auto(m_sshSession, nullptr, nullptr);
                if (rc == SSH_AUTH_SUCCESS) {
                    authenticated = true;
                    qDebug() << "[SFTP] [AUTH] ✓ Authenticated with SSH key:" << m_keyPath;
                } else if (rc == SSH_AUTH_PARTIAL) {
                    qWarning() << "[SFTP] [AUTH] Key auth partial, needs more methods";
                } else {
                    qWarning() << "[SFTP] [AUTH] SSH key auth failed:" << QString::fromUtf8(ssh_get_error(m_sshSession));
                }
            } else {
                // Auto-detect standard keys (~/.ssh/id_rsa, id_ed25519, etc.)
                qDebug() << "[SFTP] [AUTH] Trying auto-detected SSH keys (~/.ssh/id_rsa, id_ed25519, etc.)";
                int rc = ssh_userauth_publickey_auto(m_sshSession, nullptr, nullptr);
                if (rc == SSH_AUTH_SUCCESS) {
                    authenticated = true;
                    qDebug() << "[SFTP] [AUTH] ✓ Authenticated with auto-detected SSH key";
                } else if (rc == SSH_AUTH_PARTIAL) {
                    qWarning() << "[SFTP] [AUTH] Auto key auth partial, needs more methods";
                } else {
                    qDebug() << "[SFTP] [AUTH] Auto-detect SSH keys failed:" << QString::fromUtf8(ssh_get_error(m_sshSession));
                }
            }
        }

        // Fallback to password authentication if key auth failed or not enabled
        if (!authenticated && !m_password.isEmpty()) {
            qDebug() << "[SFTP] [AUTH] Trying password authentication...";
            int rc = ssh_userauth_password(m_sshSession, nullptr,
                    m_password.toUtf8().constData());
            if (rc == SSH_AUTH_SUCCESS) {
                authenticated = true;
                qDebug() << "[SFTP] [AUTH] ✓ Authenticated with password";
            } else if (rc == SSH_AUTH_PARTIAL) {
                qWarning() << "[SFTP] [AUTH] Password auth partial, needs more methods";
            } else {
                qWarning() << "[SFTP] [AUTH] Password authentication failed:" << QString::fromUtf8(ssh_get_error(m_sshSession));
                qWarning() << "[SFTP] [AUTH] Check: credentials correct? user has password auth enabled?";
            }
        }

        if (!authenticated) {
            QString errorMsg = QString::fromUtf8(ssh_get_error(m_sshSession));
            qCritical() << "[SFTP] [AUTH] All authentication methods failed:" << errorMsg;
            qWarning() << "[SFTP] [AUTH] Tried methods: SSH key, password";
            qWarning() << "[SFTP] [AUTH] Solutions: verify credentials, check SSH config, try explicit key file";
            ssh_disconnect(m_sshSession);
            ssh_free(m_sshSession);
            return false;
        }

        qDebug() << "[SFTP] [SFTP] Authenticated successfully, initializing SFTP session...";

        // Initialize SFTP session
        qDebug() << "[SFTP] [SFTP-INIT] Creating SFTP session object...";
        m_sftpSession = sftp_new(m_sshSession);
        if (!m_sftpSession) {
            qCritical() << "[SFTP] [SFTP-INIT] Failed to create SFTP session object";
            qWarning() << "[SFTP] [SFTP-INIT] Check: server supports SFTP subsystem?";
            ssh_disconnect(m_sshSession);
            ssh_free(m_sshSession);
            return false;
        }

        qDebug() << "[SFTP] [SFTP-INIT] Initializing SFTP subsystem...";
        if (sftp_init(m_sftpSession) != SSH_OK) {
            QString errorMsg = QString::fromUtf8(ssh_get_error(m_sshSession));
            qCritical() << "[SFTP] [SFTP-INIT] Failed to initialize SFTP:" << errorMsg;
            qWarning() << "[SFTP] [SFTP-INIT] Check: SFTP subsystem enabled on server?";
            qWarning() << "[SFTP] [SFTP-INIT] Check: /etc/ssh/sshd_config has Subsystem sftp?";
            sftp_free(m_sftpSession);
            ssh_disconnect(m_sshSession);
            ssh_free(m_sshSession);
            return false;
        }

        qDebug() << "[SFTP] [SFTP-INIT] ✓ SFTP session initialized successfully";
        qDebug() << "[SFTP] [READY] Connected and ready to browse remote files";
        m_isConnected = true;
        return true;
    }

    void loadDirectory(SftpItem* parent)
    {
        if (!m_isConnected)
            return;

        sftp_dir dir = sftp_opendir(m_sftpSession,
            parent->m_path.toUtf8().constData());
        if (!dir)
            return;

        // Collect items first (Claude Generated - for proper beginInsertRows)
        QVector<SftpItem*> newItems;
        sftp_attributes attributes;
        while ((attributes = sftp_readdir(m_sftpSession, dir))) {
            QString name = QString::fromUtf8(attributes->name);

            if (name == "." || name == "..") {
                sftp_attributes_free(attributes);
                continue;
            }

            // Normalize path construction to avoid double slashes (Claude Generated - SFTP Bug Fix)
            QString itemPath = parent->m_path;
            if (!itemPath.endsWith('/')) {
                itemPath += '/';
            }
            itemPath += name;

            SftpItem* item = new SftpItem(name, itemPath, parent);

            item->m_isDir = attributes->type == SSH_FILEXFER_TYPE_DIRECTORY;
            item->m_size = attributes->size;
            item->m_lastModified = QDateTime::fromSecsSinceEpoch(
                attributes->mtime);

            newItems.append(item);
            sftp_attributes_free(attributes);
        }

        sftp_closedir(dir);

        // Notify view of changes (Claude Generated - Phase SFTP Integration)
        if (!newItems.isEmpty()) {
            QModelIndex parentIndex;
            if (parent != m_rootItem) {
                // Find parent index
                int row = parent->m_parent ? parent->m_parent->m_children.indexOf(parent) : 0;
                parentIndex = createIndex(row, 0, parent);
            }

            beginInsertRows(parentIndex, parent->m_children.count(),
                           parent->m_children.count() + newItems.count() - 1);
            parent->m_children.append(newItems);
            endInsertRows();
        }
    }
};
