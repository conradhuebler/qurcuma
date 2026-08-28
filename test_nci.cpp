// NCI detection test - Claude Generated 2026
// Pins the geometric criteria in src/ncianalysis.cpp against hand-built geometries:
// hydrogen bond distance/angle gate, halogen-bond sigma-hole gate (and fluorine's
// exclusion from it), the 1-3 exclusion, and parallel pi-stacking.

#include <QCoreApplication>
#include <QVector>
#include <QVector3D>

#include <cmath>
#include <iostream>

#include "ncianalysis.h"

namespace {

int g_failures = 0;

void check(bool condition, const std::string& what)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << what << std::endl;
    if (!condition)
        ++g_failures;
}

MoleculeViewer::Atom atom(const char* element, float x, float y, float z)
{
    MoleculeViewer::Atom a;
    a.element = QString::fromLatin1(element);
    a.position = QVector3D(x, y, z);
    return a;
}

MoleculeViewer::Bond bond(int i, int j)
{
    MoleculeViewer::Bond b;
    b.atom1 = i;
    b.atom2 = j;
    b.bondOrder = 1;
    return b;
}

/// Place an acceptor so that d(bridge...acceptor) = distance and the angle
/// donor-bridge...acceptor equals the requested value.
QVector3D acceptorAt(const QVector3D& donor, const QVector3D& bridge, float distance, float angleDeg)
{
    // The arm bridge->donor points backwards; rotating it by (180 - angle) in the
    // xy-plane and scaling to `distance` gives the requested angle at the bridge.
    const QVector3D back = (donor - bridge).normalized();
    const double turn = (180.0 - angleDeg) * M_PI / 180.0;
    const double cs = std::cos(turn);
    const double sn = std::sin(turn);
    // Rotate -back by `turn` around z.
    const QVector3D forward = -back;
    const double x = forward.x() * cs - forward.y() * sn;
    const double y = forward.x() * sn + forward.y() * cs;
    return bridge + QVector3D(static_cast<float>(x), static_cast<float>(y), forward.z()) * distance;
}

/// Planar hexagon of carbons (benzene-like) in the xy-plane, centred at `centre`.
void appendRing(QVector<MoleculeViewer::Atom>& atoms, QVector<MoleculeViewer::Bond>& bonds,
    const QVector3D& centre)
{
    const int first = atoms.size();
    const float r = 1.39f;
    for (int k = 0; k < 6; ++k) {
        const double phi = k * M_PI / 3.0;
        atoms.append(atom("C", centre.x() + r * static_cast<float>(std::cos(phi)),
            centre.y() + r * static_cast<float>(std::sin(phi)), centre.z()));
    }
    for (int k = 0; k < 6; ++k)
        bonds.append(bond(first + k, first + (k + 1) % 6));
}

void testHydrogenBond()
{
    std::cout << "Hydrogen bond D-H...A" << std::endl;

    QVector<MoleculeViewer::Atom> atoms;
    atoms.append(atom("O", 0.00f, 0.00f, 0.00f));   // 0 donor
    atoms.append(atom("H", 0.96f, 0.00f, 0.00f));   // 1 bridging hydrogen
    atoms.append(atom("H", -0.24f, 0.93f, 0.00f));  // 2 second hydrogen
    QVector<MoleculeViewer::Bond> bonds { bond(0, 1), bond(0, 2) };

    nci::Options opt;
    opt.piStacking = false;

    // 1.95 A / 175 deg -> a textbook moderate hydrogen bond.
    QVector<MoleculeViewer::Atom> linear = atoms;
    linear.append(atom("O", 0, 0, 0));
    linear[3].position = acceptorAt(atoms[0].position, atoms[1].position, 1.95f, 175.0f);

    QVector<nci::Contact> found = nci::detectGeometric(linear, bonds, opt);
    check(found.size() == 1, "1.95 A / 175 deg yields exactly one contact");
    if (found.size() == 1) {
        const nci::Contact& c = found.first();
        check(c.kind == nci::Kind::HydrogenBond, "classified as a hydrogen bond");
        check(c.donor == 0 && c.bridge == 1 && c.acceptor == 3, "donor/bridge/acceptor indices");
        check(std::fabs(c.distance - 1.95f) < 1e-3f, "H...A distance reproduced");
        check(std::fabs(c.angle - 175.0f) < 0.1f, "D-H...A angle reproduced");
        check(c.score > 0.0f && c.score <= 1.0f, "score inside [0,1]");
    }

    // Same distance, bent to 120 deg -> below the 130 deg gate.
    QVector<MoleculeViewer::Atom> bent = atoms;
    bent.append(atom("O", 0, 0, 0));
    bent[3].position = acceptorAt(atoms[0].position, atoms[1].position, 1.95f, 120.0f);
    check(nci::detectGeometric(bent, bonds, opt).isEmpty(), "120 deg is rejected by the angle gate");

    // Linear but too far away.
    QVector<MoleculeViewer::Atom> distant = atoms;
    distant.append(atom("O", 0, 0, 0));
    distant[3].position = acceptorAt(atoms[0].position, atoms[1].position, 3.20f, 175.0f);
    check(nci::detectGeometric(distant, bonds, opt).isEmpty(), "3.20 A is rejected by the distance gate");
}

void testGeminalExclusion()
{
    std::cout << "1-3 exclusion (geminal H...H in methane)" << std::endl;

    const float d = 1.09f / std::sqrt(3.0f);
    QVector<MoleculeViewer::Atom> atoms;
    atoms.append(atom("C", 0, 0, 0));
    atoms.append(atom("H", d, d, d));
    atoms.append(atom("H", -d, -d, d));
    atoms.append(atom("H", -d, d, -d));
    atoms.append(atom("H", d, -d, -d));
    QVector<MoleculeViewer::Bond> bonds { bond(0, 1), bond(0, 2), bond(0, 3), bond(0, 4) };

    nci::Options opt;
    opt.piStacking = false;
    opt.closeContacts = true;
    opt.includeHH = true;

    const float hh = (atoms[1].position - atoms[2].position).length();
    check(hh < 0.90f * (2.0f * 1.10f), "geminal H...H is inside the close-contact cutoff");
    check(nci::detectGeometric(atoms, bonds, opt).isEmpty(), "geminal pairs are excluded as 1-3");
}

void testHalogenBond()
{
    std::cout << "Halogen bond C-X...A" << std::endl;

    nci::Options opt;
    opt.piStacking = false;

    const QVector3D carbon(0, 0, 0);
    const QVector3D halogen(1.76f, 0, 0);

    QVector<MoleculeViewer::Bond> bonds { bond(0, 1) };

    QVector<MoleculeViewer::Atom> chlorine;
    chlorine.append(atom("C", carbon.x(), carbon.y(), carbon.z()));
    chlorine.append(atom("Cl", halogen.x(), halogen.y(), halogen.z()));
    chlorine.append(atom("N", 0, 0, 0));
    chlorine[2].position = acceptorAt(carbon, halogen, 3.00f, 175.0f);

    QVector<nci::Contact> found = nci::detectGeometric(chlorine, bonds, opt);
    check(found.size() == 1, "C-Cl...N at 3.00 A / 175 deg yields one contact");
    if (found.size() == 1) {
        check(found.first().kind == nci::Kind::HalogenBond, "classified as a halogen bond");
        check(found.first().bridge == 1 && found.first().acceptor == 2, "halogen/acceptor indices");
    }

    // 90 deg: no sigma-hole in that direction.
    QVector<MoleculeViewer::Atom> perpendicular = chlorine;
    perpendicular[2].position = acceptorAt(carbon, halogen, 3.00f, 90.0f);
    check(nci::detectGeometric(perpendicular, bonds, opt).isEmpty(),
        "90 deg is rejected by the sigma-hole gate");

    // Fluorine has no sigma-hole and is not a halogen-bond donor.
    QVector<MoleculeViewer::Atom> fluorine = chlorine;
    fluorine[1].element = QStringLiteral("F");
    fluorine[2].position = acceptorAt(carbon, halogen, 2.60f, 175.0f);
    check(nci::detectGeometric(fluorine, bonds, opt).isEmpty(), "fluorine is not a halogen-bond donor");
}

void testPiStacking()
{
    std::cout << "Pi stacking between planar six-rings" << std::endl;

    nci::Options opt;
    opt.hydrogenBonds = false;
    opt.halogenBonds = false;

    QVector<MoleculeViewer::Atom> atoms;
    QVector<MoleculeViewer::Bond> bonds;
    appendRing(atoms, bonds, QVector3D(0, 0, 0));
    appendRing(atoms, bonds, QVector3D(1.5f, 0, 3.7f));

    const QVector<QVector<int>> rings = nci::findAromaticRings(atoms.size(), bonds);
    check(rings.size() == 2, "ring perception finds both six-rings");

    QVector<nci::Contact> found = nci::detectGeometric(atoms, bonds, opt, &rings);
    check(found.size() == 1, "parallel-displaced rings yield one contact");
    if (found.size() == 1) {
        const nci::Contact& c = found.first();
        check(c.kind == nci::Kind::PiStacking, "classified as pi stacking");
        check(std::fabs(c.angle) < 1.0f, "interplanar angle is zero for parallel rings");
        check(std::fabs(c.offset - 1.5f) < 1e-2f, "lateral offset reproduced");
        check(c.ringA.size() == 6 && c.ringB.size() == 6, "both rings carried in the contact");
    }

    // Pulled apart beyond the centroid cutoff.
    QVector<MoleculeViewer::Atom> distant;
    QVector<MoleculeViewer::Bond> distantBonds;
    appendRing(distant, distantBonds, QVector3D(0, 0, 0));
    appendRing(distant, distantBonds, QVector3D(0, 0, 8.0f));
    const QVector<QVector<int>> distantRings = nci::findAromaticRings(distant.size(), distantBonds);
    check(nci::detectGeometric(distant, distantBonds, opt, &distantRings).isEmpty(),
        "8 A separation is rejected by the centroid cutoff");
}

void testRadii()
{
    std::cout << "Van der Waals radii" << std::endl;
    check(std::fabs(nci::vdwRadius(QStringLiteral("H")) - 1.10) < 1e-6, "H = 1.10 A");
    check(std::fabs(nci::vdwRadius(QStringLiteral("C")) - 1.70) < 1e-6, "C = 1.70 A");
    check(std::fabs(nci::vdwRadius(QStringLiteral("Cl")) - 1.75) < 1e-6, "Cl = 1.75 A");
    check(nci::vdwRadius(QStringLiteral("ppo1")) == 0.0, "coarse-grained bead labels resolve to 0");
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "NCI detection test" << std::endl;
    std::cout << "==================" << std::endl;

    testRadii();
    testHydrogenBond();
    testGeminalExclusion();
    testHalogenBond();
    testPiStacking();

    std::cout << std::endl;
    if (g_failures == 0) {
        std::cout << "All checks passed." << std::endl;
        return 0;
    }
    std::cout << g_failures << " check(s) failed." << std::endl;
    return 1;
}
