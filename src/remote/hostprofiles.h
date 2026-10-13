// hostprofiles.h - Computers added in qurcuma itself (no ~/.ssh/config entry needed)
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute): a profile is what ssh needs to reach a computer
// that is not in ~/.ssh/config: address, port, key file. SshTunnel looks a host name up here
// first, so the dock, the Remote Files dialog and the tests keep passing a plain name.
// Profiles live in the qurcuma settings (key `remote/hosts`, a JSON array); ~/.ssh/config is
// never written.

#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace remote {

struct HostProfile {
    QString name;           ///< shown in the lists; also the key of the other saved settings
    QString target;         ///< [user@]host as ssh takes it
    int port = 0;           ///< 0 = ssh default
    QString identityFile;   ///< empty = agent and ssh defaults
    QString serverCommand;  ///< empty = "qurcuma-server"

    /// Names and addresses are checked because they end up as ssh arguments: a value that
    /// starts with '-' would be read by ssh as an option (for example ProxyCommand).
    bool isValid(QString* why = nullptr) const;
    /// Options placed before the target: `-p <port>`, `-i <key>`.
    QStringList sshArgs() const;
    QString effectiveServerCommand() const { return serverCommand.trimmed().isEmpty() ? QStringLiteral("qurcuma-server") : serverCommand.trimmed(); }

    QJsonObject toJson() const;
    static HostProfile fromJson(const QJsonObject& obj);
};

class HostProfiles {
public:
    /// Profiles saved in the settings; invalid entries are skipped.
    static QList<HostProfile> load();
    static void save(const QList<HostProfile>& profiles);
    static std::optional<HostProfile> find(const QString& name);
    /// Adds @p profile; false (with @p why) if it is invalid or the name is taken.
    static bool add(const HostProfile& profile, QString* why = nullptr);
    static bool remove(const QString& name);
};

} // namespace remote
