// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// fragmentlibrary — built-in builder fragments. Claude Generated 2026.
//
// Geometry sources: bond lengths from the covalent-radius sums used everywhere
// else in the builder (C-C 1.53, C=C aromatic 1.39, C-H 1.09, O-H 0.96,
// N-H 1.01 Å); angles are ideal VSEPR (tetrahedral 109.47°, trigonal 120°).
// The fine structure is deliberately ideal-typical — the Clean-up optimization
// relaxes it to the force field's minimum.
#include "fragmentlibrary.h"

#include "buildtools.h"

#include <QtMath>

#include <algorithm>

namespace build {

namespace {

using Atom = MoleculeViewer::Atom;
using Bond = MoleculeViewer::Bond;

Atom atom(const char* element, float x, float y, float z)
{
    Atom a;
    a.element = QString::fromLatin1(element);
    a.position = QVector3D(x, y, z);
    return a;
}

// The four tetrahedral directions (alternating cube corners); t(0) is the
// convention for a substituent's open valence.
QVector3D tet(int i)
{
    static const QVector3D dirs[4] = {
        QVector3D(1, 1, 1).normalized(), QVector3D(1, -1, -1).normalized(),
        QVector3D(-1, 1, -1).normalized(), QVector3D(-1, -1, 1).normalized()
    };
    return dirs[i];
}

// Claude Generated 2026 - Explicit attachment point (curcuma polymerbuild's
// Xx/R1 convention): a dummy "Xx" atom bonded to the attach atom fixes the
// bonding site and direction. Docking aligns the attach->Xx axis onto the
// target's free valence and consumes the Xx.
void addAttachPoint(Fragment& f, const QVector3D& direction)
{
    if (f.attachAtom < 0)
        return;
    const QVector3D base = f.atoms[f.attachAtom].position;
    Atom xx;
    xx.element = QStringLiteral("Xx");
    xx.type = QStringLiteral("R1");
    xx.position = base + direction.normalized() * 1.5f;
    f.bonds.append({ f.attachAtom, int(f.atoms.size()), 1 });
    f.atoms.append(xx);
}

// Saturate every atom except the attach atom with hydrogens (ring skeletons).
void saturate(Fragment& f)
{
    QVector<int> targets;
    for (int i = 0; i < f.atoms.size(); ++i)
        if (i != f.attachAtom)
            targets.append(i);
    QVector<Atom> h;
    QVector<Bond> hb;
    generateHydrogens(f.atoms, f.bonds, targets, h, hb);
    f.atoms += h;
    f.bonds += hb;
}

// Planar C6 ring with alternating bond orders (Kekulé), C-C 1.39 Å.
Fragment benzeneSkeleton(const QString& name, int attachAtom)
{
    Fragment f;
    f.name = name;
    f.category = (attachAtom >= 0) ? QStringLiteral("Substituents") : QStringLiteral("Molecules");
    f.attachAtom = attachAtom;
    const float r = 1.39f;
    for (int i = 0; i < 6; ++i) {
        const float phi = qDegreesToRadians(60.0f * i);
        f.atoms.append(atom("C", r * std::cos(phi), r * std::sin(phi), 0));
        f.bonds.append({ i, (i + 1) % 6, (i % 2 == 0) ? 2 : 1 });
    }
    saturate(f);
    if (attachAtom >= 0)
        addAttachPoint(f, QVector3D(1, 0, 0));  // outward at C0 = (r, 0, 0)
    return f;
}

// Chair C6 ring: alternating z = +-0.25 Å at ring radius 1.45 Å -> C-C ~1.53 Å.
Fragment cyclohexaneChair()
{
    Fragment f;
    f.name = QStringLiteral("Cyclohexane (chair)");
    f.category = QStringLiteral("Molecules");
    const float r = 1.45f;
    for (int i = 0; i < 6; ++i) {
        const float phi = qDegreesToRadians(60.0f * i);
        f.atoms.append(atom("C", r * std::cos(phi), r * std::sin(phi),
            (i % 2 == 0) ? 0.25f : -0.25f));
        f.bonds.append({ i, (i + 1) % 6, 1 });
    }
    saturate(f);
    return f;
}

// Claude Generated 2026 - Homonuclear dimer (H2, N2, O2, halogens) with the
// experimental bond length and order.
Fragment dimer(const char* element, const QString& name, float bondLength, int order)
{
    Fragment f;
    f.name = name;
    f.category = QStringLiteral("Gases");
    f.atoms.append(atom(element, -bondLength / 2, 0, 0));
    f.atoms.append(atom(element, bondLength / 2, 0, 0));
    f.bonds.append({ 0, 1, order });
    return f;
}

// Claude Generated 2026 - Flat graphene flake: honeycomb lattice points (C-C
// 1.42 Å) within a cutoff radius, single bonds from distance, then a greedy
// Kekulé matching (edge atoms first) so the interior carbons are saturated;
// the rim is H-terminated through the same generator as the Add-H button.
Fragment grapheneFlake()
{
    Fragment f;
    f.name = QStringLiteral("Graphene flake");
    f.category = QStringLiteral("Materials");
    const float a = 1.42f;
    const QVector3D a1(1.5f * a, 0.866025f * a, 0);
    const QVector3D a2(1.5f * a, -0.866025f * a, 0);
    for (int n = -3; n <= 3; ++n)
        for (int m = -3; m <= 3; ++m) {
            const QVector3D base = float(n) * a1 + float(m) * a2;
            for (const QVector3D& basis : { QVector3D(0, 0, 0), QVector3D(a, 0, 0) }) {
                const QVector3D p = base + basis;
                if (p.length() <= 4.3f)
                    f.atoms.append(atom("C", p.x(), p.y(), 0));
            }
        }
    for (int i = 0; i < f.atoms.size(); ++i)
        for (int j = i + 1; j < f.atoms.size(); ++j)
            if ((f.atoms[i].position - f.atoms[j].position).length() < 1.6f)
                f.bonds.append({ i, j, 1 });
    // Greedy Kekulé matching, atoms with the fewest neighbours first (edge
    // before interior) — on a compact flake this leaves no carbon unmatched.
    QVector<int> degree(f.atoms.size(), 0);
    for (const Bond& b : f.bonds) {
        ++degree[b.atom1];
        ++degree[b.atom2];
    }
    QVector<int> order(f.atoms.size());
    for (int i = 0; i < order.size(); ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(), [&degree](int l, int r) { return degree[l] < degree[r]; });
    QVector<bool> matched(f.atoms.size(), false);
    for (int i : order) {
        if (matched[i])
            continue;
        for (Bond& b : f.bonds) {
            const int other = (b.atom1 == i) ? b.atom2 : (b.atom2 == i) ? b.atom1 : -1;
            if (other < 0 || matched[other])
                continue;
            b.bondOrder = 2;
            matched[i] = true;
            matched[other] = true;
            break;
        }
    }
    saturate(f);  // attachAtom = -1: every rim carbon gets its hydrogens
    return f;
}

} // namespace

const QVector<Fragment>& fragmentLibrary()
{
    static const QVector<Fragment> library = []() {
        QVector<Fragment> lib;

        // --- Substituents (attachAtom 0, open valence along tet(0)). --------
        {
            Fragment f;
            f.name = QStringLiteral("Methyl  -CH3");
            f.category = QStringLiteral("Substituents");
            f.attachAtom = 0;
            f.atoms.append(atom("C", 0, 0, 0));
            for (int i = 1; i < 4; ++i) {
                f.atoms.append(Atom{ tet(i) * 1.09f, QStringLiteral("H"), 0, 0, {} });
                f.bonds.append({ 0, i, 1 });
            }
            addAttachPoint(f, tet(0));
            lib.append(f);
        }
        {
            Fragment f;
            f.name = QStringLiteral("Hydroxyl  -OH");
            f.category = QStringLiteral("Substituents");
            f.attachAtom = 0;
            f.atoms.append(atom("O", 0, 0, 0));
            f.atoms.append(Atom{ tet(1) * 0.96f, QStringLiteral("H"), 0, 0, {} });
            f.bonds.append({ 0, 1, 1 });
            addAttachPoint(f, tet(0));
            lib.append(f);
        }
        {
            Fragment f;
            f.name = QStringLiteral("Amino  -NH2");
            f.category = QStringLiteral("Substituents");
            f.attachAtom = 0;
            f.atoms.append(atom("N", 0, 0, 0));
            f.atoms.append(Atom{ tet(1) * 1.01f, QStringLiteral("H"), 0, 0, {} });
            f.atoms.append(Atom{ tet(2) * 1.01f, QStringLiteral("H"), 0, 0, {} });
            f.bonds.append({ 0, 1, 1 });
            f.bonds.append({ 0, 2, 1 });
            addAttachPoint(f, tet(0));
            lib.append(f);
        }
        {
            // Carboxyl in the xy-plane, open valence along -X: C=O up at 120°,
            // C-O-H down at 120° with the H roughly anti to the carbonyl.
            Fragment f;
            f.name = QStringLiteral("Carboxyl  -COOH");
            f.category = QStringLiteral("Substituents");
            f.attachAtom = 0;
            f.atoms.append(atom("C", 0, 0, 0));
            f.atoms.append(atom("O", 1.21f * 0.5f, 1.21f * 0.866f, 0));    // C=O
            f.atoms.append(atom("O", 1.34f * 0.5f, -1.34f * 0.866f, 0));   // C-O(H)
            f.atoms.append(atom("H", 1.34f * 0.5f + 0.90f, -1.34f * 0.866f - 0.33f, 0));
            f.bonds.append({ 0, 1, 2 });
            f.bonds.append({ 0, 2, 1 });
            f.bonds.append({ 2, 3, 1 });
            addAttachPoint(f, QVector3D(-1, 0, 0));
            lib.append(f);
        }
        lib.append(benzeneSkeleton(QStringLiteral("Phenyl  -C6H5"), 0));

        // --- Standalone molecules. ------------------------------------------
        lib.append(benzeneSkeleton(QStringLiteral("Benzene"), -1));
        lib.append(cyclohexaneChair());
        {
            Fragment f;
            f.name = QStringLiteral("Methane");
            f.category = QStringLiteral("Molecules");
            f.atoms.append(atom("C", 0, 0, 0));
            for (int i = 0; i < 4; ++i) {
                f.atoms.append(Atom{ tet(i) * 1.09f, QStringLiteral("H"), 0, 0, {} });
                f.bonds.append({ 0, i + 1, 1 });
            }
            lib.append(f);
        }
        {
            Fragment f;
            f.name = QStringLiteral("Water");
            f.category = QStringLiteral("Molecules");
            f.atoms.append(atom("O", 0, 0, 0));
            f.atoms.append(Atom{ tet(0) * 0.96f, QStringLiteral("H"), 0, 0, {} });
            f.atoms.append(Atom{ tet(1) * 0.96f, QStringLiteral("H"), 0, 0, {} });
            f.bonds.append({ 0, 1, 1 });
            f.bonds.append({ 0, 2, 1 });
            lib.append(f);
        }
        {
            Fragment f;
            f.name = QStringLiteral("Ammonia");
            f.category = QStringLiteral("Molecules");
            f.atoms.append(atom("N", 0, 0, 0));
            for (int i = 0; i < 3; ++i) {
                f.atoms.append(Atom{ tet(i) * 1.01f, QStringLiteral("H"), 0, 0, {} });
                f.bonds.append({ 0, i + 1, 1 });
            }
            lib.append(f);
        }
        // --- Gases (dimers, experimental bond lengths) ----------------------
        lib.append(dimer("H", QStringLiteral("H2"), 0.74f, 1));
        lib.append(dimer("N", QStringLiteral("N2"), 1.10f, 3));
        lib.append(dimer("O", QStringLiteral("O2"), 1.21f, 2));
        lib.append(dimer("F", QStringLiteral("F2"), 1.42f, 1));
        lib.append(dimer("Cl", QStringLiteral("Cl2"), 1.99f, 1));
        lib.append(dimer("Br", QStringLiteral("Br2"), 2.28f, 1));
        lib.append(dimer("I", QStringLiteral("I2"), 2.67f, 1));

        // --- Materials ------------------------------------------------------
        lib.append(grapheneFlake());

        return lib;
    }();
    return library;
}

} // namespace build
