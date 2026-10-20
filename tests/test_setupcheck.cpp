// test_setupcheck.cpp - capability list and protocol check (Qt Core only).
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - What is checked follows from the rules in setupcheck.h:
// every capability points at a file that exists, a protocol with a stage the stack
// cannot run is blocked rather than refused, a malformed one is invalid, and the
// progress file keeps what was recorded. The Min 2015 protocol is written out in
// full, so the test also documents what the check says about it.

#include "setupcheck.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <cstdio>

static int g_failed = 0;

static void check(bool ok, const QString& what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
    if (!ok)
        ++g_failed;
}

static QJsonObject parse(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

static QJsonObject stageNamed(const QJsonObject& result, const QString& name)
{
    for (const QJsonValue& v : result.value("stages").toArray())
        if (v.toObject().value("name").toString() == name)
            return v.toObject();
    return {};
}

static bool blockedBy(const QJsonObject& stage, const QString& capability)
{
    for (const QJsonValue& b : stage.value("blockers").toArray())
        if (b.toObject().value("capability").toString() == capability)
            return true;
    return false;
}

static const char* kMin2015 = R"JSON({
  "source": "Min 2015, Soft Matter, doi:10.1039/c4sm02242d",
  "method": "gfnff",
  "system": { "solute_file": "pdmaema30.xyz", "charge": 15, "solvent": "H2O", "solvent_count": 9000,
              "density_g_cm3": 1.0, "periodic": true, "electrostatics": "pppm", "ions": { "anion": "Cl" } },
  "stages": [
    { "name": "min", "type": "minimize" },
    { "name": "nvt", "type": "md", "ensemble": "nvt", "temperature_K": 283, "length_ps": 100,
      "timestep_fs": 1, "thermostat": "nosehover" },
    { "name": "npt", "type": "md", "ensemble": "npt", "temperature_K": 283, "pressure_bar": 1.01325,
      "length_ps": 900, "timestep_fs": 1, "thermostat": "nosehover" },
    { "name": "prod", "type": "md", "ensemble": "npt", "temperature_K": [283, 303, 338],
      "series": "independent", "length_ps": 30000, "timestep_fs": 1, "thermostat": "nosehover" }
  ],
  "analysis": [ { "quantity": "radius_of_gyration", "last_ns": 10 }, { "quantity": "wasa" } ]
})JSON";

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    std::printf("setupcheck\n");

    // --- the list -------------------------------------------------------------------
    QStringList ids;
    bool unique = true, located = true, noted = true;
    for (const setupcheck::Capability& c : setupcheck::capabilities()) {
        unique = unique && !ids.contains(c.id);
        ids << c.id;
        noted = noted && !c.note.isEmpty();
        const QString root = c.where.startsWith("curcuma:") ? CURCUMA_SOURCE_DIR : QURCUMA_SOURCE_DIR;
        const int colon = c.where.indexOf(':');
        const bool exists = colon > 0 && QFile::exists(QDir(root).filePath(c.where.mid(colon + 1)));
        if (!exists)
            std::printf("    missing path for %s: %s\n", qPrintable(c.id), qPrintable(c.where));
        located = located && exists;
    }
    check(unique, "capability ids are unique");
    check(noted, "every capability has a note");
    check(located, "every capability points at an existing file");
    check(setupcheck::findCapability("md.npt") && setupcheck::findCapability("md.npt")->state == setupcheck::State::Missing,
        "NPT is listed as missing");

    // --- Min 2015 ---------------------------------------------------------------------
    const QJsonObject r = setupcheck::checkProtocol(parse(kMin2015));
    check(r.value("ok").toBool(), "Min 2015 protocol is well formed");
    const QJsonObject s = r.value("summary").toObject();
    check(s.value("stages").toInt() == 4 && s.value("invalid").toInt() == 0, "four stages, none invalid");
    // The system is periodic with PPPM, so every stage is blocked by md.periodic -- the
    // protocol states what the paper did, and the stack cannot do it yet.
    check(blockedBy(stageNamed(r, "min"), "md.periodic"), "min blocked by periodic non-bonded terms");
    check(blockedBy(stageNamed(r, "nvt"), "md.periodic") && blockedBy(stageNamed(r, "nvt"), "md.longrange_electrostatics"),
        "nvt blocked by periodicity and long-range electrostatics");
    check(blockedBy(stageNamed(r, "npt"), "md.npt"), "npt blocked by the missing barostat");
    const QJsonObject prod = stageNamed(r, "prod");
    check(prod.value("runs").toArray().size() == 3, "three temperatures give three runs");
    const QJsonObject firstRun = prod.value("runs").toArray().first().toObject().value("arguments").toObject();
    check(firstRun.value("steps").toDouble() == 30000000.0 && firstRun.value("temperature").toDouble() == 283.0,
        "30 ns at 1 fs is 3e7 steps, first run at 283 K");
    check(stageNamed(r, "nvt").value("runs").toArray().first().toObject().value("arguments").toObject().value("steps").toDouble() == 100000.0,
        "100 ps at 1 fs is 1e5 steps");
    bool longNote = false;
    for (const QJsonValue& n : prod.value("notes").toArray())
        longNote = longNote || n.toString().startsWith("run.long");
    check(longNote, "the 1e6-step limit of one call is noted");
    const QJsonArray prepare = r.value("prepare").toArray();
    check(prepare.size() == 3 && prepare[0].toObject().value("tool").toString() == "open_structure"
            && prepare[1].toObject().value("tool").toString() == "fill_container"
            && prepare[2].toObject().value("tool").toString() == "add_counterions"
            && prepare[2].toObject().value("arguments").toObject().value("charge").toInt() == 15,
        "system preparation: open, fill, ions with charge 15");
    const QJsonArray analysis = r.value("analysis").toArray();
    check(analysis.size() == 2 && analysis[0].toObject().value("state").toString() == "ready"
            && analysis[1].toObject().value("state").toString() == "blocked",
        "Rg ready (analyze_trajectory), WASA blocked");

    // --- non-periodic version runs where it can ---------------------------------------
    QJsonObject open = parse(kMin2015);
    QJsonObject system = open.value("system").toObject();
    system.insert("periodic", false);
    open.insert("system", system);
    const QJsonObject o = setupcheck::checkProtocol(open);
    check(stageNamed(o, "min").value("state").toString() == "ready" && stageNamed(o, "nvt").value("state").toString() == "ready",
        "without periodicity, minimise and NVT are ready");
    check(stageNamed(o, "npt").value("state").toString() == "blocked", "NPT stays blocked");

    // --- malformed ---------------------------------------------------------------------
    const QJsonObject bad = setupcheck::checkProtocol(parse(R"JSON({
      "stages": [
        { "name": "a", "type": "md", "temperature_K": [283, 303], "length_ps": 10 },
        { "name": "b", "type": "md", "temperature_K": [283, 303], "series": "chained", "length_ps": 10 },
        { "name": "b", "type": "relax" },
        { "name": "d", "type": "md", "ensemble": "nvt", "thermostat": "magic", "temperature_K": 300, "length_ps": 1 }
      ],
      "execution": { "parallel": true },
      "analysis": [ { "quantity": "nonsense" } ] })JSON"));
    check(!bad.value("ok").toBool(), "malformed protocol is not ok");
    check(stageNamed(bad, "a").value("state").toString() == "invalid", "several temperatures without series: invalid");
    check(stageNamed(bad, "d").value("state").toString() == "invalid", "unknown thermostat: invalid");
    bool parallelChained = false, duplicate = false, unknownType = false, unknownQuantity = false;
    for (const QJsonValue& e : bad.value("errors").toArray()) {
        const QString text = e.toString();
        parallelChained = parallelChained || (text.contains("chained") && text.contains("parallel"));
        duplicate = duplicate || text.contains("used twice");
        unknownType = unknownType || text.contains("relax");
        unknownQuantity = unknownQuantity || text.contains("nonsense");
    }
    check(parallelChained, "parallel execution of a chained series is refused");
    check(duplicate && unknownType && unknownQuantity, "duplicate name, unknown type and unknown quantity are reported");

    const QJsonObject pcff = setupcheck::checkProtocol(parse(R"JSON({ "method": "pcff", "stages": [ { "name": "m", "type": "minimize" } ] })JSON"));
    check(blockedBy(stageNamed(pcff, "m"), "ff.pcff"), "PCFF blocks the stage");
    const QJsonObject unknownMethod = setupcheck::checkProtocol(parse(R"JSON({ "method": "xyz", "stages": [ { "name": "m", "type": "minimize" } ] })JSON"));
    check(!unknownMethod.value("ok").toBool(), "an unknown method is an error");

    // --- progress file -------------------------------------------------------------------
    QTemporaryDir dir;
    QString error;
    check(setupcheck::initStatus(dir.path(), { "min", "nvt" }, &error), "status.json is created");
    check(setupcheck::setStageState(dir.path(), "min", "done", "min.xyz", &error), "a stage is set to done");
    check(!setupcheck::setStageState(dir.path(), "min", "finished", {}, &error), "an unknown state is refused");
    check(!setupcheck::setStageState(dir.path(), "npt", "done", {}, &error), "a stage not in the file is refused");
    check(setupcheck::initStatus(dir.path(), { "min", "nvt", "npt" }, &error), "re-initialising with a new stage works");
    const QJsonObject st = setupcheck::readStatus(dir.path()).value("stages").toObject();
    check(st.value("min").toObject().value("state").toString() == "done" && st.value("min").toObject().value("note").toString() == "min.xyz",
        "an existing stage keeps its state and note");
    check(st.value("npt").toObject().value("state").toString() == "pending" && st.value("nvt").toObject().value("state").toString() == "pending",
        "new and untouched stages are pending");

    std::printf("%s\n", g_failed ? "FAILED" : "all passed");
    return g_failed ? 1 : 0;
}
