// test_remote_protocol.cpp - wire format and file policy of the remote protocol
// Claude Generated 2026 (WP remote compute R1). Qt-only checks, no curcuma run.

#include "remote/protocol.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>
#include <iostream>

static int failures = 0;
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond << std::endl;                \
            ++failures;                                                                     \
        }                                                                                   \
    } while (0)

static SimulationFrame sampleFrame()
{
    SimulationFrame f;
    f.step = 42;
    f.energy = -12.345678901234;
    f.ekin = 0.5;
    f.temperature = 301.25;
    f.targetTemperature = 300.0;
    f.positions = { QVector3D(1.5f, -2.25f, 3.125f), QVector3D(0.f, 0.f, 0.f), QVector3D(-7.f, 8.5f, 9.f) };
    f.bonds = { { 0, 1, 1 }, { 1, 2, 3 } };
    f.topologyVersion = 7;
    ReactEventView e;
    e.step = 40;
    e.formed = { { 0, 2 } };
    e.broken = { { 1, 2 }, { 0, 1 } };
    e.deJumpKJmol = 1.5;
    f.events.append(e);
    nci::Contact c;
    c.kind = nci::Kind::PiStacking;
    c.donor = 0; c.bridge = -1; c.acceptor = 2;
    c.ringA = { 0, 1, 2 }; c.ringB = { 3, 4 };
    c.distance = 3.5f; c.angle = 12.f; c.offset = 1.25f; c.score = 0.75f;
    c.energy = -4.5; c.hasEnergy = true; c.motif = 2;
    f.nciContacts.append(c);
    return f;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // frame round trip, all optional blocks present
    {
        const SimulationFrame f = sampleFrame();
        SimulationFrame g;
        QString err;
        const QByteArray bytes = remote::encodeFrame(f);
        CHECK(remote::decodeFrame(bytes, g, &err));
        CHECK(g.step == f.step && g.energy == f.energy && g.ekin == f.ekin);
        CHECK(g.temperature == f.temperature && g.targetTemperature == f.targetTemperature);
        CHECK(g.positions.size() == f.positions.size());
        for (size_t i = 0; i < f.positions.size(); ++i)
            CHECK(g.positions[i] == f.positions[i]);  // chosen exactly representable in float32
        CHECK(g.bonds.size() == 2 && g.bonds[1].order == 3 && g.topologyVersion == 7);
        CHECK(g.events.size() == 1 && g.events[0].broken.size() == 2 && g.events[0].formed[0].second == 2);
        CHECK(g.events[0].deJumpKJmol == 1.5 && g.events[0].step == 40);
        CHECK(g.nciContacts.size() == 1 && g.nciContacts[0].ringA.size() == 3 && g.nciContacts[0].ringB.size() == 2);
        CHECK(g.nciContacts[0].kind == nci::Kind::PiStacking && g.nciContacts[0].hasEnergy
              && g.nciContacts[0].energy == -4.5 && g.nciContacts[0].motif == 2 && g.nciContacts[0].offset == 1.25f);

        // every truncation of a valid message is rejected, none crashes
        int accepted = 0;
        for (int n = 0; n < bytes.size(); ++n)
            accepted += remote::decodeFrame(bytes.left(n), g, nullptr) ? 1 : 0;
        CHECK(accepted == 0);
        CHECK(!remote::decodeFrame(bytes + QByteArray(1, 'x'), g, nullptr));
    }

    // frame without optional blocks stays small and round-trips
    {
        SimulationFrame f;
        f.step = 1;
        f.positions.assign(1000, QVector3D(1.f, 2.f, 3.f));
        const QByteArray bytes = remote::encodeFrame(f);
        CHECK(bytes.size() == 1 + 1 + 4 + 4 * 8 + 4 + 1000 * 12);
        SimulationFrame g;
        CHECK(remote::decodeFrame(bytes, g, nullptr) && g.bonds.empty() && g.topologyVersion == -1 && g.events.isEmpty());
    }

    // a corrupt atom count must not allocate or read past the message
    {
        QByteArray bytes = remote::encodeFrame(sampleFrame());
        const int nOff = 1 + 1 + 4 + 4 * 8;
        const qint32 huge = 0x7fffffff;
        bytes.replace(nOff, 4, QByteArray(reinterpret_cast<const char*>(&huge), 4));
        SimulationFrame g;
        CHECK(!remote::decodeFrame(bytes, g, nullptr));
    }

    // atoms and bonds as JSON
    {
        QVector<MolAtom> a(2);
        a[0].element = "C"; a[0].position = QVector3D(1, 2, 3); a[0].charge = -0.25f; a[0].radius = 1.5f; a[0].type = "ppo1";
        a[1].element = "H"; a[1].position = QVector3D(-1, 0.5f, 0);
        QVector<MolAtom> b;
        QString err;
        CHECK(remote::atomsFromJson(remote::atomsToJson(a), b, &err));
        CHECK(b.size() == 2 && b[0].element == "C" && b[0].position == a[0].position && b[0].charge == -0.25f
              && b[0].radius == 1.5f && b[0].type == "ppo1" && b[1].element == "H");
        QVector<MolBond> bd{ { 0, 1, 2 } }, bd2;
        CHECK(remote::bondsFromJson(remote::bondsToJson(bd), bd2, &err) && bd2.size() == 1 && bd2[0].bondOrder == 2);
        QJsonObject bad = remote::atomsToJson(a);
        bad["x"] = QJsonArray{ 1.0 };
        CHECK(!remote::atomsFromJson(bad, b, &err));
    }

    // uploaded file round trip, checksum and name checks
    {
        const QByteArray data("3\ncomment\nC 0 0 0\n");
        QString name, err;
        QByteArray out;
        QByteArray msg = remote::encodeFile("ref.xyz", data);
        CHECK(remote::decodeFile(msg, name, out, &err) && name == "ref.xyz" && out == data);
        msg[msg.size() - 1] = msg[msg.size() - 1] ^ 1;
        CHECK(!remote::decodeFile(msg, name, out, &err));  // checksum
        CHECK(!remote::decodeFile(remote::encodeFile("../x.xyz", data), name, out, &err));
        CHECK(!remote::decodeFile(remote::encodeFile("/etc/passwd", data), name, out, &err));
    }

    // run configuration: optSingleShot survives, and the rest of the config too
    {
        SimulationConfig c;
        c.mode = SimulationConfig::Mode::GeometryOptimization;
        c.optSingleShot = true;
        c.steps = 77;
        c.thermostat = "andersen";
        c.rmsdMtdRefFile = "ref.xyz";
        const SimulationConfig d = remote::configFromJson(remote::configToJson(c));
        CHECK(d.optSingleShot && d.steps == 77 && d.thermostat == "andersen" && d.rmsdMtdRefFile == "ref.xyz");
        CHECK(d.mode == SimulationConfig::Mode::GeometryOptimization);
        CHECK(!remote::configFromJson(remote::configToJson(SimulationConfig())).optSingleShot);
    }

    // file names
    CHECK(remote::isSafeFileName("ref.xyz") && remote::isSafeFileName("a-b_c+d.1.xyz"));
    CHECK(!remote::isSafeFileName("") && !remote::isSafeFileName("..") && !remote::isSafeFileName("a/b"));
    CHECK(!remote::isSafeFileName("a\\b") && !remote::isSafeFileName(".hidden") && !remote::isSafeFileName("a..b"));
    CHECK(!remote::isSafeFileName(QString(129, 'a')) && !remote::isSafeFileName("a b") && !remote::isSafeFileName("a\n"));

    // file policy
    {
        const QSet<QString> up{ "ref.xyz", "r.rst" };
        QString err;
        QJsonObject cfg{ { "rmsdMtdRefFile", "none" }, { "mdExtraParams", QJsonObject{ { "seed", 5 } } } };
        CHECK(remote::applyFilePolicy(cfg, up, "/s/in", &err));

        cfg = QJsonObject{ { "rmsdMtdRefFile", "ref.xyz" } };
        CHECK(remote::applyFilePolicy(cfg, up, "/s/in", &err) && cfg["rmsdMtdRefFile"].toString() == "/s/in/ref.xyz");

        cfg = QJsonObject{ { "rmsdMtdRefFile", "missing.xyz" } };
        CHECK(!remote::applyFilePolicy(cfg, up, "/s/in", &err));
        cfg = QJsonObject{ { "rmsdMtdRefFile", "/etc/passwd" } };
        CHECK(!remote::applyFilePolicy(cfg, up, "/s/in", &err));
        cfg = QJsonObject{ { "rmsdMtdRefFile", "../ref.xyz" } };
        CHECK(!remote::applyFilePolicy(cfg, up, "/s/in", &err));

        cfg = QJsonObject{ { "mdExtraParams", QJsonObject{ { "restart_file", "r.rst" } } } };
        CHECK(remote::applyFilePolicy(cfg, up, "/s/in", &err)
              && cfg["mdExtraParams"].toObject()["restart_file"].toString() == "/s/in/r.rst");
        cfg = QJsonObject{ { "mdExtraParams", QJsonObject{ { "plumed_file", "other.dat" } } } };
        CHECK(!remote::applyFilePolicy(cfg, up, "/s/in", &err));
        cfg = QJsonObject{ { "mdExtraParams", QJsonObject{ { "load_ff_json", "ref.xyz" } } } };
        CHECK(!remote::applyFilePolicy(cfg, up, "/s/in", &err));
        cfg = QJsonObject{ { "mdExtraParams", QJsonObject{ { "orca_executable", "orca" } } } };
        CHECK(!remote::applyFilePolicy(cfg, up, "/s/in", &err));
        cfg = QJsonObject{ { "mdExtraParams", QJsonObject{ { "some_new_param", "/tmp/x" } } } };
        CHECK(!remote::applyFilePolicy(cfg, up, "/s/in", &err));
        cfg = QJsonObject{ { "mdExtraParams", QJsonObject{ { "some_new_param", "out.json" } } } };
        CHECK(!remote::applyFilePolicy(cfg, up, "/s/in", &err));
        cfg = QJsonObject{ { "mdExtraParams", QJsonObject{ { "thermostat", "csvr" } } } };
        CHECK(remote::applyFilePolicy(cfg, up, "/s/in", &err));
    }

    std::cout << (failures == 0 ? "All checks passed." : "FAILED") << " (" << failures << " failed)" << std::endl;
    return failures == 0 ? 0 : 1;
}
