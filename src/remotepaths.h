// remotepaths.h - Path, listing and connection helpers of the remote file browser (Qt core only)
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026
//
// Everything here is free of libssh and widgets, so it is compiled in every build and tested by
// tests/test_remotepaths.cpp. Remote paths are POSIX paths ('/' separated); the breadcrumb split is
// also used by BreadcrumbBar for local paths.

#pragma once

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QSet>
#include <QString>
#include <QStringList>

struct SshConfigEntry;
namespace remote {
struct HostProfile;
}

/// One entry of a remote directory listing.
struct RemoteEntry {
    QString name;          ///< file name without directory
    bool isDir = false;    ///< directory, or a symbolic link that points to one
    bool isLink = false;   ///< symbolic link
    qint64 size = 0;       ///< bytes (files)
    QDateTime modified;    ///< last modification on the remote side
};
Q_DECLARE_METATYPE(RemoteEntry)

/// What libssh needs to open a session, plus the source it came from.
struct RemoteConnection {
    QString sourceId;      ///< bookmark source id: an SFTP profile id or "computer:<name>"; empty for an unsaved host
    QString label;         ///< shown in the panel
    QString host;          ///< host name or address handed to libssh
    QString user;          ///< empty = libssh default (the local user name)
    int port = 22;
    QString proxyCommand;  ///< from a matching ~/.ssh/config entry (ProxyJump/ProxyCommand)
    QString identityFile;  ///< private key file to try first (a computer's key); empty = ssh agent and standard keys only

    bool isValid() const { return !host.isEmpty(); }
    /// "user@host", or "host" without a user.
    QString account() const;
    /// "user@host:port", the host key of the download cache.
    QString cacheKey() const;
};

/// One clickable part of a breadcrumb path.
struct PathSegment {
    QString label;   ///< "Home", "Root" or a directory name
    QString path;    ///< full path up to and including this part
};

namespace remotepaths {

/// Folders first, then files; names compared case-insensitively; equal names keep their order.
void sortEntries(QList<RemoteEntry>& entries);

/// Collapses repeated slashes, resolves "." and "..", drops a trailing slash ("/" stays "/").
/// ".." above the root stays at the root. An empty path becomes ".".
QString normalise(const QString& path);
/// @p name appended to @p dir; an absolute @p name replaces @p dir.
QString join(const QString& dir, const QString& name);
/// Directory that contains @p path; the parent of "/" is "/".
QString parent(const QString& path);
/// @p path relative to @p home as "~" or "~/sub/dir"; paths outside @p home are returned normalised.
QString displayPath(const QString& path, const QString& home);
/// Clickable parts of @p path: "Home" (= @p home) when the path lies inside @p home, otherwise
/// "Root" (= "/") for absolute paths, or the first component (a drive such as "C:") for others.
QList<PathSegment> breadcrumbSegments(const QString& path, const QString& home);

/// Extension of a file name as the file lists use it: the part after the last '.', lower case;
/// empty when the name has no '.' (the "<no extension>" entry of the Extensions menu).
QString entrySuffix(const QString& fileName);
/// Extensions of the files (not folders) in @p entries, as entrySuffix() gives them.
QSet<QString> collectSuffixes(const QList<RemoteEntry>& entries);
/// File filter shared by the local and the remote file list: with an extension selection
/// (@p hasExtensionSelection) only suffixes in @p patterns pass; a non-empty @p namePattern must
/// occur in the name (case-insensitive, surrounding blanks ignored).
bool fileFilterAccepts(const QString& fileName, const QString& namePattern, bool hasExtensionSelection,
                       const QStringList& patterns);

/// Key of a connection for per-source settings: its bookmark source id, or "host:user@host:port"
/// for a host that was not saved.
QString sourceKey(const RemoteConnection& connection);
/// QSettings key of the remote working directory of a source ("remote/workdir/<encoded key>"; the
/// key is percent-encoded so that '/', ':' or braces in it do not split the settings path).
QString workDirSettingsKey(const QString& sourceKey);
/// A name for "+ New Directory": not empty, not "." or "..", no '/' and no NUL character.
bool isValidNewDirectoryName(const QString& name, QString* why = nullptr);
/// "<base>_1", "<base>_2", ... : the first one not in @p existing (as the local New Calculation
/// Directory suggests a name).
QString nextFreeName(const QString& base, const QStringList& existing);

/// Bookmark source id of a computer of the list ("computer:<name>").
QString computerSourceId(const QString& computerName);
bool isComputerSourceId(const QString& sourceId);
/// Computer name of a "computer:<name>" id; empty for any other id.
QString computerNameFromSourceId(const QString& sourceId);

/// Connection for a computer of the list: address, user and port of the computer; when its host is
/// an alias of ~/.ssh/config (@p alias, may be null) the alias supplies HostName, User, Port and
/// ProxyJump first and the computer's own user and port come on top. The computer's key file is carried
/// along (an alias' IdentityFile when the computer has none).
RemoteConnection connectionForComputer(const remote::HostProfile& computer, const SshConfigEntry* alias);
/// Connection for an SFTP profile or a host typed in: @p port 0 means "not given" (alias port or 22),
/// an empty @p user means "not given" (alias user or the libssh default).
RemoteConnection connectionForHost(const QString& sourceId, const QString& label, const QString& host,
                                   const QString& user, int port, const SshConfigEntry* alias,
                                   const QString& identityFile = QString());

/// The private key file to load for a configured key path: `~/` is expanded, and a `.pub` path is replaced
/// by the private key next to it when that exists (as `ssh -i` does). The result is not checked for existence.
QString privateKeyPath(const QString& configured);

} // namespace remotepaths
