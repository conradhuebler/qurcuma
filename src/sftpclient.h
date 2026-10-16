// sftpclient.h - libssh SFTP session that runs on its own thread (remote file browser)
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026
//
// Compiled only with USE_SFTP. One SftpClient per connection: the owner (RemoteBrowserPanel) moves
// it to a fresh QThread and calls its slots through queued invocations, so connecting, listing and
// downloading never block the GUI thread. Every result comes back as a signal. Connecting is a short
// dialogue: connectTo() may stop at hostKeyUnknown() (answer with answerHostKey()) and at
// passwordRequired() (answer with authenticateWithPassword()) before connected() or connectFailed().
// The password is used for that one attempt and not kept.

#pragma once

#include "remotepaths.h"

#include <QObject>

#include <atomic>

// libssh handle types (the same typedefs as in libssh/libssh.h and libssh/sftp.h), so that this
// header does not pull libssh into every file that includes it.
typedef struct ssh_session_struct* ssh_session;
typedef struct sftp_session_struct* sftp_session;

class SftpClient : public QObject {
    Q_OBJECT

public:
    explicit SftpClient(QObject* parent = nullptr);
    ~SftpClient() override;

    /// Thread-safe: stops a running listing or download at the next chunk; their results are dropped.
    void requestCancel();

public slots:
    /// Opens the ssh session, checks the host key and tries the ssh agent and the standard keys.
    void connectTo(const RemoteConnection& connection);
    /// Answer to hostKeyUnknown(): true stores the key in ~/.ssh/known_hosts and continues.
    void answerHostKey(bool trust);
    /// Answer to passwordRequired(): one password (or keyboard-interactive) attempt.
    void authenticateWithPassword(const QString& password);
    /// Lists @p path; emits listed() or listFailed() with the caller's @p requestId (the panel has
    /// several lists and the directory chooser asking the same session).
    void list(int requestId, const QString& path);
    /// Creates the directory @p path (mode 0755, the server's umask applies); emits
    /// directoryCreated() or makeDirectoryFailed(). An existing entry of that name is refused.
    void makeDirectory(const QString& path);
    /// Downloads @p remotePath to @p localPath (via "<localPath>.part"); @p expectedSize is used for
    /// the progress when the server does not report a size.
    void download(const QString& remotePath, const QString& localPath, qint64 expectedSize);
    /// Closes the session; safe to call in any state.
    void disconnectFromHost();

signals:
    void hostKeyUnknown(const QString& host, const QString& fingerprint);
    void passwordRequired(const QString& account);
    void connected(const QString& homePath);
    void connectFailed(const QString& message);
    void listed(int requestId, const QString& path, const QList<RemoteEntry>& entries);
    void listFailed(int requestId, const QString& path, const QString& message);
    void directoryCreated(const QString& path);
    void makeDirectoryFailed(const QString& path, const QString& message);
    void downloadProgress(const QString& remotePath, qint64 received, qint64 total);
    void downloaded(const QString& remotePath, const QString& localPath);
    void downloadFailed(const QString& remotePath, const QString& message);

private:
    void authenticateWithKeys();
    int authenticateKeyboardInteractive(const QByteArray& password);
    void openSftp();
    void fail(const QString& message);
    void closeSession();
    QString sshError() const;
    QString serverFingerprint() const;
    QString sftpErrorText(const QString& path) const;

    ssh_session m_ssh = nullptr;
    sftp_session m_sftp = nullptr;
    RemoteConnection m_connection;
    int m_authMethods = 0;
    std::atomic_bool m_cancel { false };
};
