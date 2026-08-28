// NCI GFN-FF integration test - Claude Generated 2026
//
// Runs NciAnalysisWorker::analyse() on a real structure and checks that the
// curcuma path actually delivers: EnergyCalculator -> GFNFFComputationalMethod ->
// GFNFF::generateGFNFFParameterSet() -> hydrogen/halogen bond lists and charges.
// This pins the curcuma API contract the analysis depends on; the geometric
// criteria themselves are covered by test_nci.

#include <QCoreApplication>
#include <QFile>
#include <QVector>

#include <cmath>
#include <iostream>

#include "ncianalysisworker.h"
#include "ncianalysis.h"
#include "elementdata.h"
#include "xyzparser.h"

namespace {
int g_failures = 0;

/// XYZ files carry no bonds and XYZParser leaves the detection to the viewer, so
/// the test reproduces the viewer's criterion (sum of covalent radii x 1.25) to
/// exercise the worker's 1-2/1-3 filter with a real topology.
QVector<MoleculeViewer::Bond> detectBonds(const QVector<MoleculeViewer::Atom>& atoms)
{
    QVector<MoleculeViewer::Bond> bonds;
    for (int i = 0; i < atoms.size(); ++i) {
        for (int j = i + 1; j < atoms.size(); ++j) {
            const double limit = 1.25
                * (elem::covalentRadius(atoms[i].element) + elem::covalentRadius(atoms[j].element));
            if ((atoms[j].position - atoms[i].position).length() <= limit)
                bonds.append({ i, j, 1 });
        }
    }
    return bonds;
}

void check(bool condition, const std::string& what)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << what << std::endl;
    if (!condition)
        ++g_failures;
}
} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QString path = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                  : QStringLiteral("release/conf_28.xyz");
    if (!QFile::exists(path)) {
        std::cerr << "Structure not found: " << path.toStdString() << std::endl;
        std::cerr << "Pass an .xyz file as the first argument." << std::endl;
        return 77;  // skip: no fixture
    }

    XYZParser parser;
    XYZParser::XYZFrame xyz;
    if (!parser.parseFile(path, xyz)) {
        std::cerr << "Failed to parse " << path.toStdString() << std::endl;
        return 1;
    }
    QVector<MoleculeViewer::Atom> atoms;
    QVector<MoleculeViewer::Bond> bonds;
    XYZParser::convertToMoleculeViewer(xyz, atoms, bonds);
    bonds = detectBonds(atoms);

    std::cout << "NCI GFN-FF integration test" << std::endl;
    std::cout << "===========================" << std::endl;
    std::cout << "Structure: " << path.toStdString() << " (" << atoms.size() << " atoms, "
              << bonds.size() << " bonds)" << std::endl;

    NciAnalysisWorker worker;

    nci::Result result;
    bool gotResult = false;
    QVector<float> charges;
    bool gotCharges = false;
    QString error;

    QObject::connect(&worker, &NciAnalysisWorker::resultReady,
        [&](quint64, const nci::Result& r) { result = r; gotResult = true; });
    QObject::connect(&worker, &NciAnalysisWorker::chargesReady,
        [&](quint64, int, const QVector<float>& q) { charges = q; gotCharges = true; });
    QObject::connect(&worker, &NciAnalysisWorker::errorOccurred,
        [&](const QString& m) { error = m; });

    NciAnalysisWorker::Request request;
    request.atoms = atoms;
    request.bonds = bonds;
    request.method = QStringLiteral("gfnff");
    request.requestId = 1;
    request.options.electrostatics = true;
    request.options.dispersion = true;

    worker.analyse(request);  // direct call: no thread needed for the test

    if (!error.isEmpty())
        std::cout << "  worker error: " << error.toStdString() << std::endl;

    check(gotResult, "worker delivered a result");
    check(error.isEmpty(), "no error reported");
    check(gotCharges, "EEQ charges delivered");
    check(charges.size() == atoms.size(), "one charge per atom");

    if (gotCharges) {
        double sum = 0.0;
        for (float q : charges)
            sum += q;
        // The structure is neutral, so the EEQ charges must sum to zero.
        check(std::fabs(sum) < 1e-3, "charges sum to the total charge (neutral)");
    }

    int hb = 0, xb = 0, es = 0, disp = 0;
    for (const nci::Contact& c : result.contacts) {
        switch (c.kind) {
        case nci::Kind::HydrogenBond: ++hb; break;
        case nci::Kind::HalogenBond: ++xb; break;
        case nci::Kind::Electrostatic: ++es; break;
        case nci::Kind::Dispersion: ++disp; break;
        default: break;
        }
    }
    std::cout << "  contacts: " << hb << " H-bonds, " << xb << " halogen bonds, " << es
              << " electrostatic, " << disp << " dispersion" << std::endl;
    std::cout << "  summary: " << result.summary.toStdString() << std::endl;

    check(result.source == nci::Source::GfnffParameters, "result is tagged as the GFN-FF source");
    check(hb > 0, "GFN-FF reports at least one hydrogen bond for this structure");
    // GFN-FF enumerates thousands of candidate triples and lets its damping decide;
    // the analysis gates them down to the ones engaged in this frame. Without that
    // gate this structure yields >5000 "hydrogen bonds".
    check(hb < 100, "the candidate list is gated down to engaged hydrogen bonds");
    check(result.contacts.size() <= request.options.maxContacts, "the hard cap is honoured");
    check(!result.summary.isEmpty(), "summary line filled");

    // The same structure through the geometric criteria. The two lists are not
    // expected to match: the geometric source restricts donors to N, O, F, S,
    // while GFN-FF also parameterises weaker C-H...O type bridges. What must hold
    // is that they overlap - a zero overlap would mean the {donor, bridge,
    // acceptor} index convention differs between the two sources.
    nci::Options geoOptions = request.options;
    geoOptions.piStacking = false;
    const QVector<nci::Contact> geometric = nci::detectGeometric(atoms, bonds, geoOptions);
    int geoHb = 0;
    int overlap = 0;
    for (const nci::Contact& g : geometric) {
        if (g.kind != nci::Kind::HydrogenBond)
            continue;
        ++geoHb;
        for (const nci::Contact& c : result.contacts) {
            if (c.kind == nci::Kind::HydrogenBond && c.bridge == g.bridge
                && c.acceptor == g.acceptor) {
                ++overlap;
                break;
            }
        }
    }
    std::cout << "  geometric source: " << geoHb << " H-bonds, " << overlap
              << " of them also in the GFN-FF list" << std::endl;
    check(geoHb > 0, "the geometric source also finds hydrogen bonds here");
    check(overlap > 0, "both sources use the same donor/bridge/acceptor convention");

    // Ring perception on a real structure: curcuma's Topology::FindRings is the
    // reused implementation behind pi-stacking, so run it on this molecule and
    // make sure it terminates and returns sane rings rather than nothing.
    const QVector<QVector<int>> rings = nci::findAromaticRings(atoms.size(), bonds);
    std::cout << "  ring perception: " << rings.size() << " five/six-rings" << std::endl;
    bool ringsSane = true;
    for (const QVector<int>& ring : rings) {
        if (ring.size() < 5 || ring.size() > 6)
            ringsSane = false;
        for (int idx : ring)
            if (idx < 0 || idx >= atoms.size())
                ringsSane = false;
    }
    check(ringsSane, "ring perception returns five/six-rings with valid indices");

    nci::Options piOptions = request.options;
    piOptions.piStacking = true;
    const QVector<nci::Contact> withPi = nci::detectGeometric(atoms, bonds, piOptions, &rings);
    check(withPi.size() >= geometric.size(), "enabling pi stacking never loses contacts");

    bool indicesValid = true;
    bool geometryFitted = true;
    for (const nci::Contact& c : result.contacts) {
        if (c.donor < 0 || c.donor >= atoms.size() || c.acceptor < 0
            || c.acceptor >= atoms.size() || c.bridge >= atoms.size())
            indicesValid = false;
        if (c.distance <= 0.0f)
            geometryFitted = false;
    }
    check(indicesValid, "all contact atom indices are inside the structure");
    check(geometryFitted, "every contact carries a fitted distance");

    bool energiesOnPairs = true;
    for (const nci::Contact& c : result.contacts) {
        const bool isPairTerm = c.kind == nci::Kind::Electrostatic
            || c.kind == nci::Kind::Dispersion;
        // Only the closed-form pair terms carry an energy; GFN-FF keeps no
        // per-contact energy for hydrogen and halogen bonds.
        if (c.hasEnergy != isPairTerm)
            energiesOnPairs = false;
    }
    check(energiesOnPairs, "only electrostatic/dispersion rows carry a pair energy");

    std::cout << std::endl;
    if (g_failures == 0) {
        std::cout << "All checks passed." << std::endl;
        return 0;
    }
    std::cout << g_failures << " check(s) failed." << std::endl;
    return 1;
}
