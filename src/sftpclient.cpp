// sftpclient.cpp - libssh SFTP session that runs on its own thread (remote file browser)
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026
//
// The connect, ProxyCommand, key/password authentication, listing and download code was taken
// over from the former header-only SftpItemModel (sftpmodel.hpp) and extended by a host key check,
// keyboard-interactive login, symlink resolution and error texts that name the cause.

#include "sftpclient.h"

#include <QFileInfo>

#include <QByteArray>
#include <QElapsedTimer>
#include <QFile>

#include <libssh/libssh.h>
#include <libssh/sftp.h>

#include <fcntl.h>  // O_RDONLY

SftpClient::SftpClient(QObject* parent)
    : QObject(parent)
{
}

SftpClient::~SftpClient()
{
    closeSession();
}

/** @brief Marks running work as abandoned (called from the GUI thread). Claude Generated 2026 */
void SftpClient::requestCancel()
{
    m_cancel = true;
}

QString SftpClient::sshError() const
{
    return m_ssh ? QString::fromUtf8(ssh_get_error(m_ssh)) : QString();
}

void SftpClient::closeSession()
{
    if (m_sftp) {
        sftp_free(m_sftp);
        m_sftp = nullptr;
    }
    if (m_ssh) {
        if (ssh_is_connected(m_ssh))
            ssh_disconnect(m_ssh);
        ssh_free(m_ssh);
        m_ssh = nullptr;
    }
    m_authMethods = 0;
}

void SftpClient::fail(const QString& message)
{
    closeSession();
    emit connectFailed(message);
}

/** @brief Key type and SHA-256 fingerprint of the server key ("ssh-ed25519 SHA256:..."). Claude Generated 2026 */
QString SftpClient::serverFingerprint() const
{
    ssh_key key = nullptr;
    if (ssh_get_server_publickey(m_ssh, &key) != SSH_OK || !key)
        return QString();
    QString text = QString::fromUtf8(ssh_key_type_to_char(ssh_key_type(key)));
    unsigned char* hash = nullptr;
    size_t length = 0;
    if (ssh_get_publickey_hash(key, SSH_PUBLICKEY_HASH_SHA256, &hash, &length) == 0) {
        if (char* fingerprint = ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256, hash, length)) {
            text += QLatin1Char(' ') + QString::fromUtf8(fingerprint);
            ssh_string_free_char(fingerprint);
        }
        ssh_clean_pubkey_hash(&hash);
    }
    ssh_key_free(key);
    return text;
}

/** @brief Opens the session and checks the host key against ~/.ssh/known_hosts. Claude Generated 2026 */
void SftpClient::connectTo(const RemoteConnection& connection)
{
    closeSession();
    m_connection = connection;
    m_ssh = ssh_new();
    if (!m_ssh) {
        emit connectFailed(tr("libssh could not create a session."));
        return;
    }

    const QByteArray host = connection.host.toUtf8();
    ssh_options_set(m_ssh, SSH_OPTIONS_HOST, host.constData());
    if (!connection.user.isEmpty()) {
        const QByteArray user = connection.user.toUtf8();
        ssh_options_set(m_ssh, SSH_OPTIONS_USER, user.constData());
    }
    const unsigned int port = connection.port > 0 ? static_cast<unsigned int>(connection.port) : 22u;
    ssh_options_set(m_ssh, SSH_OPTIONS_PORT, &port);
    const long timeoutSeconds = 20;  // connect and every later blocking wait
    ssh_options_set(m_ssh, SSH_OPTIONS_TIMEOUT, &timeoutSeconds);
    if (!connection.proxyCommand.isEmpty()) {
        const QByteArray proxy = connection.proxyCommand.toUtf8();
        ssh_options_set(m_ssh, SSH_OPTIONS_PROXYCOMMAND, proxy.constData());
    }

    if (ssh_connect(m_ssh) != SSH_OK) {
        const QString via = connection.proxyCommand.isEmpty() ? QString() : tr(" via \"%1\"").arg(connection.proxyCommand);
        fail(tr("Could not reach %1 (port %2)%3: %4").arg(connection.host).arg(port).arg(via, sshError()));
        return;
    }
    if (m_cancel) {
        closeSession();
        return;
    }

    switch (ssh_session_is_known_server(m_ssh)) {
    case SSH_KNOWN_HOSTS_OK:
        authenticateWithKeys();
        return;
    case SSH_KNOWN_HOSTS_CHANGED:
        fail(tr("The host key of %1 has changed. Someone may be intercepting the connection, or the server "
                "was reinstalled. Check the key and remove the old entry from ~/.ssh/known_hosts "
                "(ssh-keygen -R %1).").arg(connection.host));
        return;
    case SSH_KNOWN_HOSTS_OTHER:
        fail(tr("%1 offers a host key of another type than the one stored in ~/.ssh/known_hosts. "
                "Check the key with ssh before connecting.").arg(connection.host));
        return;
    case SSH_KNOWN_HOSTS_NOT_FOUND:
    case SSH_KNOWN_HOSTS_UNKNOWN:
        emit hostKeyUnknown(connection.host, serverFingerprint());
        return;
    case SSH_KNOWN_HOSTS_ERROR:
    default:
        fail(tr("Could not check the host key of %1: %2").arg(connection.host, sshError()));
        return;
    }
}

/** @brief Continues after the user has seen the fingerprint of an unknown host. Claude Generated 2026 */
void SftpClient::answerHostKey(bool trust)
{
    if (!m_ssh)
        return;
    if (!trust) {
        fail(tr("The host key of %1 was not accepted.").arg(m_connection.host));
        return;
    }
    // A known_hosts file that cannot be written only means the question comes again next time.
    ssh_session_update_known_hosts(m_ssh);
    authenticateWithKeys();
}

/** @brief Asks the server for its methods, then tries the ssh agent and the standard keys. Claude Generated 2026 */
void SftpClient::authenticateWithKeys()
{
    const QString account = m_connection.account();
    int rc = ssh_userauth_none(m_ssh, nullptr);
    if (rc == SSH_AUTH_SUCCESS) {
        openSftp();
        return;
    }
    if (rc == SSH_AUTH_ERROR) {
        fail(tr("Authentication with %1 failed: %2").arg(account, sshError()));
        return;
    }
    m_authMethods = ssh_userauth_list(m_ssh, nullptr);
    // A key file given for the computer is tried first: a rejected or unreadable one only means that the
    // agent, the standard keys and the password follow, a protected one asks for its passphrase once.
    if ((m_authMethods & SSH_AUTH_METHOD_PUBLICKEY) && !m_connection.identityFile.trimmed().isEmpty()) {
        switch (tryExplicitKey(QString(), false)) {
        case KeyResult::Success:
            openSftp();
            return;
        case KeyResult::NeedsPassphrase:
            emit passphraseRequired(remotepaths::privateKeyPath(m_connection.identityFile));
            return;
        case KeyResult::Rejected:
        case KeyResult::Unusable:
            break;
        }
    }
    continueAuthentication();
}

/** @brief Loads the computer's key file and offers it to the server. Claude Generated 2026 */
SftpClient::KeyResult SftpClient::tryExplicitKey(const QString& passphrase, bool passphraseGiven)
{
    const QString path = remotepaths::privateKeyPath(m_connection.identityFile);
    const QFileInfo info(path);
    if (!info.isFile() || !info.isReadable())
        return KeyResult::Unusable;
    ssh_key key = nullptr;
    const QByteArray secret = passphrase.toUtf8();
    const int imported = ssh_pki_import_privkey_file(path.toUtf8().constData(),
        secret.isEmpty() ? nullptr : secret.constData(), nullptr, nullptr, &key);
    if (imported != SSH_OK || !key)
        return passphraseGiven ? KeyResult::Unusable : KeyResult::NeedsPassphrase;
    const int rc = ssh_userauth_publickey(m_ssh, nullptr, key);
    ssh_key_free(key);
    return rc == SSH_AUTH_SUCCESS ? KeyResult::Success : KeyResult::Rejected;
}

/** @brief After the passphrase prompt: one attempt with the key file, then the rest. Claude Generated 2026 */
void SftpClient::authenticateWithPassphrase(const QString& passphrase)
{
    if (!m_ssh)
        return;
    if (!passphrase.isEmpty() && tryExplicitKey(passphrase, true) == KeyResult::Success) {
        openSftp();
        return;
    }
    continueAuthentication();
}

/** @brief The ssh agent and the standard keys, then the password prompt. Claude Generated 2026 */
void SftpClient::continueAuthentication()
{
    const QString account = m_connection.account();
    int rc = SSH_AUTH_DENIED;
    if (m_authMethods & SSH_AUTH_METHOD_PUBLICKEY) {
        rc = ssh_userauth_publickey_auto(m_ssh, nullptr, nullptr);
        if (rc == SSH_AUTH_SUCCESS) {
            openSftp();
            return;
        }
        if (rc == SSH_AUTH_ERROR) {
            fail(tr("Authentication with %1 failed: %2").arg(account, sshError()));
            return;
        }
        if (rc == SSH_AUTH_PARTIAL)
            m_authMethods = ssh_userauth_list(m_ssh, nullptr);
    }
    if (m_cancel) {
        closeSession();
        return;
    }
    if (m_authMethods & (SSH_AUTH_METHOD_PASSWORD | SSH_AUTH_METHOD_INTERACTIVE)) {
        emit passwordRequired(account);
        return;
    }
    fail(tr("Authentication with %1 failed: the server accepted none of the keys from the ssh agent or "
            "~/.ssh and offers no password login.").arg(account));
}

/** @brief Answers every hidden prompt of a keyboard-interactive login with the password. Claude Generated 2026 */
int SftpClient::authenticateKeyboardInteractive(const QByteArray& password)
{
    int rc = ssh_userauth_kbdint(m_ssh, nullptr, nullptr);
    for (int round = 0; rc == SSH_AUTH_INFO && round < 5; ++round) {
        const int prompts = ssh_userauth_kbdint_getnprompts(m_ssh);
        for (int i = 0; i < prompts; ++i) {
            char echo = 0;
            ssh_userauth_kbdint_getprompt(m_ssh, static_cast<unsigned int>(i), &echo);
            if (echo)
                return SSH_AUTH_DENIED;  // a visible question is not a password prompt
            if (ssh_userauth_kbdint_setanswer(m_ssh, static_cast<unsigned int>(i), password.constData()) < 0)
                return SSH_AUTH_ERROR;
        }
        rc = ssh_userauth_kbdint(m_ssh, nullptr, nullptr);
    }
    return rc;
}

/** @brief One password attempt (password method, then keyboard-interactive). Claude Generated 2026 */
void SftpClient::authenticateWithPassword(const QString& password)
{
    if (!m_ssh)
        return;
    const QString account = m_connection.account();
    const QByteArray secret = password.toUtf8();
    int rc = SSH_AUTH_DENIED;
    if (m_authMethods & SSH_AUTH_METHOD_PASSWORD)
        rc = ssh_userauth_password(m_ssh, nullptr, secret.constData());
    if (rc != SSH_AUTH_SUCCESS && rc != SSH_AUTH_ERROR && (m_authMethods & SSH_AUTH_METHOD_INTERACTIVE))
        rc = authenticateKeyboardInteractive(secret);

    if (rc == SSH_AUTH_SUCCESS)
        openSftp();
    else if (rc == SSH_AUTH_PARTIAL)
        fail(tr("%1 asks for a further login step (for example a one-time code); the remote browser "
                "does not support that.").arg(account));
    else if (rc == SSH_AUTH_ERROR)
        fail(tr("Authentication with %1 failed: %2").arg(account, sshError()));
    else
        fail(tr("Authentication failed for %1: the password was not accepted.").arg(account));
}

/** @brief Starts the SFTP subsystem and reports the remote home directory. Claude Generated 2026 */
void SftpClient::openSftp()
{
    const QString host = m_connection.host;
    m_sftp = sftp_new(m_ssh);
    if (!m_sftp) {
        fail(tr("Could not open an SFTP channel on %1: %2").arg(host, sshError()));
        return;
    }
    if (sftp_init(m_sftp) != SSH_OK) {
        fail(tr("%1 accepted the login but offers no SFTP service (%2).").arg(host, sshError()));
        return;
    }
    QString home = QStringLiteral("/");
    if (char* canonical = sftp_canonicalize_path(m_sftp, ".")) {
        home = QString::fromUtf8(canonical);
        ssh_string_free_char(canonical);
    }
    emit connected(remotepaths::normalise(home));
}

/** @brief Error text for a failed SFTP request on @p path. Claude Generated 2026 */
QString SftpClient::sftpErrorText(const QString& path) const
{
    if (!m_ssh || !ssh_is_connected(m_ssh))
        return tr("The connection to %1 was lost.").arg(m_connection.host);
    switch (sftp_get_error(m_sftp)) {
    case SSH_FX_PERMISSION_DENIED:
        return tr("Permission denied: %1").arg(path);
    case SSH_FX_NO_SUCH_FILE:
    case SSH_FX_NO_SUCH_PATH:
        return tr("No such file or directory: %1").arg(path);
    default:
        return tr("SFTP request on %1 failed: %2").arg(path, sshError());
    }
}

/** @brief Lists one directory; links to directories count as directories. Claude Generated 2026 */
void SftpClient::list(int requestId, const QString& path)
{
    const QString dirPath = remotepaths::normalise(path);
    if (!m_sftp) {
        emit listFailed(requestId, dirPath, tr("Not connected."));
        return;
    }
    sftp_dir dir = sftp_opendir(m_sftp, dirPath.toUtf8().constData());
    if (!dir) {
        emit listFailed(requestId, dirPath, sftpErrorText(dirPath));
        return;
    }

    QList<RemoteEntry> entries;
    while (sftp_attributes attributes = sftp_readdir(m_sftp, dir)) {
        RemoteEntry entry;
        entry.name = QString::fromUtf8(attributes->name);
        entry.isDir = attributes->type == SSH_FILEXFER_TYPE_DIRECTORY;
        entry.isLink = attributes->type == SSH_FILEXFER_TYPE_SYMLINK;
        entry.size = static_cast<qint64>(attributes->size);
        entry.modified = QDateTime::fromSecsSinceEpoch(attributes->mtime);
        sftp_attributes_free(attributes);
        if (m_cancel)
            break;
        if (entry.name == QLatin1String(".") || entry.name == QLatin1String(".."))
            continue;
        if (entry.isLink) {
            const QByteArray target = remotepaths::join(dirPath, entry.name).toUtf8();
            if (sftp_attributes resolved = sftp_stat(m_sftp, target.constData())) {
                entry.isDir = resolved->type == SSH_FILEXFER_TYPE_DIRECTORY;
                entry.size = static_cast<qint64>(resolved->size);
                sftp_attributes_free(resolved);
            }
        }
        entries.append(entry);
    }
    const bool complete = sftp_dir_eof(dir) == 1;
    sftp_closedir(dir);
    if (m_cancel)
        return;
    if (!complete) {
        emit listFailed(requestId, dirPath, tr("Listing %1 stopped: %2").arg(dirPath, sftpErrorText(dirPath)));
        return;
    }
    remotepaths::sortEntries(entries);
    emit listed(requestId, dirPath, entries);
}

/** @brief Copies a remote file in 64 KiB chunks; the target appears only when complete. Claude Generated 2026 */
void SftpClient::download(const QString& remotePath, const QString& localPath, qint64 expectedSize)
{
    if (!m_sftp) {
        emit downloadFailed(remotePath, tr("Not connected."));
        return;
    }
    sftp_file file = sftp_open(m_sftp, remotePath.toUtf8().constData(), O_RDONLY, 0);
    if (!file) {
        emit downloadFailed(remotePath, sftpErrorText(remotePath));
        return;
    }
    qint64 total = expectedSize;
    if (sftp_attributes attributes = sftp_fstat(file)) {
        total = static_cast<qint64>(attributes->size);
        sftp_attributes_free(attributes);
    }

    const QString partPath = localPath + QStringLiteral(".part");
    QFile out(partPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        sftp_close(file);
        emit downloadFailed(remotePath, tr("Cannot write %1: %2").arg(partPath, out.errorString()));
        return;
    }

    QByteArray buffer(64 * 1024, Qt::Uninitialized);
    qint64 received = 0;
    ssize_t n = 0;
    bool writeOk = true;
    QElapsedTimer sinceProgress;
    sinceProgress.start();
    while (!m_cancel && (n = sftp_read(file, buffer.data(), static_cast<size_t>(buffer.size()))) > 0) {
        if (out.write(buffer.constData(), n) != n) {
            writeOk = false;
            break;
        }
        received += n;
        if (sinceProgress.elapsed() > 100) {
            emit downloadProgress(remotePath, received, total);
            sinceProgress.restart();
        }
    }
    const QString readError = n < 0 ? sftpErrorText(remotePath) : QString();
    sftp_close(file);
    out.close();

    if (m_cancel || n < 0 || !writeOk) {
        QFile::remove(partPath);
        if (m_cancel)
            return;
        emit downloadFailed(remotePath, writeOk ? readError
                                                : tr("Writing %1 failed: %2").arg(partPath, out.errorString()));
        return;
    }
    QFile::remove(localPath);
    if (!QFile::rename(partPath, localPath)) {
        QFile::remove(partPath);
        emit downloadFailed(remotePath, tr("Cannot move the download to %1.").arg(localPath));
        return;
    }
    emit downloadProgress(remotePath, received, total);
    emit downloaded(remotePath, localPath);
}

/** @brief Creates one remote directory after checking that the name is free. Claude Generated 2026 */
void SftpClient::makeDirectory(const QString& path)
{
    const QString dirPath = remotepaths::normalise(path);
    if (!m_sftp) {
        emit makeDirectoryFailed(dirPath, tr("Not connected."));
        return;
    }
    const QByteArray encoded = dirPath.toUtf8();
    if (sftp_attributes existing = sftp_lstat(m_sftp, encoded.constData())) {
        sftp_attributes_free(existing);
        emit makeDirectoryFailed(dirPath, tr("%1 already exists.").arg(dirPath));
        return;
    }
    if (sftp_mkdir(m_sftp, encoded.constData(), 0755) != SSH_OK) {
        emit makeDirectoryFailed(dirPath, sftpErrorText(dirPath));
        return;
    }
    emit directoryCreated(dirPath);
}

void SftpClient::disconnectFromHost()
{
    closeSession();
}
