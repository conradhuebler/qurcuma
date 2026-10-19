// programpaths.cpp - external program table, path detection/validation, ORCID check
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "programpaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

namespace programpaths {

namespace {

/// Translation in the "programpaths" context (%n plural when @p n >= 0). Claude Generated 2026
QString tr(const char* text, int n = -1)
{
    return QCoreApplication::translate("programpaths", text, nullptr, n);
}

/// Executable names tried for a File spec; Avogadro 2 installs as `avogadro2`. Claude Generated 2026
QStringList executableNames(const ProgramSpec& spec)
{
    if (spec.name == QLatin1String("avogadro"))
        return { QStringLiteral("avogadro2"), QStringLiteral("avogadro") };
    return { spec.name };
}

/// Values the removed first-start defaults (Settings::loadDefaults) wrote into programs/<name>.
/// Claude Generated 2026
bool isOldPlaceholder(const QString& name, const QString& value)
{
    return value == QLatin1String("/usr/local/bin/") + name || value == QLatin1String("C:/Program Files/") + name;
}

} // namespace

QVector<ProgramSpec> specs()
{
    return {
        { QStringLiteral("curcuma"), Kind::File, tr("curcuma"),
            tr("curcuma executable started for external calculations. Interactive simulations use the "
               "curcuma library built into qurcuma and do not need it.") },
        { QStringLiteral("orca"), Kind::Directory, tr("ORCA folder"),
            tr("Folder that contains the orca program and its orca_* helpers (orca_pltvib for "
               "vibrations, orca_2mkl for orbitals).") },
        { QStringLiteral("xtb"), Kind::File, tr("xtb"), tr("xtb executable started for external calculations.") },
        { QStringLiteral("iboview"), Kind::File, tr("IboView"),
            tr("Orbital viewer; opens .molden files and ORCA orbital files (.gbw, .loc, .ges).") },
        { QStringLiteral("avogadro"), Kind::File, tr("Avogadro"),
            tr("Molecule viewer; opens structure files and ORCA vibration modes.") },
    };
}

ProgramSpec spec(const QString& name)
{
    for (const ProgramSpec& s : specs())
        if (s.name == name)
            return s;
    return { name, Kind::File, name, QString() };
}

QString settingsKey(const QString& name)
{
    return QStringLiteral("programs/") + name;
}

QString legacyOrcaKey()
{
    return QStringLiteral("orca/binaryPath");
}

QStringList usualFolders(const ProgramSpec& spec)
{
    const QString home = QDir::homePath();
    QStringList folders;
#ifdef Q_OS_WIN
    folders << QStringLiteral("C:/Program Files/") + spec.name << QStringLiteral("C:/Program Files/") + spec.name + "/bin";
#else
    folders << home + "/.local/bin" << home + "/bin" << QStringLiteral("/usr/local/bin") << QStringLiteral("/opt/homebrew/bin");
#endif
    // Versioned install folders such as /opt/orca_6_0_1 or ~/xtb-6.7.1; reversed name order
    // puts the newest version first.
    for (const QString& base : { home, QStringLiteral("/opt") }) {
        const QDir dir(base);
        const QStringList names = dir.entryList({ spec.name + "*" }, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
        for (const QString& n : names)
            folders << dir.filePath(n) << dir.filePath(n) + "/bin";
    }
    return folders;
}

QString detectIn(const ProgramSpec& spec, const QStringList& folders)
{
    for (const QString& folder : folders) {
        if (folder.isEmpty())
            continue;
        if (spec.kind == Kind::File) {
            for (const QString& exe : executableNames(spec)) {
                const QString found = QStandardPaths::findExecutable(exe, { folder });
                if (!found.isEmpty())
                    return QDir::cleanPath(found);
            }
            continue;
        }
        if (validate(spec, folder).state == Check::Ok)
            return QDir::cleanPath(QDir(folder).absolutePath());
        // A symlinked program (e.g. /usr/local/bin/orca -> /opt/orca/orca): try the target's folder.
        const QFileInfo link(QDir(folder).filePath(spec.name));
        if (link.isSymLink()) {
            const QString target = QFileInfo(link.canonicalFilePath()).absolutePath();
            if (!target.isEmpty() && validate(spec, target).state == Check::Ok)
                return QDir::cleanPath(target);
        }
    }
    return QString();
}

QString detect(const ProgramSpec& spec)
{
    QStringList folders = QString::fromLocal8Bit(qgetenv("PATH")).split(QDir::listSeparator(), Qt::SkipEmptyParts);
    folders += usualFolders(spec);
    folders.removeDuplicates();
    return detectIn(spec, folders);
}

Check validate(const ProgramSpec& spec, const QString& path)
{
    const QString p = path.trimmed();
    if (p.isEmpty())
        return { Check::NotSet, tr("Not set") };

    if (spec.kind == Kind::File) {
        // A bare name is what QProcess resolves through PATH.
        if (!p.contains(QLatin1Char('/')) && !p.contains(QLatin1Char('\\'))) {
            const QString found = QStandardPaths::findExecutable(p);
            if (found.isEmpty())
                return { Check::Missing, tr("No program of this name in PATH") };
            return { Check::Ok, tr("Found in PATH: %1").arg(QDir::toNativeSeparators(found)) };
        }
        const QFileInfo info(p);
        if (!info.exists())
            return { Check::Missing, tr("File not found") };
        if (info.isDir())
            return { Check::WrongKind, tr("This is a folder; choose the program file") };
        if (!info.isExecutable())
            return { Check::NotExecutable, tr("File is not executable") };
        return { Check::Ok, tr("Executable found") };
    }

    const QFileInfo info(p);
    if (!info.exists())
        return { Check::Missing, tr("Folder not found") };
    if (!info.isDir())
        return { Check::WrongKind, tr("This is a file; choose the folder that contains it") };
    const QDir dir(info.absoluteFilePath());
    const QString exe = spec.name;
    if (QStandardPaths::findExecutable(exe, { dir.absolutePath() }).isEmpty()) {
        if (QFileInfo::exists(dir.filePath(exe)))
            return { Check::NotExecutable, tr("%1 in this folder is not executable").arg(exe) };
        return { Check::Missing, tr("No %1 program in this folder").arg(exe) };
    }
    const QStringList helpers = dir.entryList({ exe + "_*" }, QDir::Files | QDir::Executable);
    if (helpers.isEmpty())
        return { Check::WrongKind, tr("%1 here has no %1_* helpers next to it; not an ORCA folder").arg(exe) };
    return { Check::Ok, tr("%1 and %n helper program(s) found", int(helpers.size())).arg(exe) };
}

OrcidState checkOrcid(const QString& orcid)
{
    const QString s = orcid.trimmed();
    if (s.isEmpty())
        return OrcidState::Empty;
    static const QRegularExpression format(QStringLiteral("^[0-9]{4}-[0-9]{4}-[0-9]{4}-[0-9]{3}[0-9X]$"));
    if (!format.match(s).hasMatch())
        return OrcidState::BadFormat;
    // ISO 7064 MOD 11-2 over the first 15 digits; 10 is written as X.
    int total = 0;
    int digits = 0;
    for (const QChar c : s) {
        if (digits == 15)
            break;
        if (c == QLatin1Char('-'))
            continue;
        total = (total + (c.unicode() - '0')) * 2;
        ++digits;
    }
    const int result = (12 - total % 11) % 11;
    const QChar expected = result == 10 ? QLatin1Char('X') : QChar('0' + result);
    return s.back() == expected ? OrcidState::Ok : OrcidState::BadChecksum;
}

bool migrateLegacyKeys(QSettings& settings)
{
    bool changed = false;
    // The old first-start defaults wrote fixed paths (/usr/local/bin/<program>) for every program;
    // such a value is dropped unless it really is a valid path, so that "not set" is shown (and
    // Detect offered) instead of a path that does not exist.
    for (const ProgramSpec& s : specs()) {
        const QString k = settingsKey(s.name);
        if (!settings.contains(k))
            continue;
        const QString value = settings.value(k).toString().trimmed();
        if (isOldPlaceholder(s.name, value) && validate(s, value).state != Check::Ok) {
            settings.remove(k);
            changed = true;
        }
    }
    // ORCA lived under its own key; the value moves to programs/orca once.
    const QString key = settingsKey(QStringLiteral("orca"));
    if (settings.contains(legacyOrcaKey())) {
        const QString current = settings.value(key).toString().trimmed();
        const QString legacy = settings.value(legacyOrcaKey()).toString().trimmed();
        if (current.isEmpty() && !legacy.isEmpty())
            settings.setValue(key, legacy);
        settings.remove(legacyOrcaKey());
        changed = true;
    }
    if (changed)
        settings.sync();
    return changed;
}

} // namespace programpaths
