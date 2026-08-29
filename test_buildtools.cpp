// Builder auto-hydrogen test - Claude Generated 2026
//
// Pins build::generateHydrogens: H counts from the valence accounting (double
// bonds reduce them), VSEPR placement (tetrahedral 109.47°, trigonal planar,
// bent oxygen) and the covalent H distances.

#include <QVector>
#include <QVector3D>

#include <cmath>
#include <iostream>

#include "buildtools.h"

namespace {

int g_failures = 0;

void check(bool condition, const std::string& what)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << what << std::endl;
    if (!condition)
        ++g_failures;
}

MoleculeViewer::Atom atom(const char* element, float x, float y = 0, float z = 0)
{
    MoleculeViewer::Atom a;
    a.element = QString::fromLatin1(element);
    a.position = QVector3D(x, y, z);
    return a;
}

float angleDeg(const QVector3D& a, const QVector3D& apex, const QVector3D& b)
{
    const QVector3D u = (a - apex).normalized();
    const QVector3D v = (b - apex).normalized();
    return std::acos(std::clamp(QVector3D::dotProduct(u, v), -1.0f, 1.0f)) * 180.0f / float(M_PI);
}

} // namespace

int main(int, char**)
{
    std::cout << "Builder auto-hydrogen test" << std::endl;

    // --- Methane: a bare carbon gets 4 tetrahedral H at ~1.07 A. -----------
    {
        QVector<MoleculeViewer::Atom> atoms = { atom("C", 0) };
        QVector<MoleculeViewer::Bond> bonds;
        QVector<MoleculeViewer::Atom> h;
        QVector<MoleculeViewer::Bond> hb;
        build::generateHydrogens(atoms, bonds, {}, h, hb);
        check(h.size() == 4, "methane: 4 hydrogens");
        check(hb.size() == 4, "methane: 4 C-H bonds");
        bool distOk = true;
        for (const auto& a : h)
            distOk = distOk && std::abs(a.position.length() - 1.07f) < 0.02f;
        check(distOk, "methane: C-H distance ~1.07 A");
        float minAngle = 180.0f, maxAngle = 0.0f;
        for (int i = 0; i < h.size(); ++i)
            for (int j = i + 1; j < h.size(); ++j) {
                const float ang = angleDeg(h[i].position, atoms[0].position, h[j].position);
                minAngle = std::min(minAngle, ang);
                maxAngle = std::max(maxAngle, ang);
            }
        check(std::abs(minAngle - 109.47f) < 1.0f && std::abs(maxAngle - 109.47f) < 1.0f,
            "methane: all H-C-H angles ~109.47 deg");
    }

    // --- Ethanol skeleton C-C-O: 3 + 2 + 1 hydrogens. ----------------------
    {
        QVector<MoleculeViewer::Atom> atoms = { atom("C", 0), atom("C", 1.53f), atom("O", 2.96f) };
        QVector<MoleculeViewer::Bond> bonds = { { 0, 1, 1 }, { 1, 2, 1 } };
        QVector<MoleculeViewer::Atom> h;
        QVector<MoleculeViewer::Bond> hb;
        build::generateHydrogens(atoms, bonds, {}, h, hb);
        check(h.size() == 6, "ethanol: 6 hydrogens (3+2+1)");
        int onO = 0;
        for (const auto& b : hb)
            if (b.atom1 == 2)
                ++onO;
        check(onO == 1, "ethanol: exactly 1 H on oxygen");
        // The O-H bond references the combined index (3 heavy + k).
        bool indexOk = true;
        for (const auto& b : hb)
            indexOk = indexOk && b.atom2 >= atoms.size() && b.atom2 < atoms.size() + h.size();
        check(indexOk, "ethanol: H bond indices are absolute");
        for (const auto& b : hb)
            if (b.atom1 == 2) {
                const float d = (h[b.atom2 - atoms.size()].position - atoms[2].position).length();
                check(std::abs(d - 0.97f) < 0.02f, "ethanol: O-H distance ~0.97 A");
            }
    }

    // --- Ethylene C=C: the double bond leaves 2 H per carbon, planar sp2. --
    {
        QVector<MoleculeViewer::Atom> atoms = { atom("C", 0), atom("C", 1.33f) };
        QVector<MoleculeViewer::Bond> bonds = { { 0, 1, 2 } };
        QVector<MoleculeViewer::Atom> h;
        QVector<MoleculeViewer::Bond> hb;
        build::generateHydrogens(atoms, bonds, {}, h, hb);
        check(h.size() == 4, "ethylene: 4 hydrogens");
        bool planar = true;
        for (const auto& a : h)
            planar = planar && std::abs(a.position.z()) < 1e-4f;
        check(planar, "ethylene: sp2 hydrogens stay in one plane");
        const float ang = angleDeg(atoms[1].position, atoms[0].position, h[0].position);
        check(std::abs(ang - 120.0f) < 1.0f, "ethylene: C=C-H angle ~120 deg");
    }

    // --- Acetylene-like C with a triple bond: 1 linear H. ------------------
    {
        QVector<MoleculeViewer::Atom> atoms = { atom("C", 0), atom("C", 1.20f) };
        QVector<MoleculeViewer::Bond> bonds = { { 0, 1, 3 } };
        QVector<MoleculeViewer::Atom> h;
        QVector<MoleculeViewer::Bond> hb;
        build::generateHydrogens(atoms, bonds, { 0 }, h, hb);
        check(h.size() == 1, "acetylene C: 1 hydrogen");
        const float ang = angleDeg(atoms[1].position, atoms[0].position, h[0].position);
        check(std::abs(ang - 180.0f) < 1.0f, "acetylene: C#C-H linear");
    }

    // --- Valence accounting edge cases. ------------------------------------
    {
        QVector<MoleculeViewer::Atom> atoms = { atom("Fe", 0), atom("bead1", 2) };
        QVector<MoleculeViewer::Bond> bonds;
        QVector<MoleculeViewer::Atom> h;
        QVector<MoleculeViewer::Bond> hb;
        build::generateHydrogens(atoms, bonds, {}, h, hb);
        check(h.isEmpty(), "metals and beads receive no automatic hydrogens");
        check(build::maxValence(QStringLiteral("C")) == 4, "maxValence(C) = 4");
        check(build::openValence(0, { atom("S", 0) }, {}) == 2,
            "sulfur auto-fill targets H2S, not hypervalent SF6");
    }

    // --- Hand-built rings: per-carbon H counts. ----------------------------
    {
        // Planar hexagon, all single bonds (cyclohexane connectivity).
        QVector<MoleculeViewer::Atom> atoms;
        QVector<MoleculeViewer::Bond> bonds;
        for (int i = 0; i < 6; ++i) {
            const float phi = float(i) * float(M_PI) / 3.0f;
            atoms.append(atom("C", 1.45f * std::cos(phi), 1.45f * std::sin(phi)));
            bonds.append({ i, (i + 1) % 6, 1 });
        }
        QVector<MoleculeViewer::Atom> h;
        QVector<MoleculeViewer::Bond> hb;
        build::generateHydrogens(atoms, bonds, {}, h, hb);
        check(h.size() == 12, "hexagon (single bonds): 12 hydrogens");
        QVector<int> perC(6, 0);
        for (const auto& b : hb)
            ++perC[b.atom1];
        bool twoEach = true;
        for (int c : perC)
            twoEach = twoEach && c == 2;
        check(twoEach, "hexagon: exactly 2 H per carbon");

        // Same ring with Kekule alternating orders (benzene).
        for (int i = 0; i < 6; ++i)
            bonds[i].bondOrder = (i % 2 == 0) ? 2 : 1;
        build::generateHydrogens(atoms, bonds, {}, h, hb);
        check(h.size() == 6, "benzene (Kekule): 6 hydrogens");
        // A ring carbon that already carries its H gets no second one.
        QVector<MoleculeViewer::Atom> atoms2 = atoms;
        QVector<MoleculeViewer::Bond> bonds2 = bonds;
        atoms2.append(atom("H", 2.5f, 0));
        bonds2.append({ 0, 6, 1 });
        build::generateHydrogens(atoms2, bonds2, {}, h, hb);
        check(h.size() == 5, "benzene with one existing H: only 5 more");
    }

    // --- Excess-H detection: aromatizing an already saturated ring. --------
    {
        // CH2-CH2 fragment of a saturated ring, then the C-C bond is raised to 2:
        // each carbon is now at valence 5 and must give up one H.
        QVector<MoleculeViewer::Atom> atoms = { atom("C", 0), atom("C", 1.5f) };
        QVector<MoleculeViewer::Bond> bonds = { { 0, 1, 2 } };
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 3; ++k) {
                atoms.append(atom("H", float(c) + 0.3f * (k + 1), 1.0f));
                bonds.append({ c, int(atoms.size()) - 1, 1 });
            }
        const QVector<int> excess0 = build::excessHydrogens(0, atoms, bonds);
        const QVector<int> excess1 = build::excessHydrogens(1, atoms, bonds);
        check(excess0.size() == 1 && excess1.size() == 1,
            "over-valent sp3 carbons surrender one H each");
        check(excess0.first() != excess1.first(), "each carbon gives up its own H");
        const QVector<MoleculeViewer::Atom> ok = { atom("C", 0) };
        check(build::excessHydrogens(0, ok, {}).isEmpty(),
            "no excess on an under-valent atom");
    }

    std::cout << (g_failures == 0 ? "ALL PASS" : "FAILURES") << std::endl;
    return g_failures;
}
