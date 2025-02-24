// sftpmodel.hpp
#pragma once

#include <QAbstractItemModel>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <libssh/libssh.h>
#include <libssh/sftp.h>

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

    ssh_session m_sshSession{ nullptr };
    sftp_session m_sftpSession{ nullptr };
    bool m_isConnected{ false };
    SftpItem* m_rootItem{ nullptr };

public:
    SftpItemModel(const QString& host,
        const QString& username,
        const QString& password,
        QObject* parent = nullptr)
        : QAbstractItemModel(parent)
        , m_host(host)
        , m_username(username)
        , m_password(password)
    {

        m_rootItem = new SftpItem("/", "/");
        m_rootItem->m_isDir = true;

        if (connectToHost()) {
            loadDirectory(m_rootItem);
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
        if (!m_sshSession)
            return false;

        ssh_options_set(m_sshSession, SSH_OPTIONS_HOST,
            m_host.toUtf8().constData());
        ssh_options_set(m_sshSession, SSH_OPTIONS_USER,
            m_username.toUtf8().constData());

        if (ssh_connect(m_sshSession) != SSH_OK) {
            ssh_free(m_sshSession);
            return false;
        }

        if (ssh_userauth_password(m_sshSession, nullptr,
                m_password.toUtf8().constData())
            != SSH_AUTH_SUCCESS) {
            ssh_disconnect(m_sshSession);
            ssh_free(m_sshSession);
            return false;
        }

        m_sftpSession = sftp_new(m_sshSession);
        if (!m_sftpSession) {
            ssh_disconnect(m_sshSession);
            ssh_free(m_sshSession);
            return false;
        }

        if (sftp_init(m_sftpSession) != SSH_OK) {
            sftp_free(m_sftpSession);
            ssh_disconnect(m_sshSession);
            ssh_free(m_sshSession);
            return false;
        }

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

        sftp_attributes attributes;
        while ((attributes = sftp_readdir(m_sftpSession, dir))) {
            QString name = QString::fromUtf8(attributes->name);

            if (name == "." || name == "..") {
                sftp_attributes_free(attributes);
                continue;
            }

            SftpItem* item = new SftpItem(
                name,
                parent->m_path + "/" + name,
                parent);

            item->m_isDir = attributes->type == SSH_FILEXFER_TYPE_DIRECTORY;
            item->m_size = attributes->size;
            item->m_lastModified = QDateTime::fromSecsSinceEpoch(
                attributes->mtime);

            parent->m_children.append(item);
            sftp_attributes_free(attributes);
        }

        sftp_closedir(dir);
    }
};
