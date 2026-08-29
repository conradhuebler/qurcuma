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

} // namespace

const QVector<Fragment>& fragmentLibrary()
{
    static const QVector<Fragment> library = []() {
        QVector<Fragment> lib;

        // --- Substituents (attachAtom 0, open valence along tet(0)). --------
        {
            Fragment f;
            f.name = QStringLiteral("Methyl  -CH3");
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
            f.atoms.append(atom("N", 0, 0, 0));
            for (int i = 0; i < 3; ++i) {
                f.atoms.append(Atom{ tet(i) * 1.01f, QStringLiteral("H"), 0, 0, {} });
                f.bonds.append({ 0, i + 1, 1 });
            }
            lib.append(f);
        }
        return lib;
    }();
    return library;
}

} // namespace build
