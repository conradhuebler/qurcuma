// sftpcache.cpp - SFTP File Cache Management Implementation
// Copyright (C) 2025 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Phase SFTP Integration

#include "sftpcache.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>
#include <QDirIterator>

SftpCache::SftpCache()
{
    // Default cache directory
    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/qurcuma_sftp";
    ensureCacheDirectory();
}

SftpCache::~SftpCache()
{
    // Optional: Auto-cleanup on exit (could be made configurable)
    // clearCache();
}

void SftpCache::ensureCacheDirectory()
{
    QDir dir;
    if (!dir.exists(m_cacheDir)) {
        dir.mkpath(m_cacheDir);
        qDebug() << "Created SFTP cache directory:" << m_cacheDir;
    }
}

QString SftpCache::generateCacheKey(const QString& host, const QString& remotePath) const
{
    // Generate unique hash from host + path combination
    QString combined = host + ":" + remotePath;
    QByteArray hash = QCryptographicHash::hash(combined.toUtf8(), QCryptographicHash::Sha256);
    return QString(hash.toHex());
}

QString SftpCache::getExtension(const QString& path) const
{
    QFileInfo info(path);
    QString ext = info.suffix();
    return ext.isEmpty() ? QString() : ("." + ext);
}

bool SftpCache::isCached(const QString& host, const QString& remotePath) const
{
    QString cacheKey = generateCacheKey(host, remotePath);

    // Check if we have it in our index
    if (m_cacheIndex.contains(cacheKey)) {
        QString localPath = m_cacheIndex[cacheKey];
        // Verify file still exists
        if (QFile::exists(localPath)) {
            return true;
        }
    }

    // Fallback: Check if file exists based on expected path
    QString expectedPath = m_cacheDir + "/" + cacheKey + getExtension(remotePath);
    return QFile::exists(expectedPath);
}

QString SftpCache::getCachedPath(const QString& host, const QString& remotePath) const
{
    QString cacheKey = generateCacheKey(host, remotePath);

    // Check index first
    if (m_cacheIndex.contains(cacheKey)) {
        QString localPath = m_cacheIndex[cacheKey];
        if (QFile::exists(localPath)) {
            return localPath;
        }
    }

    // Fallback: Try to find based on expected path
    QString expectedPath = m_cacheDir + "/" + cacheKey + getExtension(remotePath);
    if (QFile::exists(expectedPath)) {
        return expectedPath;
    }

    return QString();  // Not cached
}

QString SftpCache::prepareCachePath(const QString& host, const QString& remotePath, bool preserveExtension)
{
    ensureCacheDirectory();

    QString cacheKey = generateCacheKey(host, remotePath);
    QString extension = preserveExtension ? getExtension(remotePath) : QString();
    QString localPath = m_cacheDir + "/" + cacheKey + extension;

    qDebug() << "Prepared cache path:" << localPath << "for" << host << ":" << remotePath;

    return localPath;
}

void SftpCache::markCached(const QString& host, const QString& remotePath, const QString& localPath)
{
    QString cacheKey = generateCacheKey(host, remotePath);
    m_cacheIndex[cacheKey] = localPath;
    m_timestamps[cacheKey] = QDateTime::currentDateTime();

    qDebug() << "Marked as cached:" << cacheKey << "->" << localPath;
}

int SftpCache::clearCache()
{
    QDir dir(m_cacheDir);
    if (!dir.exists()) {
        return 0;
    }

    int count = 0;
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& fileInfo : files) {
        if (QFile::remove(fileInfo.absoluteFilePath())) {
            count++;
        }
    }

    // Clear index
    m_cacheIndex.clear();
    m_timestamps.clear();

    qDebug() << "Cleared SFTP cache:" << count << "files removed";
    return count;
}

qint64 SftpCache::getCacheSize() const
{
    QDir dir(m_cacheDir);
    if (!dir.exists()) {
        return 0;
    }

    qint64 totalSize = 0;
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& fileInfo : files) {
        totalSize += fileInfo.size();
    }

    return totalSize;
}

void SftpCache::setCacheDirectory(const QString& path)
{
    m_cacheDir = path;
    ensureCacheDirectory();
}

int SftpCache::cleanupBySize(qint64 maxBytes)
{
    qint64 currentSize = getCacheSize();
    if (currentSize <= maxBytes) {
        return 0;  // Within limit
    }

    // Get all files with timestamps
    QDir dir(m_cacheDir);
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Time | QDir::Reversed);

    int removed = 0;
    qint64 freedSize = 0;

    // Remove oldest files until under limit
    for (const QFileInfo& fileInfo : files) {
        if (currentSize - freedSize <= maxBytes) {
            break;
        }

        qint64 fileSize = fileInfo.size();
        if (QFile::remove(fileInfo.absoluteFilePath())) {
            freedSize += fileSize;
            removed++;

            // Remove from index if present
            QString fileName = fileInfo.fileName();
            // Try to find in index by value
            for (auto it = m_cacheIndex.begin(); it != m_cacheIndex.end(); ++it) {
                if (it.value().endsWith(fileName)) {
                    QString key = it.key();
                    m_cacheIndex.erase(it);
                    m_timestamps.remove(key);
                    break;
                }
            }
        }
    }

    qDebug() << "Cleaned up by size:" << removed << "files removed, freed" << freedSize << "bytes";
    return removed;
}

int SftpCache::cleanupByAge(int days)
{
    QDateTime threshold = QDateTime::currentDateTime().addDays(-days);
    QDir dir(m_cacheDir);
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);

    int removed = 0;
    for (const QFileInfo& fileInfo : files) {
        if (fileInfo.lastModified() < threshold) {
            if (QFile::remove(fileInfo.absoluteFilePath())) {
                removed++;

                // Remove from index
                QString fileName = fileInfo.fileName();
                for (auto it = m_cacheIndex.begin(); it != m_cacheIndex.end(); ++it) {
                    if (it.value().endsWith(fileName)) {
                        QString key = it.key();
                        m_cacheIndex.erase(it);
                        m_timestamps.remove(key);
                        break;
                    }
                }
            }
        }
    }

    qDebug() << "Cleaned up by age:" << removed << "files older than" << days << "days removed";
    return removed;
}
