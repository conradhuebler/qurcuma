// sftpcache.h - SFTP File Cache Management
// Copyright (C) 2025 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Phase SFTP Integration

#pragma once

#include <QString>
#include <QDateTime>
#include <QMap>
#include <QCryptographicHash>

/**
 * Manages local file cache for SFTP downloaded files
 *
 * Provides intelligent caching to avoid re-downloading files:
 * - Hash-based filenames for uniqueness (host + remote path)
 * - Timestamp tracking for cache validation
 * - Size-based cache cleanup
 * - Thread-safe operations
 *
 * Usage:
 * @code
 * SftpCache cache;
 * QString remotePath = "/home/user/calc/structure.xyz";
 * QString host = "hpc.cluster.edu";
 *
 * if (cache.isCached(host, remotePath)) {
 *     QString localPath = cache.getCachedPath(host, remotePath);
 *     // Use cached file
 * } else {
 *     // Download and cache
 *     QString localPath = cache.prepareCachePath(host, remotePath);
 *     // ... download to localPath ...
 *     cache.markCached(host, remotePath, localPath);
 * }
 * @endcode
 */
class SftpCache {
public:
    SftpCache();
    ~SftpCache();

    /**
     * Check if a remote file is already cached
     * @param host Remote hostname
     * @param remotePath Full path on remote server
     * @return true if file exists in cache
     */
    bool isCached(const QString& host, const QString& remotePath) const;

    /**
     * Get local path for a cached remote file
     * @param host Remote hostname
     * @param remotePath Full path on remote server
     * @return Local file path if cached, empty string otherwise
     */
    QString getCachedPath(const QString& host, const QString& remotePath) const;

    /**
     * Prepare cache path for a remote file (creates directory if needed)
     * @param host Remote hostname
     * @param remotePath Full path on remote server
     * @param preserveExtension If true, keeps original file extension
     * @return Local file path where file should be stored
     */
    QString prepareCachePath(const QString& host, const QString& remotePath, bool preserveExtension = true);

    /**
     * Mark a file as cached after successful download
     * @param host Remote hostname
     * @param remotePath Full path on remote server
     * @param localPath Local file path where file was downloaded
     */
    void markCached(const QString& host, const QString& remotePath, const QString& localPath);

    /**
     * Clear all cached files
     * @return Number of files deleted
     */
    int clearCache();

    /**
     * Get total size of cache directory in bytes
     * @return Cache size in bytes
     */
    qint64 getCacheSize() const;

    /**
     * Get cache directory path
     * @return Absolute path to cache directory
     */
    QString getCacheDirectory() const { return m_cacheDir; }

    /**
     * Set custom cache directory (default: /tmp/qurcuma_sftp/)
     * @param path New cache directory path
     */
    void setCacheDirectory(const QString& path);

    /**
     * Remove old cached files beyond a size limit
     * @param maxBytes Maximum cache size in bytes
     * @return Number of files removed
     */
    int cleanupBySize(qint64 maxBytes);

    /**
     * Remove cached files older than specified days
     * @param days Age threshold in days
     * @return Number of files removed
     */
    int cleanupByAge(int days);

private:
    /**
     * Generate unique hash for host + remote path combination
     * @param host Remote hostname
     * @param remotePath Full path on remote server
     * @return SHA-256 hash string
     */
    QString generateCacheKey(const QString& host, const QString& remotePath) const;

    /**
     * Get file extension from path
     * @param path File path
     * @return Extension including dot (e.g., ".xyz"), empty if none
     */
    QString getExtension(const QString& path) const;

    /**
     * Ensure cache directory exists
     */
    void ensureCacheDirectory();

    QString m_cacheDir;
    QMap<QString, QString> m_cacheIndex;  // cacheKey -> localPath
    QMap<QString, QDateTime> m_timestamps; // cacheKey -> download timestamp
};
