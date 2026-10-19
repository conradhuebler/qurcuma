// hostprofiles.cpp - see hostprofiles.h
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute)

#include "hostprofiles.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSettings>

namespace remote {

namespace {
const QString kKey = QStringLiteral("remote/hosts");
}

bool HostProfile::isValid(QString* why) const
{
    auto fail = [why](const QString& text) {
        if (why)
            *why = text;
        return false;
    };
    static const QRegularExpression nameRe(QStringLiteral("^[A-Za-z0-9._][A-Za-z0-9._ -]{0,39}$"));
    if (!nameRe.match(name).hasMatch())
        return fail(QObject::tr("The name may use letters, digits, '.', '_', '-' and spaces (at most 40 characters) and must not start with '-'."));
    static const QRegularExpression targetRe(QStringLiteral("^[A-Za-z0-9_][A-Za-z0-9._@:%\\[\\]-]{0,254}$"));
    if (!targetRe.match(target).hasMatch())
        return fail(QObject::tr("The address must be a host name or IP address, optionally with 'user@' in front, without spaces and not starting with '-'."));
    if (port < 0 || port > 65535)
        return fail(QObject::tr("The port must be between 1 and 65535 (0 = ssh default)."));
    if (identityFile.startsWith(QLatin1Char('-')))
        return fail(QObject::tr("The key file path must not start with '-'."));
    return true;
}

QStringList HostProfile::sshArgs() const
{
    QStringList args;
    if (port > 0)
        args << QStringLiteral("-p") << QString::number(port);
    if (!identityFile.trimmed().isEmpty())
        args << QStringLiteral("-i") << identityFile.trimmed();
    return args;
}

QString HostProfile::userPart() const
{
    return target.contains(QLatin1Char('@')) ? target.section(QLatin1Char('@'), 0, 0) : QString();
}

QString HostProfile::hostPart() const
{
    return target.section(QLatin1Char('@'), -1);
}

QJsonObject HostProfile::toJson() const
{
    return { { "name", name }, { "target", target }, { "port", port },
        { "identityFile", identityFile }, { "serverCommand", serverCommand } };
}

HostProfile HostProfile::fromJson(const QJsonObject& obj)
{
    HostProfile p;
    p.name = obj.value("name").toString();
    p.target = obj.value("target").toString();
    p.port = obj.value("port").toInt();
    p.identityFile = obj.value("identityFile").toString();
    p.serverCommand = obj.value("serverCommand").toString();
    return p;
}

QList<HostProfile> HostProfiles::load()
{
    QList<HostProfile> out;
    const QJsonArray array = QJsonDocument::fromJson(QSettings().value(kKey).toString().toUtf8()).array();
    for (const QJsonValue& v : array) {
        const HostProfile p = HostProfile::fromJson(v.toObject());
        if (p.isValid())
            out.append(p);
    }
    return out;
}

void HostProfiles::save(const QList<HostProfile>& profiles)
{
    QJsonArray array;
    for (const HostProfile& p : profiles)
        array.append(p.toJson());
    QSettings().setValue(kKey, QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

std::optional<HostProfile> HostProfiles::find(const QString& name)
{
    for (const HostProfile& p : load())
        if (p.name == name)
            return p;
    return std::nullopt;
}

bool HostProfiles::add(const HostProfile& profile, QString* why)
{
    if (!profile.isValid(why))
        return false;
    QList<HostProfile> all = load();
    for (const HostProfile& p : all)
        if (p.name == profile.name) {
            if (why)
                *why = QObject::tr("A computer named '%1' is already in the list.").arg(profile.name);
            return false;
        }
    all.append(profile);
    save(all);
    return true;
}

bool HostProfiles::update(const QString& oldName, const HostProfile& profile, QString* why)
{
    if (!profile.isValid(why))
        return false;
    QList<HostProfile> all = load();
    int index = -1;
    for (int i = 0; i < all.size(); ++i) {
        if (all[i].name == oldName)
            index = i;
        else if (all[i].name == profile.name) {
            if (why)
                *why = QObject::tr("A computer named '%1' is already in the list.").arg(profile.name);
            return false;
        }
    }
    if (index < 0) {
        if (why)
            *why = QObject::tr("The computer '%1' is no longer in the list.").arg(oldName);
        return false;
    }
    all[index] = profile;
    save(all);
    if (profile.name != oldName) {
        QSettings s;
        for (const QString& prefix : { QStringLiteral("remote/serverCommand/"), QStringLiteral("remote/gpu/") }) {
            if (s.contains(prefix + oldName)) {
                s.setValue(prefix + profile.name, s.value(prefix + oldName));
                s.remove(prefix + oldName);
            }
        }
        if (s.value(QStringLiteral("remote/host")).toString() == oldName)
            s.setValue(QStringLiteral("remote/host"), profile.name);
    }
    return true;
}

bool HostProfiles::remove(const QString& name)
{
    QList<HostProfile> all = load();
    const int before = all.size();
    all.erase(std::remove_if(all.begin(), all.end(), [&](const HostProfile& p) { return p.name == name; }), all.end());
    if (all.size() == before)
        return false;
    save(all);
    return true;
}

} // namespace remote
