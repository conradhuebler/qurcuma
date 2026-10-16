// programpaths.h - external program table, path detection/validation, ORCID check
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

class QSettings;

/**
 * @brief GUI-free logic behind the Settings dialog: which external programs qurcuma starts,
 *        where they are looked for, whether a configured path is usable, and the ORCID check.
 *
 * Paths are stored in QSettings under "programs/<name>" (settingsKey()). ORCA is a folder
 * (orca and its orca_* helpers live side by side), every other program is an executable file.
 * Claude Generated 2026
 */
namespace programpaths {

/// What a configured path has to point to.
enum class Kind { File, Directory };

/// One external program. @c name is the executable name and the settings key suffix.
struct ProgramSpec {
    QString name;
    Kind kind = Kind::File;
    QString label;  ///< translated display name
    QString help;   ///< translated one-line description of what qurcuma uses it for
};

/// Result of validate(): a state plus one translated line for the status label.
struct Check {
    enum State { NotSet, Ok, Missing, NotExecutable, WrongKind };
    State state = NotSet;
    QString text;
};

/// The programs qurcuma starts (curcuma, ORCA, xtb, IboView, Avogadro). Claude Generated 2026
QVector<ProgramSpec> specs();

/// The spec for @p name; a File spec with that name when the table has none. Claude Generated 2026
ProgramSpec spec(const QString& name);

/// QSettings key of a program path ("programs/<name>"). Claude Generated 2026
QString settingsKey(const QString& name);

/// Legacy QSettings key of the ORCA folder before October 2026. Claude Generated 2026
QString legacyOrcaKey();

/// A short fixed list of usual install folders for @p spec, plus `/opt/<name>*` and
/// `~/<name>*` folders (newest name first) and their `bin/`. Claude Generated 2026
QStringList usualFolders(const ProgramSpec& spec);

/// First match of @p spec in @p folders, in order: the executable for a File spec (Avogadro
/// also as `avogadro2`), the folder
/// that validate() accepts for a Directory spec (a symlinked `orca` is followed to its folder).
/// Empty when nothing matches. Claude Generated 2026
QString detectIn(const ProgramSpec& spec, const QStringList& folders);

/// detectIn() over the folders of the PATH environment variable followed by usualFolders().
/// Claude Generated 2026
QString detect(const ProgramSpec& spec);

/**
 * @brief Checks @p path for @p spec.
 *
 * File: the file exists, is not a folder, and is executable. A bare name without a slash is
 * looked up in PATH, as QProcess does.
 * Directory (ORCA): the folder exists and holds an executable `orca` plus at least one
 * executable `orca_*` helper. The helper check tells the quantum chemistry package apart from
 * the Orca screen reader, which installs a lone `/usr/bin/orca` on GNOME systems.
 * Claude Generated 2026
 */
Check validate(const ProgramSpec& spec, const QString& path);

/// Result of checkOrcid().
enum class OrcidState { Empty, BadFormat, BadChecksum, Ok };

/**
 * @brief ORCID iD check: format `0000-0000-0000-000X` (four groups of four digits, the last
 *        character may be `X`) and the ISO 7064 MOD 11-2 check digit over the first 15 digits.
 *        Leading/trailing blanks are ignored. Does not tell whether the iD is registered.
 * Claude Generated 2026
 */
OrcidState checkOrcid(const QString& orcid);

/**
 * @brief One-time clean-up of the program keys.
 *
 * Drops, for every program, the placeholder the old first-start defaults wrote ("/usr/local/bin/<name>",
 * "C:/Program Files/<name>") unless it is a valid path; nothing selected those programs through a
 * placeholder on purpose. Moves the ORCA folder from legacyOrcaKey() to settingsKey("orca") when the
 * new key is empty and removes the legacy key in every case, so a second call changes nothing.
 * @return true when the settings were changed.
 * Claude Generated 2026
 */
bool migrateLegacyKeys(QSettings& settings);

} // namespace programpaths
