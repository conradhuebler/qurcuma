// sshconfig.h - SSH Config File Parser
// Copyright (C) 2025 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Phase SFTP Integration

#pragma once

#include <QString>
#include <QList>
#include <QMap>

/**
 * Structure representing a single Host entry from SSH config
 * Parses standard OpenSSH config options used for HPC cluster connections
 */
struct SshConfigEntry {
    QString host;          // Host alias (what user types)
    QString hostName;      // Actual hostname/IP (defaults to host if not specified)
    QString user;          // Username for authentication
    int port;              // SSH port (defaults to 22)
    QString identityFile;  // Path to private key file
    QString proxyJump;     // Claude Generated - ProxyJump directive (e.g., "gateway.edu" or "jump1,jump2")
    QString proxyCommand;  // Claude Generated - ProxyCommand for libssh (e.g., "ssh -W %h:%p gateway")
    QMap<QString, QString> otherOptions;  // Store unrecognized options for future use

    SshConfigEntry()
        : port(22)
    {
    }

    bool isValid() const
    {
        return !host.isEmpty() && !hostName.isEmpty();
    }
};

/**
 * Parser for OpenSSH config files (~/.ssh/config)
 *
 * Supports parsing:
 * - Host aliases and patterns
 * - HostName, Port, User, IdentityFile directives
 * - Comments (lines starting with #)
 * - Multi-line continuations (not implemented yet)
 *
 * Usage:
 * @code
 * SshConfigParser parser;
 * QList<SshConfigEntry> entries = parser.parseFile();
 * for (const auto& entry : entries) {
 *     if (entry.host == "hpc-cluster") {
 *         qDebug() << "Connect to" << entry.hostName << "as" << entry.user;
 *     }
 * }
 * @endcode
 */
class SshConfigParser {
public:
    /**
     * Parse SSH config file
     * @param configPath Path to config file (default: ~/.ssh/config)
     * @return List of parsed host entries
     */
    static QList<SshConfigEntry> parseFile(const QString& configPath = QString());

    /**
     * Find a specific host entry by alias
     * @param host Host alias to search for
     * @param configPath Path to config file (optional)
     * @return Host entry if found, invalid entry otherwise
     */
    static SshConfigEntry findHost(const QString& host, const QString& configPath = QString());

    /**
     * Get default SSH config file path
     * @return Path to ~/.ssh/config with proper expansion
     */
    static QString getDefaultConfigPath();

    /**
     * Check if SSH config file exists and is readable
     * @param configPath Path to config file (optional, uses default if empty)
     * @return true if file exists and can be read
     */
    static bool configFileExists(const QString& configPath = QString());

private:
    /**
     * Parse a single line of SSH config
     * @param line Line to parse
     * @param currentEntry Current host entry being built
     * @return true if this line starts a new Host section
     */
    static bool parseLine(const QString& line, SshConfigEntry& currentEntry);

    /**
     * Expand tilde (~) to home directory
     * @param path Path that may contain ~
     * @return Expanded path
     */
    static QString expandTilde(const QString& path);

    /**
     * Convert ProxyJump directive to ProxyCommand format for libssh
     * @param proxyJump ProxyJump value (e.g., "gateway.edu" or "jump1,jump2,jump3")
     * @return ProxyCommand string (e.g., "ssh -W %h:%p gateway.edu" or "ssh -W %h:%p -J jump1,jump2")
     *
     * Claude Generated - ProxyJump Support
     */
    static QString convertProxyJumpToCommand(const QString& proxyJump);
};
