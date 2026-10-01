// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 — packing rules of build::fillContainer, without a GUI.

#include "scenefiller.h"

#include <QCoreApplication>
#include <QtMath>
#include <cmath>
#include <cstdio>

static int g_failed = 0;
static void check(bool ok, const QString& what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
    if (!ok)
        ++g_failed;
}

static const build::Fragment* byName(const QString& name)
{
    for (const build::Fragment& f : build::fragmentLibrary())
        if (f.name.compare(name, Qt::CaseInsensitive) == 0)
            return &f;
    return nullptr;
}

// Smallest distance between any two atoms of DIFFERENT copies. Atoms inside one
// copy keep their template geometry (a bond is shorter than minDistance), so the
// packing rule only applies across copies.
static float minInterCopyDistance(const build::FillResult& r, int atomsPerCopy)
{
    float best = 1e9f;
    for (int i = 0; i < r.atoms.size(); ++i) {
        for (int j = i + 1; j < r.atoms.size(); ++j) {
            if (i / atomsPerCopy == j / atomsPerCopy)
                continue;
            best = std::min(best, (r.atoms[i].position - r.atoms[j].position).length());
        }
    }
    return best;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    const build::Fragment* n2 = byName("N2");
    const build::Fragment* h2 = byName("H2");
    if (!n2 || !h2) {
        std::printf("  FAIL  fragment library must provide N2 and H2\n");
        return 1;
    }
    check(n2->atoms.size() == 2 && n2->bonds.size() == 1 && n2->bonds[0].bondOrder == 3,
        "N2 template: 2 atoms, one triple bond");

    // --- Haber-Bosch scene: 2 N2 + 6 H2 in a 4.5 A sphere -------------------
    build::Container sphere;
    sphere.kind = build::Container::Sphere;
    sphere.radius = 4.5f;
    const QVector<build::FillRequest> req { { n2, 2 }, { h2, 6 } };

    const build::FillResult r = build::fillContainer(req, sphere, {}, 2.2f, 4000, 42);
    check(r.requested == 8, QString("8 copies requested (got %1)").arg(r.requested));
    check(r.placed == 8, QString("all 8 copies placed (got %1)").arg(r.placed));
    check(r.atoms.size() == 16, QString("16 atoms (got %1)").arg(r.atoms.size()));
    check(r.bonds.size() == 8, QString("8 bonds (got %1)").arg(r.bonds.size()));

    bool inside = true, indices_ok = true, order3 = false;
    for (const MoleculeViewer::Atom& a : r.atoms)
        inside = inside && (a.position.length() <= sphere.radius - sphere.margin + 1e-4f);
    check(inside, "every atom lies inside the sphere, margin respected");

    for (const MoleculeViewer::Bond& b : r.bonds) {
        indices_ok = indices_ok && b.atom1 >= 0 && b.atom2 >= 0
            && b.atom1 < r.atoms.size() && b.atom2 < r.atoms.size() && b.atom1 != b.atom2;
        if (b.bondOrder == 3)
            order3 = true;
    }
    check(indices_ok, "bond indices are re-based into the merged atom list");
    check(order3, "the N2 triple bond order survives the fill");

    const float dmin = minInterCopyDistance(r, 2);
    check(dmin >= 2.2f - 1e-3f,
        QString("copies keep the 2.2 A minimum distance (closest %1 A)").arg(dmin, 0, 'f', 2));

    // --- Reproducibility ----------------------------------------------------
    const build::FillResult r2 = build::fillContainer(req, sphere, {}, 2.2f, 4000, 42);
    bool same = (r2.atoms.size() == r.atoms.size());
    for (int i = 0; same && i < r.atoms.size(); ++i)
        same = (r.atoms[i].position - r2.atoms[i].position).length() < 1e-6f;
    check(same, "the same seed reproduces the same packing");

    const build::FillResult r3 = build::fillContainer(req, sphere, {}, 2.2f, 4000, 7);
    bool differs = (r3.atoms.size() != r.atoms.size());
    for (int i = 0; !differs && i < r.atoms.size(); ++i)
        differs = (r.atoms[i].position - r3.atoms[i].position).length() > 1e-3f;
    check(differs, "a different seed gives a different packing");

    // --- Existing atoms are respected --------------------------------------
    QVector<MoleculeViewer::Atom> existing;
    MoleculeViewer::Atom centre;
    centre.element = "C";
    centre.position = QVector3D(0, 0, 0);
    existing.append(centre);
    const build::FillResult r4 = build::fillContainer({ { h2, 4 } }, sphere, existing, 2.2f, 4000, 3);
    bool clearsExisting = true;
    for (const MoleculeViewer::Atom& a : r4.atoms)
        clearsExisting = clearsExisting && (a.position - centre.position).length() >= 2.2f - 1e-3f;
    check(clearsExisting, "placed atoms keep their distance to atoms already in the scene");

    // --- Over-full container terminates ------------------------------------
    build::Container tiny;
    tiny.kind = build::Container::Sphere;
    tiny.radius = 3.0f;
    const build::FillResult r5 = build::fillContainer({ { h2, 200 } }, tiny, {}, 2.2f, 200, 11);
    check(r5.placed < r5.requested,
        QString("an over-full container terminates with placed < requested (%1 of %2)")
            .arg(r5.placed).arg(r5.requested));
    check(r5.placed > 0, "and still places what fits");

    // --- Box container ------------------------------------------------------
    build::Container box;
    box.kind = build::Container::Box;
    box.min = QVector3D(-5, -5, -5);
    box.max = QVector3D(5, 5, 5);
    const build::FillResult r6 = build::fillContainer({ { n2, 4 } }, box, {}, 2.2f, 4000, 5);
    bool inBox = (r6.placed == 4);
    for (const MoleculeViewer::Atom& a : r6.atoms) {
        inBox = inBox && a.position.x() >= box.min.x() + box.margin - 1e-4f
            && a.position.x() <= box.max.x() - box.margin + 1e-4f
            && a.position.y() >= box.min.y() + box.margin - 1e-4f
            && a.position.y() <= box.max.y() - box.margin + 1e-4f
            && a.position.z() >= box.min.z() + box.margin - 1e-4f
            && a.position.z() <= box.max.z() - box.margin + 1e-4f;
    }
    check(inBox, "box container: all 4 copies placed within the bounds");

    std::printf("%s (%d failed)\n", g_failed == 0 ? "PASS" : "FAIL", g_failed);
    return g_failed == 0 ? 0 : 1;
}
