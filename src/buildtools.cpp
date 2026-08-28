// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// buildtools — valence accounting + automatic hydrogen placement. Claude Generated 2026.
#include "buildtools.h"

#include "elementdata.h"

#include <QHash>
#include <QtMath>

namespace build {

int maxValence(const QString& element)
{
    // Typical valences of the elements the builder saturates with H. Everything
    // else (metals, noble gases, coarse-grained beads) returns 0 = leave alone.
    // Values follow BondEditor::getMaxValence, minus its catch-all default.
    static const QHash<QString, int> valences = {
        { "H", 1 }, { "C", 4 }, { "N", 3 }, { "O", 2 }, { "F", 1 },
        { "P", 5 }, { "S", 6 }, { "Cl", 1 }, { "Br", 1 }, { "I", 1 },
        { "Si", 4 }, { "B", 3 }, { "As", 3 }, { "Se", 2 }
    };
    return valences.value(element, 0);
}

int usedValence(int atomIndex, const QVector<MoleculeViewer::Bond>& bonds)
{
    int used = 0;
    for (const MoleculeViewer::Bond& b : bonds)
        if (b.atom1 == atomIndex || b.atom2 == atomIndex)
            used += qMax(1, b.bondOrder);
    return used;
}

int openValence(int atomIndex, const QVector<MoleculeViewer::Atom>& atoms,
    const QVector<MoleculeViewer::Bond>& bonds)
{
    if (atomIndex < 0 || atomIndex >= atoms.size())
        return 0;
    const int maxV = maxValence(atoms[atomIndex].element);
    if (maxV <= 0)
        return 0;
    // S and P: only saturate up to the common low-valence state (H2S, PH3) —
    // hypervalence (SF6, PF5) is a deliberate act, not an auto-fill target.
    const QString& el = atoms[atomIndex].element;
    const int target = (el == QLatin1String("S")) ? 2 : (el == QLatin1String("P")) ? 3 : maxV;
    return qMax(0, target - usedValence(atomIndex, bonds));
}

namespace {

// Lone pairs entering the steric number (VSEPR): O/S/Se 2, N/P/As 1, halogens 3.
int lonePairs(const QString& element)
{
    static const QHash<QString, int> pairs = {
        { "O", 2 }, { "S", 2 }, { "Se", 2 },
        { "N", 1 }, { "P", 1 }, { "As", 1 },
        { "F", 3 }, { "Cl", 3 }, { "Br", 3 }, { "I", 3 }
    };
    return pairs.value(element, 0);
}

// Any unit vector perpendicular to v.
QVector3D anyPerpendicular(const QVector3D& v)
{
    QVector3D p = QVector3D::crossProduct(v, QVector3D(0, 0, 1));
    if (p.lengthSquared() < 1e-6f)
        p = QVector3D::crossProduct(v, QVector3D(0, 1, 0));
    return p.normalized();
}

// Directions (unit vectors) for `missing` new substituents at an atom with the
// given existing neighbour directions and steric number. See buildtools.h.
QVector<QVector3D> newSubstituentDirections(const QVector<QVector3D>& neighbours,
    int missing, int stericNumber)
{
    QVector<QVector3D> dirs;
    const int n = neighbours.size();

    if (n == 0) {
        // Isolated atom: ideal geometry for the steric number. Tetrahedral
        // directions are the alternating cube corners (methane geometry).
        static const QVector3D tet[4] = {
            QVector3D(1, 1, 1).normalized(), QVector3D(1, -1, -1).normalized(),
            QVector3D(-1, 1, -1).normalized(), QVector3D(-1, -1, 1).normalized()
        };
        if (stericNumber <= 2) {
            if (missing >= 1) dirs.append(QVector3D(1, 0, 0));
            if (missing >= 2) dirs.append(QVector3D(-1, 0, 0));
        } else if (stericNumber == 3) {
            const float s = std::sqrt(3.0f) / 2.0f;
            const QVector3D tri[3] = { { 1, 0, 0 }, { -0.5f, s, 0 }, { -0.5f, -s, 0 } };
            for (int i = 0; i < qMin(missing, 3); ++i)
                dirs.append(tri[i]);
        } else {
            for (int i = 0; i < qMin(missing, 4); ++i)
                dirs.append(tet[i]);
        }
        return dirs;
    }

    if (n == 1) {
        const QVector3D d = neighbours[0];
        const QVector3D p = anyPerpendicular(d);
        if (stericNumber <= 2) {
            dirs.append(-d);  // linear: opposite the existing bond
        } else if (stericNumber == 3) {
            // Trigonal planar: 120 deg from the existing bond, in one plane.
            const float c = std::cos(qDegreesToRadians(120.0f));
            const float s = std::sin(qDegreesToRadians(120.0f));
            dirs.append((d * c + p * s).normalized());
            if (missing >= 2)
                dirs.append((d * c - p * s).normalized());
        } else {
            // Tetrahedral cap: cone at 109.47 deg from the existing bond,
            // staggered 120 deg apart around it.
            const QVector3D q = QVector3D::crossProduct(d, p).normalized();
            const float c = std::cos(qDegreesToRadians(109.47f));
            const float s = std::sin(qDegreesToRadians(109.47f));
            for (int k = 0; k < qMin(missing, 3); ++k) {
                const float phi = qDegreesToRadians(120.0f * k);
                dirs.append((d * c + (p * std::cos(phi) + q * std::sin(phi)) * s).normalized());
            }
        }
        return dirs;
    }

    if (n == 2) {
        // Bisector of the existing bonds; out-of-plane axis for tetrahedral.
        QVector3D b = -(neighbours[0] + neighbours[1]);
        if (b.lengthSquared() < 1e-6f)
            b = anyPerpendicular(neighbours[0]);  // linear neighbours
        b.normalize();
        QVector3D p = QVector3D::crossProduct(neighbours[0], neighbours[1]);
        if (p.lengthSquared() < 1e-6f)
            p = anyPerpendicular(b);
        p.normalize();
        if (stericNumber >= 4) {
            // Tetrahedral: H out of the neighbour plane, +-54.74 deg about the
            // bisector (two H then span ~109.47 deg; one H takes the + side).
            const float c = std::cos(qDegreesToRadians(54.74f));
            const float s = std::sin(qDegreesToRadians(54.74f));
            dirs.append((b * c + p * s).normalized());
            if (missing >= 2)
                dirs.append((b * c - p * s).normalized());
        } else {
            dirs.append(b);  // trigonal: in-plane along the bisector
        }
        return dirs;
    }

    // n >= 3: the remaining apex — opposite the neighbour average; for planar
    // coordination that average vanishes, then use the plane normal.
    QVector3D sum;
    for (const QVector3D& d : neighbours)
        sum += d;
    QVector3D b = -sum;
    if (b.lengthSquared() < 1e-6f)
        b = QVector3D::crossProduct(neighbours[1] - neighbours[0],
            neighbours[2] - neighbours[0]);
    if (b.lengthSquared() < 1e-6f)
        b = anyPerpendicular(neighbours[0]);
    dirs.append(b.normalized());
    return dirs;
}

} // namespace

void generateHydrogens(const QVector<MoleculeViewer::Atom>& atoms,
    const QVector<MoleculeViewer::Bond>& bonds, const QVector<int>& targets,
    QVector<MoleculeViewer::Atom>& outH, QVector<MoleculeViewer::Bond>& outHBonds)
{
    outH.clear();
    outHBonds.clear();

    QVector<int> work = targets;
    if (work.isEmpty())
        for (int i = 0; i < atoms.size(); ++i)
            work.append(i);

    const float rH = elem::covalentRadius(QStringLiteral("H"));
    for (int index : work) {
        if (index < 0 || index >= atoms.size())
            continue;
        const MoleculeViewer::Atom& atom = atoms[index];
        if (!elem::isElementSymbol(atom.element))
            continue;  // coarse-grained beads etc. stay untouched
        const int missing = openValence(index, atoms, bonds);
        if (missing <= 0)
            continue;

        QVector<QVector3D> neighbourDirs;
        int neighbourCount = 0;
        for (const MoleculeViewer::Bond& b : bonds) {
            int other = -1;
            if (b.atom1 == index)
                other = b.atom2;
            else if (b.atom2 == index)
                other = b.atom1;
            if (other < 0 || other >= atoms.size())
                continue;
            ++neighbourCount;
            const QVector3D d = atoms[other].position - atom.position;
            if (d.lengthSquared() > 1e-8f)
                neighbourDirs.append(d.normalized());
        }

        const int stericNumber = neighbourCount + missing + lonePairs(atom.element);
        const QVector<QVector3D> dirs = newSubstituentDirections(neighbourDirs, missing, stericNumber);
        const float dist = elem::covalentRadius(atom.element) + rH;
        for (int k = 0; k < qMin<int>(missing, dirs.size()); ++k) {
            MoleculeViewer::Atom h;
            h.element = QStringLiteral("H");
            h.position = atom.position + dirs[k] * dist;
            outHBonds.append({ index, int(atoms.size() + outH.size()), 1 });
            outH.append(h);
        }
    }
}

} // namespace build
