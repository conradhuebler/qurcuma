// remotefiles.h - Browse and download files on the machine that runs qurcuma-server
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 (WP remote compute R4, docs/WP-remote-compute-vr.md).
// A connection of its own (ssh tunnel to its own qurcuma-server process, or a direct URL),
// separate from a simulation run: the compute server serves one session, so a file browser
// that shared it would block runs. Only files below the directories the server shares
// (its session root and --browse directories) can be listed or fetched; downloads are
// streamed in chunks and verified with SHA-256.

#pragma once

#include <QElapsedTimer>
#include <QFile>
#include <QCryptographicHash>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVector>

class QWebSocket;

namespace remote {

class SshTunnel;

class RemoteFiles : public QObject {
    Q_OBJECT
public:
    struct Entry {
        QString name;
        bool isDir = false;
        qint64 size = 0;
        qint64 mtime = 0;  ///< seconds since the epoch
    };

    RemoteFiles(const QString& host, const QString& serverCommand, QObject* parent = nullptr);
    RemoteFiles(const QUrl& url, const QString& token, QObject* parent = nullptr);
    ~RemoteFiles() override;

    void connectToHost();
    bool isConnected() const { return m_welcomed && !m_closed; }

    /// Asks for a directory listing; "" = the first shared directory.
    void list(const QString& remotePath);
    /// Downloads one file to @p localPath (written as <localPath>.part, renamed when the
    /// checksum matches). One download at a time. Returns the transfer id.
    quint32 download(const QString& remotePath, const QString& localPath);
    void cancelDownload();

    QString host() const { return m_host; }

signals:
    void logMessage(const QString& text);  ///< diagnostic text (also in remote::logFilePath())
    void connected(const QStringList& roots);
    /// @p path is the canonical path of the listed directory.
    void listing(const QString& path, const QVector<RemoteFiles::Entry>& entries, bool truncated);
    void listingFailed(const QString& request, const QString& message);
    void downloadProgress(quint32 id, qint64 received, qint64 total);
    void downloadFinished(quint32 id, const QString& localPath);
    void downloadFailed(quint32 id, const QString& message);
    void failed(const QString& message);  ///< connection could not be made or was lost

private:
    void connectSocket();
    void onText(const QString& text);
    void onBinary(const QByteArray& message);
    void onSocketError();
    void abortDownload(const QString& message);
    void sendJson(const QJsonObject& obj);
    void log(const QString& text);

    QString m_host, m_serverCommand;
    QUrl m_url;
    QString m_token;
    SshTunnel* m_tunnel = nullptr;
    QWebSocket* m_socket = nullptr;
    QTimer m_retry;
    QElapsedTimer m_connectTimer;
    bool m_welcomed = false;
    bool m_closed = false;
    bool m_connectedEmitted = false;

    quint32 m_nextId = 1;
    quint32 m_activeId = 0;
    QString m_localPath;
    QFile m_out;
    QCryptographicHash m_hash{ QCryptographicHash::Sha256 };
    qint64 m_expected = 0, m_received = 0;
};

} // namespace remote

Q_DECLARE_METATYPE(remote::RemoteFiles::Entry)
