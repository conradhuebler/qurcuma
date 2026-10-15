// sshconfig.cpp - SSH Config File Parser Implementation
// Copyright (C) 2025 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Phase SFTP Integration

#include "sshconfig.h"

#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

QString SshConfigParser::getDefaultConfigPath()
{
    return QDir::homePath() + "/.ssh/config";
}

bool SshConfigParser::configFileExists(const QString& configPath)
{
    QString path = configPath.isEmpty() ? getDefaultConfigPath() : configPath;
    QFileInfo fileInfo(path);
    return fileInfo.exists() && fileInfo.isReadable();
}

QString SshConfigParser::expandTilde(const QString& path)
{
    if (path.startsWith("~/")) {
        return QDir::homePath() + path.mid(1);
    }
    return path;
}

QList<SshConfigEntry> SshConfigParser::parseFile(const QString& configPath)
{
    QList<SshConfigEntry> entries;

    QString path = configPath.isEmpty() ? getDefaultConfigPath() : configPath;

    if (!configFileExists(path)) {
        qDebug() << "SSH config file not found or not readable:" << path;
        return entries;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open SSH config file:" << path;
        return entries;
    }

    QTextStream in(&file);
    SshConfigEntry currentEntry;
    QStringList aliases;  // the names on the current Host line (`Host a b` is one block with two names)
    bool inHostSection = false;

    // Saves the block that has just ended, once per name. Claude Generated 2026: the block used to
    // be saved after parseLine() had already replaced it with the new, empty one, so only the last
    // entry of a file was ever returned.
    auto saveBlock = [&]() {
        if (!inHostSection)
            return;
        for (const QString& alias : aliases) {
            SshConfigEntry entry = currentEntry;
            entry.host = alias;
            if (entry.hostName.isEmpty())
                entry.hostName = alias;  // ssh uses the name as the address when HostName is not set
            if (entry.isValid())
                entries.append(entry);
        }
    };

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        // Skip empty lines and comments
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }

        const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (!parts.isEmpty() && parts[0].compare("host", Qt::CaseInsensitive) == 0) {
            saveBlock();
            currentEntry = SshConfigEntry();
            aliases = parts.mid(1);
            inHostSection = true;
            continue;
        }
        parseLine(line, currentEntry);  // an option of the current block
    }
    saveBlock();

    file.close();

    qDebug() << "Parsed" << entries.size() << "SSH config entries from" << path;
    return entries;
}

SshConfigEntry SshConfigParser::findHost(const QString& host, const QString& configPath)
{
    QList<SshConfigEntry> entries = parseFile(configPath);

    for (const auto& entry : entries) {
        if (entry.host == host) {
            return entry;
        }
    }

    // Return invalid entry
    return SshConfigEntry();
}

bool SshConfigParser::parseLine(const QString& line, SshConfigEntry& currentEntry)
{
    // Split line into key and value (Qt6 compatible)
    QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    if (parts.isEmpty()) {
        return false;
    }

    QString key = parts[0].toLower();
    QString value = parts.size() > 1 ? parts.mid(1).join(" ") : QString();

    if (key == "host") {
        // New host section - save old entry and start new one
        if (!currentEntry.host.isEmpty()) {
            // Will be saved by caller
        }

        // Start new entry
        currentEntry = SshConfigEntry();
        currentEntry.host = value;

        // Don't set hostname yet - will default to host if not explicitly set
        return true;  // Indicate new host section
    }
    else if (key == "hostname") {
        currentEntry.hostName = value;
    }
    else if (key == "user") {
        currentEntry.user = value;
    }
    else if (key == "port") {
        bool ok;
        int port = value.toInt(&ok);
        if (ok && port > 0 && port <= 65535) {
            currentEntry.port = port;
        }
    }
    else if (key == "identityfile") {
        currentEntry.identityFile = expandTilde(value);
    }
    else if (key == "proxyjump") {
        // Claude Generated - ProxyJump Support
        currentEntry.proxyJump = value;
        // Convert to ProxyCommand format for libssh
        currentEntry.proxyCommand = convertProxyJumpToCommand(value);
    }
    else if (key == "proxycommand") {
        // Claude Generated - ProxyCommand Support
        currentEntry.proxyCommand = value;
    }
    else {
        // Store unrecognized options for potential future use
        currentEntry.otherOptions[key] = value;
    }

    return false;  // Not a new host section
}

QString SshConfigParser::convertProxyJumpToCommand(const QString& proxyJump)
{
    // Claude Generated - ProxyJump to ProxyCommand conversion for libssh
    if (proxyJump.isEmpty()) {
        return QString();
    }

    // Split multiple jumps by comma
    QStringList jumps = proxyJump.split(',', Qt::SkipEmptyParts);

    if (jumps.size() == 1) {
        // Single jump host: ssh -W %h:%p user@gateway.edu
        // %h and %p will be replaced by libssh with target host and port
        return QString("ssh -W %h:%p %1").arg(jumps[0].trimmed());
    } else {
        // Multiple jump hosts: ssh -W %h:%p -J jump1,jump2,jump3
        // The -J option chains multiple jumps
        return QString("ssh -W %h:%p -J %1").arg(proxyJump);
    }
}
