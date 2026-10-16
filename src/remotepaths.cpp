// remotepaths.cpp - Path, listing and connection helpers of the remote file browser
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "remotepaths.h"

#include "remote/hostprofiles.h"
#include "sshconfig.h"

#include <QCoreApplication>
#include <QStringList>
#include <QUrl>

#include <algorithm>

QString RemoteConnection::account() const
{
    return user.isEmpty() ? host : user + QLatin1Char('@') + host;
}

QString RemoteConnection::cacheKey() const
{
    return account() + QLatin1Char(':') + QString::number(port);
}

namespace remotepaths {

namespace {
const QString kComputerPrefix = QStringLiteral("computer:");

/// True when @p path is @p dir itself or lies below it (both normalised, dir not "/").
bool isInside(const QString& path, const QString& dir)
{
    return path == dir || path.startsWith(dir + QLatin1Char('/'));
}

QString stripTrailingSlashes(QString path)
{
    while (path.size() > 1 && path.endsWith(QLatin1Char('/')))
        path.chop(1);
    return path;
}
} // namespace

/** @brief Stable sort: folders first, then case-insensitive name order. Claude Generated 2026 */
void sortEntries(QList<RemoteEntry>& entries)
{
    std::stable_sort(entries.begin(), entries.end(), [](const RemoteEntry& a, const RemoteEntry& b) {
        if (a.isDir != b.isDir)
            return a.isDir;
        return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
    });
}

/** @brief Lexical normalisation of a POSIX path (no file system access). Claude Generated 2026 */
QString normalise(const QString& path)
{
    const bool absolute = path.startsWith(QLatin1Char('/'));
    QStringList parts;
    for (const QString& part : path.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        if (part == QLatin1String("."))
            continue;
        if (part == QLatin1String("..")) {
            if (!parts.isEmpty() && parts.last() != QLatin1String(".."))
                parts.removeLast();
            else if (!absolute)
                parts.append(part);  // a relative path may climb; the root may not
            continue;
        }
        parts.append(part);
    }
    const QString joined = parts.join(QLatin1Char('/'));
    if (absolute)
        return QLatin1Char('/') + joined;
    return joined.isEmpty() ? QStringLiteral(".") : joined;
}

/** @brief Appends a name to a directory path. Claude Generated 2026 */
QString join(const QString& dir, const QString& name)
{
    if (name.startsWith(QLatin1Char('/')) || dir.isEmpty())
        return normalise(name);
    return normalise(dir + QLatin1Char('/') + name);
}

/** @brief Parent directory of a POSIX path. Claude Generated 2026 */
QString parent(const QString& path)
{
    const QString p = normalise(path);
    if (p == QLatin1String("/"))
        return p;
    const int slash = p.lastIndexOf(QLatin1Char('/'));
    if (slash == 0)
        return QStringLiteral("/");
    if (slash < 0)
        return QStringLiteral(".");
    return p.left(slash);
}

/** @brief Home-relative display form ("~", "~/a/b"). Claude Generated 2026 */
QString displayPath(const QString& path, const QString& home)
{
    const QString p = normalise(path);
    if (home.isEmpty())
        return p;
    const QString h = normalise(home);
    if (h == QLatin1String("/") || !isInside(p, h))
        return p;
    return QLatin1Char('~') + p.mid(h.size());
}

/** @brief Splits a path into breadcrumb parts (shared with BreadcrumbBar). Claude Generated 2026 */
QList<PathSegment> breadcrumbSegments(const QString& rawPath, const QString& rawHome)
{
    QList<PathSegment> out;
    const QString path = stripTrailingSlashes(rawPath);
    if (path.isEmpty())
        return out;
    const QString home = stripTrailingSlashes(rawHome);

    QString base;
    QString rest;
    if (!home.isEmpty() && home != QLatin1String("/") && isInside(path, home)) {
        base = home;
        rest = path.mid(home.size());
        out.append({ QStringLiteral("Home"), base });
    } else if (path.startsWith(QLatin1Char('/'))) {
        base = QStringLiteral("/");
        rest = path.mid(1);
        out.append({ QStringLiteral("Root"), base });
    } else {
        // relative path or a drive ("C:/data"): its first component is its own root
        const int slash = path.indexOf(QLatin1Char('/'));
        base = slash < 0 ? path : path.left(slash);
        rest = slash < 0 ? QString() : path.mid(slash);
        out.append({ base, base });
    }
    for (const QString& part : rest.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        base = base.endsWith(QLatin1Char('/')) ? base + part : base + QLatin1Char('/') + part;
        out.append({ part, base });
    }
    return out;
}

/** @brief Lower-case extension after the last '.'. Claude Generated 2026 */
QString entrySuffix(const QString& fileName)
{
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    return dot < 0 ? QString() : fileName.mid(dot + 1).toLower();
}

/** @brief Extensions of the files of a listing. Claude Generated 2026 */
QSet<QString> collectSuffixes(const QList<RemoteEntry>& entries)
{
    QSet<QString> suffixes;
    for (const RemoteEntry& entry : entries)
        if (!entry.isDir)
            suffixes.insert(entrySuffix(entry.name));
    return suffixes;
}

/** @brief Search text plus extension subset, the rule of DirectoryFilterProxyModel. Claude Generated 2026 */
bool fileFilterAccepts(const QString& fileName, const QString& namePattern, bool hasExtensionSelection,
                       const QStringList& patterns)
{
    if (hasExtensionSelection && !patterns.contains(entrySuffix(fileName)))
        return false;
    const QString needle = namePattern.trimmed();
    return needle.isEmpty() || fileName.contains(needle, Qt::CaseInsensitive);
}

QString sourceKey(const RemoteConnection& connection)
{
    return connection.sourceId.isEmpty() ? QStringLiteral("host:") + connection.cacheKey() : connection.sourceId;
}

QString workDirSettingsKey(const QString& sourceKey)
{
    return QStringLiteral("remote/workdir/") + QString::fromLatin1(QUrl::toPercentEncoding(sourceKey));
}

/** @brief Checks a directory name typed for "+ New Directory". Claude Generated 2026 */
bool isValidNewDirectoryName(const QString& name, QString* why)
{
    QString reason;
    if (name.trimmed().isEmpty())
        reason = QCoreApplication::translate("remotepaths", "The name is empty.");
    else if (name == QLatin1String(".") || name == QLatin1String(".."))
        reason = QCoreApplication::translate("remotepaths", "\".\" and \"..\" are not allowed.");
    else if (name.contains(QLatin1Char('/')))
        reason = QCoreApplication::translate("remotepaths", "The name must not contain '/'.");
    else if (name.contains(QChar(0)))
        reason = QCoreApplication::translate("remotepaths", "The name contains a NUL character.");
    if (why)
        *why = reason;
    return reason.isEmpty();
}

QString nextFreeName(const QString& base, const QStringList& existing)
{
    for (int counter = 1;; ++counter) {
        const QString candidate = base + QLatin1Char('_') + QString::number(counter);
        if (!existing.contains(candidate))
            return candidate;
    }
}

QString computerSourceId(const QString& computerName)
{
    return kComputerPrefix + computerName;
}

bool isComputerSourceId(const QString& sourceId)
{
    return sourceId.startsWith(kComputerPrefix) && sourceId.size() > kComputerPrefix.size();
}

QString computerNameFromSourceId(const QString& sourceId)
{
    return isComputerSourceId(sourceId) ? sourceId.mid(kComputerPrefix.size()) : QString();
}

/** @brief Alias values first, explicit user/port of the source on top. Claude Generated 2026 */
RemoteConnection connectionForHost(const QString& sourceId, const QString& label, const QString& host,
                                   const QString& user, int port, const SshConfigEntry* alias)
{
    RemoteConnection c;
    c.sourceId = sourceId;
    c.label = label;
    c.host = host.trimmed();
    c.port = 22;
    if (alias && alias->isValid()) {
        if (!alias->hostName.isEmpty())
            c.host = alias->hostName;
        c.user = alias->user;
        if (alias->port > 0)
            c.port = alias->port;
        c.proxyCommand = alias->proxyCommand;
    }
    if (!user.trimmed().isEmpty())
        c.user = user.trimmed();
    if (port > 0)
        c.port = port;
    return c;
}

/** @brief Maps a computer of the list (address, user, port) to a connection. Claude Generated 2026 */
RemoteConnection connectionForComputer(const remote::HostProfile& computer, const SshConfigEntry* alias)
{
    return connectionForHost(computerSourceId(computer.name), computer.name, computer.hostPart(),
                             computer.userPart(), computer.port, alias);
}

} // namespace remotepaths
