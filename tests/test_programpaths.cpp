// test_programpaths.cpp - program path validation/detection, ORCID check, ORCA key migration
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026: covers src/programpaths.* (the GUI-free part of the Settings dialog)
// with fake executables in a temporary folder, a modified PATH and a test QSettings file.

#include "programpaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <iostream>

using namespace programpaths;

static int failures = 0;
static int checks = 0;
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        ++checks;                                                                           \
        if (!(cond)) {                                                                      \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond << std::endl;               \
            ++failures;                                                                     \
        }                                                                                   \
    } while (0)

/// Writes a small file at @p path (creating its folder) and sets the executable bits on request.
static bool makeFile(const QString& path, bool executable)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write("#!/bin/sh\nexit 0\n");
    f.close();
    QFileDevice::Permissions p = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
    if (executable)
        p |= QFileDevice::ExeOwner;
    return f.setPermissions(p);
}

static QString canonical(const QString& path)
{
    return QFileInfo(path).canonicalFilePath();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("qurcuma-test"));
    app.setApplicationName(QStringLiteral("test_programpaths"));
    QStandardPaths::setTestModeEnabled(true);

    QTemporaryDir tmp;
    CHECK(tmp.isValid());
    const QString d = tmp.path();
    CHECK(makeFile(d + "/bin/xtb", true));
    CHECK(makeFile(d + "/bin/avogadro2", true));
    CHECK(makeFile(d + "/plain/curcuma", false));
    CHECK(makeFile(d + "/orca_ok/orca", true));
    CHECK(makeFile(d + "/orca_ok/orca_2mkl", true));
    CHECK(makeFile(d + "/orca_lone/orca", true));          // like the Orca screen reader
    CHECK(makeFile(d + "/orca_noexec/orca", false));
    CHECK(makeFile(d + "/orca_noexec/orca_2mkl", true));
    CHECK(QDir().mkpath(d + "/empty"));
    CHECK(QDir().mkpath(d + "/link"));
    CHECK(QFile::link(d + "/orca_ok/orca", d + "/link/orca"));

    const ProgramSpec xtb = spec(QStringLiteral("xtb"));
    const ProgramSpec orca = spec(QStringLiteral("orca"));
    const ProgramSpec avogadro = spec(QStringLiteral("avogadro"));
    CHECK(xtb.kind == Kind::File);
    CHECK(orca.kind == Kind::Directory);
    CHECK(specs().size() == 5);
    CHECK(settingsKey(QStringLiteral("orca")) == QLatin1String("programs/orca"));

    const QByteArray oldPath = qgetenv("PATH");
    qputenv("PATH", QFile::encodeName(d + "/bin"));

    // validate(): File
    CHECK(validate(xtb, QString()).state == Check::NotSet);
    CHECK(validate(xtb, QStringLiteral("   ")).state == Check::NotSet);
    CHECK(validate(xtb, d + "/bin/xtb").state == Check::Ok);
    CHECK(validate(xtb, " " + d + "/bin/xtb ").state == Check::Ok);
    CHECK(validate(xtb, d + "/bin/nothing").state == Check::Missing);
    CHECK(validate(xtb, d + "/bin").state == Check::WrongKind);
    CHECK(validate(xtb, d + "/plain/curcuma").state == Check::NotExecutable);
    CHECK(validate(xtb, QStringLiteral("xtb")).state == Check::Ok);   // bare name through PATH
    CHECK(validate(xtb, QStringLiteral("no-such-program-qurcuma-test")).state == Check::Missing);
    CHECK(!validate(xtb, d + "/bin/xtb").text.isEmpty());

    // validate(): Directory (ORCA)
    CHECK(validate(orca, QString()).state == Check::NotSet);
    CHECK(validate(orca, d + "/orca_ok").state == Check::Ok);
    CHECK(validate(orca, d + "/orca_ok/").state == Check::Ok);
    CHECK(validate(orca, d + "/nothing").state == Check::Missing);
    CHECK(validate(orca, d + "/orca_ok/orca").state == Check::WrongKind);
    CHECK(validate(orca, d + "/empty").state == Check::Missing);
    CHECK(validate(orca, d + "/orca_noexec").state == Check::NotExecutable);
    CHECK(validate(orca, d + "/orca_lone").state == Check::WrongKind);

    // detect() through PATH, detectIn() over explicit folders
    CHECK(canonical(detect(xtb)) == canonical(d + "/bin/xtb"));
    CHECK(canonical(detect(avogadro)) == canonical(d + "/bin/avogadro2"));
    qputenv("PATH", QFile::encodeName(d + "/orca_lone" + QDir::listSeparator() + d + "/orca_ok"));
    CHECK(canonical(detect(orca)) == canonical(d + "/orca_ok"));      // skips the lone orca
    CHECK(detectIn(xtb, { d + "/empty" }).isEmpty());
    CHECK(detectIn(xtb, { QString(), d + "/empty", d + "/bin" }) == QDir::cleanPath(d + "/bin/xtb"));
    CHECK(detectIn(orca, { d + "/orca_lone", d + "/empty" }).isEmpty());
    CHECK(canonical(detectIn(orca, { d + "/link" })) == canonical(d + "/orca_ok"));  // symlink followed
    CHECK(!usualFolders(xtb).isEmpty());
    qputenv("PATH", oldPath);

    // ORCID: format and ISO 7064 MOD 11-2 check digit
    CHECK(checkOrcid(QStringLiteral("0000-0002-1825-0097")) == OrcidState::Ok);
    CHECK(checkOrcid(QStringLiteral("0000-0001-5109-3700")) == OrcidState::Ok);
    CHECK(checkOrcid(QStringLiteral("0000-0002-1694-233X")) == OrcidState::Ok);
    CHECK(checkOrcid(QStringLiteral(" 0000-0002-1825-0097 ")) == OrcidState::Ok);
    CHECK(checkOrcid(QStringLiteral("0000-0002-1825-0098")) == OrcidState::BadChecksum);
    CHECK(checkOrcid(QStringLiteral("0000-0002-1694-2330")) == OrcidState::BadChecksum);
    CHECK(checkOrcid(QStringLiteral("0000-0002-1825-009")) == OrcidState::BadFormat);
    CHECK(checkOrcid(QStringLiteral("0000000218250097")) == OrcidState::BadFormat);
    CHECK(checkOrcid(QStringLiteral("0000-0002-1694-233x")) == OrcidState::BadFormat);
    CHECK(checkOrcid(QStringLiteral("000X-0002-1825-0097")) == OrcidState::BadFormat);
    CHECK(checkOrcid(QStringLiteral("https://orcid.org/0000-0002-1825-0097")) == OrcidState::BadFormat);
    CHECK(checkOrcid(QString()) == OrcidState::Empty);

    // migrateLegacyKeys() on a QSettings file of the test organisation
    QSettings s(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("qurcuma-test"), QStringLiteral("test_programpaths"));
    if (!s.fileName().contains(QLatin1String(".qttest"))) {
        std::cerr << "FAIL: settings file is not in the Qt test location: " << qPrintable(s.fileName()) << std::endl;
        return 1;
    }
    const QString key = settingsKey(QStringLiteral("orca"));
    s.clear();
    CHECK(!migrateLegacyKeys(s));                                   // nothing to do

    s.setValue(legacyOrcaKey(), QStringLiteral("/opt/orca_6_0_1"));    // legacy only
    CHECK(migrateLegacyKeys(s));
    CHECK(s.value(key).toString() == QLatin1String("/opt/orca_6_0_1"));
    CHECK(!s.contains(legacyOrcaKey()));
    CHECK(!migrateLegacyKeys(s));                                   // once
    CHECK(s.value(key).toString() == QLatin1String("/opt/orca_6_0_1"));

    s.clear();                                                         // legacy + old placeholder
    s.setValue(key, QStringLiteral("/usr/local/bin/orca"));
    s.setValue(legacyOrcaKey(), QStringLiteral("/opt/orca"));
    CHECK(migrateLegacyKeys(s));
    CHECK(s.value(key).toString() == QLatin1String("/opt/orca"));
    CHECK(!s.contains(legacyOrcaKey()));

    s.clear();                                                         // legacy + a real new value
    s.setValue(key, d + "/orca_ok");
    s.setValue(legacyOrcaKey(), QStringLiteral("/opt/orca"));
    CHECK(migrateLegacyKeys(s));
    CHECK(s.value(key).toString() == d + "/orca_ok");
    CHECK(!s.contains(legacyOrcaKey()));

    s.clear();                                                         // old placeholder only
    s.setValue(key, QStringLiteral("/usr/local/bin/orca"));
    CHECK(migrateLegacyKeys(s));
    CHECK(!s.contains(key));
    CHECK(!migrateLegacyKeys(s));

    s.clear();                                                         // empty legacy value
    s.setValue(legacyOrcaKey(), QString());
    CHECK(migrateLegacyKeys(s));
    CHECK(s.value(key).toString().isEmpty());
    CHECK(!s.contains(legacyOrcaKey()));

    s.clear();
    s.sync();
    QFile::remove(s.fileName());

    // the placeholders of the old first-start defaults go for every program, real choices stay
    s.clear();
    s.setValue(settingsKey(QStringLiteral("xtb")), QStringLiteral("/usr/local/bin/xtb"));       // placeholder, no such file here
    s.setValue(settingsKey(QStringLiteral("curcuma")), QStringLiteral("/usr/local/bin/curcuma"));
    s.setValue(settingsKey(QStringLiteral("iboview")), d + "/ibo_chosen");                       // a real choice, even if missing
    s.setValue(settingsKey(QStringLiteral("avogadro")), QStringLiteral("avogadro2"));            // a bare name stays
    CHECK(migrateLegacyKeys(s));
    CHECK(!s.contains(settingsKey(QStringLiteral("xtb"))));
    CHECK(!s.contains(settingsKey(QStringLiteral("curcuma"))));
    CHECK(s.value(settingsKey(QStringLiteral("iboview"))).toString() == d + "/ibo_chosen");
    CHECK(s.value(settingsKey(QStringLiteral("avogadro"))).toString() == QLatin1String("avogadro2"));
    CHECK(!migrateLegacyKeys(s));                                                                  // once

    if (failures) {
        std::cerr << failures << " of " << checks << " checks failed" << std::endl;
        return 1;
    }
    std::cout << "test_programpaths: all " << checks << " checks passed" << std::endl;
    return 0;
}
