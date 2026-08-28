// ncianalysisworker.cpp - Off-thread NCI analysis from curcuma calculations
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "ncianalysisworker.h"

#include "moleculebridge.h"
#include "ncianalysis.h"

#include <src/core/energy_calculators/ff_methods/gfnff.h>
#include <src/core/energy_calculators/ff_methods/gfnff_parameters.h>
#include <src/core/energy_calculators/qm_methods/gfnff_method.h>
#include <src/core/energycalculator.h>
#include <src/core/global.h>
#include <src/core/molecule.h>
#include <src/core/units.h>

#include <algorithm>
#include <cmath>

using json = nlohmann::json;

namespace {

constexpr double kHartreeToKJ = CurcumaUnit::Energy::HARTREE_TO_KJMOL;
const double kAngstromPerBohr = au;  // curcuma's global.h, 0.52917721092

/// Pair Coulomb energy of the GFN-FF electrostatic term, in kJ/mol.
///
/// E = q_i q_j erf(gamma_ij r) / r, with r in Bohr (mirrors
/// FFWorkspace::calcCoulomb in ff_workspace_gfnff.cpp). The kernel prefers the
/// dynamic EEQ charges over the ones stored in the pair, so this does too.
/// Claude Generated.
double coulombPairEnergy(const GFNFFCoulomb& pair, double distanceAngstrom,
    const Eigen::VectorXd& eeqCharges)
{
    const double r = distanceAngstrom / kAngstromPerBohr;
    if (r < 1e-10 || r > pair.r_cut)
        return 0.0;

    double qi = pair.q_i;
    double qj = pair.q_j;
    if (eeqCharges.size() > pair.i && eeqCharges.size() > pair.j) {
        const double qiDyn = eeqCharges(pair.i);
        const double qjDyn = eeqCharges(pair.j);
        if (!std::isnan(qiDyn) && !std::isnan(qjDyn)) {
            qi = qiDyn;
            qj = qjDyn;
        }
    }
    return qi * qj * std::erf(pair.gamma_ij * r) / r * kHartreeToKJ;
}

/// Pair dispersion energy of the GFN-FF D4 term, in kJ/mol.
///
/// E = -C6 * zeta * (1/(r^6 + R0^6) + 2 * r4r2ij / (r^8 + R0^8)), r in Bohr
/// (mirrors FFWorkspace::calcDispersion in ff_workspace_gfnff.cpp).
/// Claude Generated.
double dispersionPairEnergy(const GFNFFDispersion& pair, double distanceAngstrom)
{
    const double r = distanceAngstrom / kAngstromPerBohr;
    if (r < 1e-8 || r > pair.r_cut)
        return 0.0;

    const double r2 = r * r;
    const double r6 = r2 * r2 * r2;
    const double r8 = r6 * r2;
    const double r06 = pair.r0_squared * pair.r0_squared * pair.r0_squared;
    const double r08 = r06 * pair.r0_squared;
    const double sum = 1.0 / (r6 + r06) + 2.0 * pair.r4r2ij / (r8 + r08);
    return -pair.C6 * sum * pair.zetac6 * kHartreeToKJ;
}

float distanceBetween(const QVector<MoleculeViewer::Atom>& atoms, int i, int j)
{
    if (i < 0 || j < 0 || i >= atoms.size() || j >= atoms.size())
        return 0.0f;
    return (atoms[j].position - atoms[i].position).length();
}

/// Bonded and 1-3 pairs are covalent bookkeeping, not non-covalent contacts.
/// The GFN-FF dispersion and Coulomb pair lists contain every pair, so this
/// filter is mandatory there.
bool topologicallyClose(const QVector<QVector<int>>& adjacency, int i, int j)
{
    if (i == j)
        return true;
    if (i < 0 || j < 0 || i >= adjacency.size() || j >= adjacency.size())
        return false;
    if (adjacency[i].contains(j))
        return true;
    for (int nb : adjacency[i])
        if (nb >= 0 && nb < adjacency.size() && adjacency[nb].contains(j))
            return true;
    return false;
}

} // namespace

NciAnalysisWorker::NciAnalysisWorker(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<NciAnalysisWorker::Request>("NciAnalysisWorker::Request");
    qRegisterMetaType<nci::Result>("nci::Result");
}

void NciAnalysisWorker::analyse(NciAnalysisWorker::Request request)
{
    if (request.atoms.isEmpty()) {
        emit errorOccurred(tr("No structure to analyse."));
        return;
    }

    nci::Result result;
    result.frame = request.frame;
    result.source = request.method == QLatin1String("gfnff") ? nci::Source::GfnffParameters
                                                             : nci::Source::Population;

    // Adjacency of the displayed bond graph, for the 1-2/1-3 filter below.
    QVector<QVector<int>> adjacency(request.atoms.size());
    for (const MoleculeViewer::Bond& b : request.bonds) {
        if (b.atom1 >= 0 && b.atom1 < adjacency.size() && b.atom2 >= 0
            && b.atom2 < adjacency.size()) {
            adjacency[b.atom1].append(b.atom2);
            adjacency[b.atom2].append(b.atom1);
        }
    }

    try {
        curcuma::Molecule mol = atomsToMolecule(request.atoms);

        json controller;
        controller["method"] = request.method.toStdString();
        controller["gpu"] = "none";
        controller["verbosity"] = 0;
        controller["threads"] = 1;

        EnergyCalculator calc(request.method.toStdString(), controller);
        calc.setMolecule(mol.getMolInfo());
        calc.CalculateEnergy(false);

        // Charges: EEQ for GFN-FF, Mulliken for GFN2/GFN1. Both feed the viewer's
        // "By Charge" colour scheme, so this is delivered for every method.
        const Vector charges = calc.Charges();
        if (charges.size() == request.atoms.size()) {
            QVector<float> q;
            q.reserve(int(charges.size()));
            for (int i = 0; i < charges.size(); ++i)
                q.append(static_cast<float>(charges(i)));
            emit chargesReady(request.requestId, request.frame, q);
        }

        if (request.method == QLatin1String("gfnff")) {
            auto* method = dynamic_cast<GFNFFComputationalMethod*>(calc.Interface());
            GFNFF* ff = method ? method->getGFNFF() : nullptr;
            if (!ff) {
                emit errorOccurred(tr("GFN-FF parameters are not available for this structure."));
                return;
            }

            // generateGFNFFParameterSet() returns a fresh set. consumeCachedParameterSet()
            // would move the force field's own cached set away from it.
            const GFNFFParameterSet params = ff->generateGFNFFParameterSet();

            for (const GFNFFHydrogenBond& hb : params.hbonds) {
                nci::Contact c;
                c.kind = nci::Kind::HydrogenBond;
                c.donor = hb.i;    // heavy donor A
                c.bridge = hb.j;   // hydrogen
                c.acceptor = hb.k; // acceptor B
                c.motif = hb.case_type;
                result.contacts.append(c);
            }
            for (const GFNFFHalogenBond& xb : params.xbonds) {
                nci::Contact c;
                c.kind = nci::Kind::HalogenBond;
                c.donor = xb.i;    // carrier A
                c.bridge = xb.j;   // halogen X
                c.acceptor = xb.k; // acceptor B
                result.contacts.append(c);
            }

            // Electrostatics and dispersion carry a real per-pair energy, so they
            // are only worth showing above a threshold - otherwise every atom pair
            // in the molecule turns into a line.
            if (request.options.electrostatics) {
                constexpr double kMinCoulomb = 2.0; // kJ/mol
                for (const GFNFFCoulomb& pair : params.coulombs) {
                    if (topologicallyClose(adjacency, pair.i, pair.j))
                        continue;
                    const float d = distanceBetween(request.atoms, pair.i, pair.j);
                    const double e = coulombPairEnergy(pair, d, params.eeq_charges);
                    if (std::fabs(e) < kMinCoulomb)
                        continue;
                    nci::Contact c;
                    c.kind = nci::Kind::Electrostatic;
                    c.donor = pair.i;
                    c.acceptor = pair.j;
                    c.distance = d;
                    c.energy = e;
                    c.hasEnergy = true;
                    c.score = float(std::min(1.0, std::fabs(e) / 40.0));
                    result.contacts.append(c);
                }
            }
            if (request.options.dispersion) {
                constexpr double kMinDispersion = 0.5; // kJ/mol
                for (const GFNFFDispersion& pair : params.dispersions) {
                    if (topologicallyClose(adjacency, pair.i, pair.j))
                        continue;
                    const float d = distanceBetween(request.atoms, pair.i, pair.j);
                    const double e = dispersionPairEnergy(pair, d);
                    if (std::fabs(e) < kMinDispersion)
                        continue;
                    nci::Contact c;
                    c.kind = nci::Kind::Dispersion;
                    c.donor = pair.i;
                    c.acceptor = pair.j;
                    c.distance = d;
                    c.energy = e;
                    c.hasEnergy = true;
                    c.score = float(std::min(1.0, std::fabs(e) / 5.0));
                    result.contacts.append(c);
                }
            }

            // The hydrogen- and halogen-bond geometry is recomputed here rather than
            // read from the force field: GFN-FF reduces those energies to a single
            // scalar per system, so the honest per-contact numbers are the distance
            // and the angle. The system totals go into the summary line below.
            nci::refreshGeometry(result.contacts, request.atoms, request.options);
            // GFN-FF's lists are candidate enumerations (thousands of triples for a
            // medium molecule), so keep the ones geometrically engaged in this frame.
            nci::applyGeometricGate(result.contacts, request.atoms, request.options);

            // The two pair terms are numerous by nature. Budget them separately so
            // they cannot crowd the directional interactions - or each other - out
            // of the overall cap.
            const int pairBudget = std::max(1, request.options.maxContacts / 4);
            int dropped = nci::limitKind(result.contacts, nci::Kind::Electrostatic, pairBudget);
            dropped += nci::limitKind(result.contacts, nci::Kind::Dispersion, pairBudget);
            dropped += nci::rankAndTruncate(result.contacts, request.options);

            result.summary = nci::summarize(result.contacts, result.source)
                + nci::truncationNote(dropped);
            const json decomposition = calc.getEnergyDecomposition();
            if (decomposition.contains("HBond") && decomposition.contains("XBond")) {
                const double eHb = decomposition["HBond"].get<double>() * kHartreeToKJ;
                const double eXb = decomposition["XBond"].get<double>() * kHartreeToKJ;
                result.summary += tr(" - E(H-bonds) = %1 kJ/mol, E(halogen bonds) = %2 kJ/mol")
                                      .arg(eHb, 0, 'f', 1)
                                      .arg(eXb, 0, 'f', 1);
            }
        } else {
            // Population analysis: the charges above are the payload. Contacts still
            // come from the geometry so the overlay stays populated.
            result.contacts = nci::detectGeometric(request.atoms, request.bonds, request.options);
            result.summary = nci::summarize(result.contacts, result.source);
        }
    } catch (const std::exception& e) {
        emit errorOccurred(tr("Analysis failed: %1").arg(QString::fromUtf8(e.what())));
        return;
    } catch (...) {
        emit errorOccurred(tr("Analysis failed."));
        return;
    }

    emit resultReady(request.requestId, result);
}
