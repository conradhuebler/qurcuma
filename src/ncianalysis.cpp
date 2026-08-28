// ncianalysis.cpp - Non-covalent interaction (NCI) detection
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026
//
// All criteria are documented in ncianalysis.h next to their declarations; the
// literature references live there so a reader finds them before the code.

#include "ncianalysis.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <QObject>
#include <QSet>
#include <QStringList>

#include <Eigen/Dense>

#include "forceinjector.h"

#include "src/core/elements.h"
#include "src/core/topology.h"

namespace nci {

namespace {

    /// Atomic number of an element label, 0 for coarse-grained bead names.
    int atomicNumber(const QString& element)
    {
        if (element.isEmpty())
            return 0;
        return Elements::String2Element(element.toStdString());
    }

    bool isElement(const QString& element, const char* symbol)
    {
        return element.compare(QLatin1String(symbol), Qt::CaseInsensitive) == 0;
    }

    /// Angle at @p vertex between the two arms, in degrees.
    float angleAt(const QVector3D& vertex, const QVector3D& armA, const QVector3D& armB)
    {
        const QVector3D u = armA - vertex;
        const QVector3D v = armB - vertex;
        const float lu = u.length();
        const float lv = v.length();
        if (lu < 1e-6f || lv < 1e-6f)
            return 0.0f;
        float cosine = QVector3D::dotProduct(u, v) / (lu * lv);
        cosine = std::max(-1.0f, std::min(1.0f, cosine));
        return static_cast<float>(std::acos(cosine) * 180.0 / M_PI);
    }

    float clamp01(float x)
    {
        return std::max(0.0f, std::min(1.0f, x));
    }

    /// True if atoms @p i and @p j are at most @p maxDepth bonds apart.
    /// Small bounded BFS: with maxDepth <= 3 this stays O(degree^3).
    bool withinBondDistance(const forceinjector::Adjacency& adj, int i, int j, int maxDepth)
    {
        if (i == j)
            return true;
        if (maxDepth < 1)
            return false;

        QVector<int> current { i };
        QVector<int> visited { i };
        for (int depth = 1; depth <= maxDepth; ++depth) {
            QVector<int> next;
            for (int atom : current) {
                for (int nb : adj[atom]) {
                    if (nb == j)
                        return true;
                    if (visited.contains(nb))
                        continue;
                    visited.append(nb);
                    next.append(nb);
                }
            }
            if (next.isEmpty())
                break;
            current = next;
        }
        return false;
    }

    /// Connected components of the bond graph; entry i is the fragment id of atom i.
    QVector<int> fragmentIds(int atomCount, const forceinjector::Adjacency& adj)
    {
        QVector<int> ids(atomCount, -1);
        int fragment = 0;
        for (int start = 0; start < atomCount; ++start) {
            if (ids[start] >= 0)
                continue;
            QVector<int> stack { start };
            ids[start] = fragment;
            while (!stack.isEmpty()) {
                const int atom = stack.takeLast();
                for (int nb : adj[atom]) {
                    if (ids[nb] < 0) {
                        ids[nb] = fragment;
                        stack.append(nb);
                    }
                }
            }
            ++fragment;
        }
        return ids;
    }

    /// Fragment filter: 0 = all, 1 = intermolecular only, 2 = intramolecular only.
    bool fragmentAllowed(const QVector<int>& ids, int i, int j, int filter)
    {
        if (filter == 0)
            return true;
        const bool same = ids[i] == ids[j];
        return filter == 2 ? same : !same;
    }

    /// Best-fit plane of a set of atoms: centroid plus the normal (the eigenvector
    /// of the smallest eigenvalue of the coordinate covariance matrix) and the RMS
    /// deviation of the atoms from that plane.
    struct Plane {
        QVector3D centroid;
        QVector3D normal;
        float rms = 0.0f;
        bool valid = false;
    };

    Plane fitPlane(const QVector<MoleculeViewer::Atom>& atoms, const QVector<int>& members)
    {
        Plane plane;
        if (members.size() < 3)
            return plane;

        QVector3D centroid;
        for (int idx : members)
            centroid += atoms[idx].position;
        centroid /= static_cast<float>(members.size());

        Eigen::Matrix3d covariance = Eigen::Matrix3d::Zero();
        for (int idx : members) {
            const QVector3D d = atoms[idx].position - centroid;
            const Eigen::Vector3d v(d.x(), d.y(), d.z());
            covariance += v * v.transpose();
        }

        Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> solver(covariance);
        if (solver.info() != Eigen::Success)
            return plane;
        const Eigen::Vector3d n = solver.eigenvectors().col(0); // smallest eigenvalue

        plane.centroid = centroid;
        plane.normal = QVector3D(static_cast<float>(n.x()), static_cast<float>(n.y()),
            static_cast<float>(n.z()))
                           .normalized();

        double sum = 0.0;
        for (int idx : members) {
            const float d = QVector3D::dotProduct(atoms[idx].position - centroid, plane.normal);
            sum += static_cast<double>(d) * d;
        }
        plane.rms = static_cast<float>(std::sqrt(sum / members.size()));
        plane.valid = true;
        return plane;
    }

    /// Angle between two ring planes, folded into 0..90 degrees.
    float interplanarAngle(const QVector3D& na, const QVector3D& nb)
    {
        float cosine = std::fabs(QVector3D::dotProduct(na, nb));
        cosine = std::min(1.0f, cosine);
        return static_cast<float>(std::acos(cosine) * 180.0 / M_PI);
    }

    float hydrogenBondScore(float distance, float angle, const Options& opt)
    {
        const float dSpan = std::max(0.1f, opt.hbMaxDistance - 1.6f);
        const float aSpan = std::max(1.0f, 180.0f - opt.hbMinAngle);
        return clamp01((opt.hbMaxDistance - distance) / dSpan)
            * clamp01((angle - opt.hbMinAngle) / aSpan);
    }

    float halogenBondScore(float distance, float angle, float cutoff, const Options& opt)
    {
        const float dSpan = std::max(0.1f, cutoff - 2.0f);
        const float aSpan = std::max(1.0f, 180.0f - opt.xbMinAngle);
        return clamp01((cutoff - distance) / dSpan)
            * clamp01((angle - opt.xbMinAngle) / aSpan);
    }

    float piStackingScore(float distance, const Options& opt)
    {
        const float dSpan = std::max(0.1f, opt.piMaxCentroid - 3.3f);
        return clamp01((opt.piMaxCentroid - distance) / dSpan);
    }

    float closeContactScore(float distance, float cutoff)
    {
        return clamp01((cutoff - distance) / 0.5f);
    }

    int kindRank(Kind k)
    {
        switch (k) {
        case Kind::HydrogenBond:
            return 0;
        case Kind::HalogenBond:
            return 1;
        case Kind::PiStacking:
            return 2;
        case Kind::Electrostatic:
            return 3;
        case Kind::Dispersion:
            return 4;
        case Kind::CloseContact:
            return 5;
        }
        return 6;
    }

    /// Ring atom closest to the ring centroid - the representative index a
    /// pi-stacking row reports so table and selection behave like any other row.
    int centralAtom(const QVector<MoleculeViewer::Atom>& atoms, const QVector<int>& ring,
        const QVector3D& centroid)
    {
        int best = ring.isEmpty() ? -1 : ring.first();
        float bestDist = std::numeric_limits<float>::max();
        for (int idx : ring) {
            const float d = (atoms[idx].position - centroid).length();
            if (d < bestDist) {
                bestDist = d;
                best = idx;
            }
        }
        return best;
    }

    QString atomLabel(const QVector<MoleculeViewer::Atom>& atoms, int index)
    {
        if (index < 0 || index >= atoms.size())
            return QStringLiteral("?");
        const QString& e = atoms[index].element;
        const QString symbol = e.isEmpty() ? atoms[index].type : e;
        return QStringLiteral("%1%2").arg(symbol.isEmpty() ? QStringLiteral("X") : symbol).arg(index);
    }

} // namespace

double vdwRadius(const QString& element)
{
    const int z = atomicNumber(element);
    if (z <= 0 || z >= static_cast<int>(Elements::VanDerWaalsRadius.size()))
        return 0.0;
    const double r = Elements::VanDerWaalsRadius[z];
    return r > 0.0 ? r : 0.0;
}

bool isHydrogenBondDonor(const QString& element)
{
    return isElement(element, "N") || isElement(element, "O") || isElement(element, "F")
        || isElement(element, "S");
}

bool isHydrogenBondAcceptor(const QString& element)
{
    return isElement(element, "N") || isElement(element, "O") || isElement(element, "F")
        || isElement(element, "S") || isElement(element, "Cl") || isElement(element, "Br")
        || isElement(element, "I");
}

bool isHalogenDonor(const QString& element)
{
    // Fluorine is deliberately absent: it carries no sigma-hole.
    return isElement(element, "Cl") || isElement(element, "Br") || isElement(element, "I")
        || isElement(element, "At");
}

bool isLewisBase(const QString& element)
{
    return isElement(element, "N") || isElement(element, "O") || isElement(element, "F")
        || isElement(element, "P") || isElement(element, "S") || isElement(element, "Se")
        || isElement(element, "Cl") || isElement(element, "Br") || isElement(element, "I");
}

QVector<QVector<int>> findAromaticRings(int atomCount, const QVector<MoleculeViewer::Bond>& bonds)
{
    QVector<QVector<int>> rings;
    if (atomCount < 5)
        return rings;

    const forceinjector::Adjacency adj = forceinjector::buildAdjacency(atomCount, bonds);

    // curcuma's ring perception works on std::vector adjacency lists.
    std::vector<std::vector<int>> stored(atomCount);
    for (int i = 0; i < atomCount; ++i) {
        stored[i].reserve(adj[i].size());
        for (int nb : adj[i])
            stored[i].push_back(nb);
    }

    // maxsize 8 bounds the search: only five- and six-rings are of interest here.
    const std::vector<std::vector<int>> found = Topology::FindRings(stored, atomCount, 8);

    for (const std::vector<int>& ring : found) {
        if (ring.size() < 5 || ring.size() > 6)
            continue;
        bool aromaticLike = true;
        for (int idx : ring) {
            if (idx < 0 || idx >= atomCount) {
                aromaticLike = false;
                break;
            }
            // sp2 proxy: an aromatic ring atom has at most three bonded neighbours.
            if (adj[idx].size() > 3) {
                aromaticLike = false;
                break;
            }
        }
        if (!aromaticLike)
            continue;
        QVector<int> members;
        members.reserve(static_cast<int>(ring.size()));
        for (int idx : ring)
            members.append(idx);
        rings.append(members);
    }
    return rings;
}

QVector<Contact> detectGeometric(const QVector<MoleculeViewer::Atom>& atoms,
    const QVector<MoleculeViewer::Bond>& bonds,
    const Options& opt,
    const QVector<QVector<int>>* rings)
{
    QVector<Contact> contacts;
    const int n = atoms.size();
    if (n < 2)
        return contacts;

    const forceinjector::Adjacency adj = forceinjector::buildAdjacency(n, bonds);
    const QVector<int> fragments = fragmentIds(n, adj);
    const int exclusionDepth = std::max(1, opt.minBondSeparation - 1);

    // Pairs already reported as a directional interaction are not repeated as a
    // plain close contact.
    QSet<quint64> reported;
    const auto pairKey = [](int a, int b) -> quint64 {
        const quint64 lo = static_cast<quint64>(std::min(a, b));
        const quint64 hi = static_cast<quint64>(std::max(a, b));
        return (hi << 32) | lo;
    };

    // --- Hydrogen bonds: loop over hydrogens bound to a donor, not over all pairs.
    if (opt.hydrogenBonds) {
        for (int h = 0; h < n; ++h) {
            if (!isElement(atoms[h].element, "H"))
                continue;
            if (adj[h].isEmpty())
                continue;
            const int donor = adj[h].first();
            if (donor < 0 || donor >= n || !isHydrogenBondDonor(atoms[donor].element))
                continue;

            for (int a = 0; a < n; ++a) {
                if (a == h || a == donor)
                    continue;
                if (!isHydrogenBondAcceptor(atoms[a].element))
                    continue;
                if (adj[h].contains(a))
                    continue;
                if (!fragmentAllowed(fragments, donor, a, opt.fragmentFilter))
                    continue;
                if (withinBondDistance(adj, donor, a, exclusionDepth))
                    continue;

                const float d = (atoms[a].position - atoms[h].position).length();
                if (d > opt.hbMaxDistance)
                    continue;
                const float angle = angleAt(atoms[h].position, atoms[donor].position, atoms[a].position);
                if (angle < opt.hbMinAngle)
                    continue;

                Contact c;
                c.kind = Kind::HydrogenBond;
                c.donor = donor;
                c.bridge = h;
                c.acceptor = a;
                c.distance = d;
                c.angle = angle;
                c.score = hydrogenBondScore(d, angle, opt);
                contacts.append(c);
                reported.insert(pairKey(h, a));
                reported.insert(pairKey(donor, a));
            }
        }
    }

    // --- Halogen bonds: loop over Cl/Br/I/At only.
    if (opt.halogenBonds) {
        for (int x = 0; x < n; ++x) {
            if (!isHalogenDonor(atoms[x].element))
                continue;
            if (adj[x].isEmpty())
                continue;
            const int carrier = adj[x].first();
            const double rx = vdwRadius(atoms[x].element);
            if (rx <= 0.0)
                continue;

            for (int a = 0; a < n; ++a) {
                if (a == x || a == carrier)
                    continue;
                if (!isLewisBase(atoms[a].element))
                    continue;
                if (!fragmentAllowed(fragments, x, a, opt.fragmentFilter))
                    continue;
                if (withinBondDistance(adj, x, a, exclusionDepth))
                    continue;

                const double ra = vdwRadius(atoms[a].element);
                if (ra <= 0.0)
                    continue;
                const float cutoff = static_cast<float>(opt.xbVdwFraction * (rx + ra));
                const float d = (atoms[a].position - atoms[x].position).length();
                if (d > cutoff)
                    continue;
                const float angle = angleAt(atoms[x].position, atoms[carrier].position, atoms[a].position);
                if (angle < opt.xbMinAngle)
                    continue;

                Contact c;
                c.kind = Kind::HalogenBond;
                c.donor = carrier;
                c.bridge = x;
                c.acceptor = a;
                c.distance = d;
                c.angle = angle;
                c.score = halogenBondScore(d, angle, cutoff, opt);
                contacts.append(c);
                reported.insert(pairKey(x, a));
            }
        }
    }

    // --- Pi-stacking between planar five- and six-rings.
    if (opt.piStacking) {
        const QVector<QVector<int>> localRings = rings ? QVector<QVector<int>>() : findAromaticRings(n, bonds);
        const QVector<QVector<int>>& ringList = rings ? *rings : localRings;

        QVector<Plane> planes;
        QVector<int> planeRing;
        planes.reserve(ringList.size());
        for (int r = 0; r < ringList.size(); ++r) {
            bool inRange = true;
            for (int idx : ringList[r]) {
                if (idx < 0 || idx >= n) {
                    inRange = false;
                    break;
                }
            }
            if (!inRange)
                continue;
            const Plane p = fitPlane(atoms, ringList[r]);
            if (!p.valid || p.rms > opt.piPlanarityRms)
                continue;
            planes.append(p);
            planeRing.append(r);
        }

        for (int i = 0; i < planes.size(); ++i) {
            for (int j = i + 1; j < planes.size(); ++j) {
                const QVector<int>& ringI = ringList[planeRing[i]];
                const QVector<int>& ringJ = ringList[planeRing[j]];

                // Rings that share atoms are fused, not stacked.
                bool fused = false;
                for (int idx : ringI) {
                    if (ringJ.contains(idx)) {
                        fused = true;
                        break;
                    }
                }
                if (fused)
                    continue;
                if (!fragmentAllowed(fragments, ringI.first(), ringJ.first(), opt.fragmentFilter))
                    continue;

                const QVector3D delta = planes[j].centroid - planes[i].centroid;
                const float d = delta.length();
                if (d > opt.piMaxCentroid || d < 1e-3f)
                    continue;

                const float angle = interplanarAngle(planes[i].normal, planes[j].normal);
                // Lateral offset: the centroid separation projected into the ring plane.
                const float along = std::fabs(QVector3D::dotProduct(delta, planes[i].normal));
                const float offset = std::sqrt(std::max(0.0f, d * d - along * along));

                const bool parallel = angle <= opt.piParallelAngle && offset <= opt.piMaxOffset;
                const bool tShaped = angle >= opt.piTShapeAngle;
                if (!parallel && !tShaped)
                    continue;

                Contact c;
                c.kind = Kind::PiStacking;
                c.ringA = ringI;
                c.ringB = ringJ;
                c.donor = centralAtom(atoms, ringI, planes[i].centroid);
                c.acceptor = centralAtom(atoms, ringJ, planes[j].centroid);
                c.distance = d;
                c.angle = angle;
                c.offset = offset;
                c.score = piStackingScore(d, opt);
                c.motif = parallel ? 1 : 2; // 1 = parallel/offset, 2 = T-shaped
                contacts.append(c);
            }
        }
    }

    // --- Generic close contacts (off by default: this is the only full O(N^2) pass).
    if (opt.closeContacts) {
        for (int i = 0; i < n; ++i) {
            const double ri = vdwRadius(atoms[i].element);
            if (ri <= 0.0)
                continue;
            const bool iIsH = isElement(atoms[i].element, "H");
            for (int j = i + 1; j < n; ++j) {
                if (!opt.includeHH && iIsH && isElement(atoms[j].element, "H"))
                    continue;
                const double rj = vdwRadius(atoms[j].element);
                if (rj <= 0.0)
                    continue;
                if (reported.contains(pairKey(i, j)))
                    continue;
                if (!fragmentAllowed(fragments, i, j, opt.fragmentFilter))
                    continue;
                if (withinBondDistance(adj, i, j, exclusionDepth))
                    continue;

                const float cutoff = static_cast<float>(opt.contactVdwFraction * (ri + rj));
                const float d = (atoms[j].position - atoms[i].position).length();
                if (d > cutoff)
                    continue;

                Contact c;
                c.kind = Kind::CloseContact;
                c.donor = i;
                c.acceptor = j;
                c.distance = d;
                c.score = closeContactScore(d, cutoff);
                contacts.append(c);
            }
        }
    }

    rankAndTruncate(contacts, opt);
    return contacts;
}

void applyGeometricGate(QVector<Contact>& contacts, const QVector<MoleculeViewer::Atom>& atoms,
    const Options& opt)
{
    const int n = atoms.size();
    QVector<Contact> kept;
    kept.reserve(contacts.size());
    for (const Contact& c : contacts) {
        if (c.kind == Kind::HydrogenBond) {
            if (c.distance > opt.hbMaxDistance || c.angle < opt.hbMinAngle)
                continue;
        } else if (c.kind == Kind::HalogenBond) {
            if (c.bridge < 0 || c.bridge >= n || c.acceptor < 0 || c.acceptor >= n)
                continue;
            const double rx = vdwRadius(atoms[c.bridge].element);
            const double ra = vdwRadius(atoms[c.acceptor].element);
            const float cutoff = static_cast<float>(opt.xbVdwFraction * (rx + ra));
            if (c.distance > cutoff || c.angle < opt.xbMinAngle)
                continue;
        }
        kept.append(c);
    }
    contacts = kept;
}

int rankAndTruncate(QVector<Contact>& contacts, const Options& opt)
{
    std::sort(contacts.begin(), contacts.end(), [](const Contact& a, const Contact& b) {
        const int ra = kindRank(a.kind);
        const int rb = kindRank(b.kind);
        if (ra != rb)
            return ra < rb;
        return a.score > b.score;
    });
    if (opt.maxContacts <= 0 || contacts.size() <= opt.maxContacts)
        return 0;
    const int dropped = contacts.size() - opt.maxContacts;
    contacts.resize(opt.maxContacts);
    return dropped;
}

int limitKind(QVector<Contact>& contacts, Kind kind, int limit)
{
    if (limit < 0)
        return 0;
    QVector<Contact> ofKind;
    QVector<Contact> rest;
    for (const Contact& c : contacts) {
        if (c.kind == kind)
            ofKind.append(c);
        else
            rest.append(c);
    }
    if (ofKind.size() <= limit)
        return 0;
    std::sort(ofKind.begin(), ofKind.end(),
        [](const Contact& a, const Contact& b) { return a.score > b.score; });
    const int dropped = ofKind.size() - limit;
    ofKind.resize(limit);
    contacts = rest + ofKind;
    return dropped;
}

QString truncationNote(int dropped)
{
    if (dropped <= 0)
        return QString();
    return QObject::tr(" (%1 weaker contacts not shown)").arg(dropped);
}

void refreshGeometry(QVector<Contact>& contacts, const QVector<MoleculeViewer::Atom>& atoms,
    const Options& opt)
{
    const int n = atoms.size();
    for (Contact& c : contacts) {
        if (c.kind == Kind::PiStacking) {
            bool ok = !c.ringA.isEmpty() && !c.ringB.isEmpty();
            for (int idx : c.ringA)
                ok = ok && idx >= 0 && idx < n;
            for (int idx : c.ringB)
                ok = ok && idx >= 0 && idx < n;
            if (!ok)
                continue;
            const Plane pa = fitPlane(atoms, c.ringA);
            const Plane pb = fitPlane(atoms, c.ringB);
            if (!pa.valid || !pb.valid)
                continue;
            const QVector3D delta = pb.centroid - pa.centroid;
            c.distance = delta.length();
            c.angle = interplanarAngle(pa.normal, pb.normal);
            const float along = std::fabs(QVector3D::dotProduct(delta, pa.normal));
            c.offset = std::sqrt(std::max(0.0f, c.distance * c.distance - along * along));
            c.score = piStackingScore(c.distance, opt);
            continue;
        }

        const int from = c.bridge >= 0 ? c.bridge : c.donor;
        if (from < 0 || from >= n || c.acceptor < 0 || c.acceptor >= n)
            continue;
        c.distance = (atoms[c.acceptor].position - atoms[from].position).length();

        if (c.bridge >= 0 && c.donor >= 0 && c.donor < n) {
            c.angle = angleAt(atoms[c.bridge].position, atoms[c.donor].position,
                atoms[c.acceptor].position);
        }

        switch (c.kind) {
        case Kind::HydrogenBond:
            c.score = hydrogenBondScore(c.distance, c.angle, opt);
            break;
        case Kind::HalogenBond: {
            const double rx = vdwRadius(atoms[c.bridge].element);
            const double ra = vdwRadius(atoms[c.acceptor].element);
            const float cutoff = static_cast<float>(opt.xbVdwFraction * (rx + ra));
            c.score = halogenBondScore(c.distance, c.angle, cutoff, opt);
            break;
        }
        case Kind::CloseContact: {
            const double ri = vdwRadius(atoms[c.donor].element);
            const double rj = vdwRadius(atoms[c.acceptor].element);
            c.score = closeContactScore(c.distance,
                static_cast<float>(opt.contactVdwFraction * (ri + rj)));
            break;
        }
        default:
            // Electrostatic/Dispersion keep the score their parameter source assigned.
            break;
        }
    }
}

QVector<int> contactAtoms(const Contact& c)
{
    if (c.kind == Kind::PiStacking) {
        QVector<int> all = c.ringA;
        all += c.ringB;
        return all;
    }
    QVector<int> all;
    if (c.donor >= 0)
        all.append(c.donor);
    if (c.bridge >= 0)
        all.append(c.bridge);
    if (c.acceptor >= 0)
        all.append(c.acceptor);
    return all;
}

QString kindName(Kind k)
{
    switch (k) {
    case Kind::HydrogenBond:
        return QObject::tr("Hydrogen bond");
    case Kind::HalogenBond:
        return QObject::tr("Halogen bond");
    case Kind::PiStacking:
        return QObject::tr("Pi stacking");
    case Kind::CloseContact:
        return QObject::tr("Close contact");
    case Kind::Electrostatic:
        return QObject::tr("Electrostatic");
    case Kind::Dispersion:
        return QObject::tr("Dispersion");
    }
    return QString();
}

QColor kindColor(Kind k, double energy)
{
    switch (k) {
    case Kind::HydrogenBond:
        return QColor(0x4F, 0xD6, 0xC0);
    case Kind::HalogenBond:
        return QColor(0xFF, 0x9E, 0x4F);
    case Kind::PiStacking:
        return QColor(0x8F, 0xD3, 0x6B);
    case Kind::CloseContact:
        return QColor(0xB0, 0xB6, 0xC0);
    case Kind::Electrostatic:
        return energy > 0.0 ? QColor(0xFF, 0x6B, 0x6B) : QColor(0x7F, 0xA8, 0xFF);
    case Kind::Dispersion:
        return QColor(0xC9, 0xA0, 0xFF);
    }
    return QColor(0xB0, 0xB6, 0xC0);
}

int paletteKey(Kind k, double energy)
{
    if (k == Kind::Electrostatic && energy > 0.0)
        return ElectrostaticRepulsiveKey;
    return int(k);
}

QColor kindColor(Kind k, double energy, const Palette& palette)
{
    const auto it = palette.constFind(paletteKey(k, energy));
    if (it != palette.constEnd() && it.value().isValid())
        return it.value();
    return kindColor(k, energy);
}

QVector<QPair<int, QString>> paletteEntries()
{
    return {
        { int(Kind::HydrogenBond), kindName(Kind::HydrogenBond) },
        { int(Kind::HalogenBond), kindName(Kind::HalogenBond) },
        { int(Kind::PiStacking), kindName(Kind::PiStacking) },
        { int(Kind::CloseContact), kindName(Kind::CloseContact) },
        { int(Kind::Electrostatic), QObject::tr("Electrostatic (attractive)") },
        { ElectrostaticRepulsiveKey, QObject::tr("Electrostatic (repulsive)") },
        { int(Kind::Dispersion), kindName(Kind::Dispersion) },
    };
}

QString describe(const Contact& c, const QVector<MoleculeViewer::Atom>& atoms)
{
    if (c.kind == Kind::PiStacking) {
        return QObject::tr("ring(%1) %2 ... ring(%3) %4")
            .arg(c.ringA.size())
            .arg(atomLabel(atoms, c.donor))
            .arg(c.ringB.size())
            .arg(atomLabel(atoms, c.acceptor));
    }
    if (c.bridge >= 0) {
        return QStringLiteral("%1-%2 ... %3")
            .arg(atomLabel(atoms, c.donor), atomLabel(atoms, c.bridge), atomLabel(atoms, c.acceptor));
    }
    return QStringLiteral("%1 ... %2").arg(atomLabel(atoms, c.donor), atomLabel(atoms, c.acceptor));
}

QString summarize(const QVector<Contact>& contacts, Source source)
{
    int hb = 0, xb = 0, pi = 0, cc = 0, es = 0, disp = 0;
    for (const Contact& c : contacts) {
        switch (c.kind) {
        case Kind::HydrogenBond:
            ++hb;
            break;
        case Kind::HalogenBond:
            ++xb;
            break;
        case Kind::PiStacking:
            ++pi;
            break;
        case Kind::CloseContact:
            ++cc;
            break;
        case Kind::Electrostatic:
            ++es;
            break;
        case Kind::Dispersion:
            ++disp;
            break;
        }
    }

    QStringList parts;
    if (hb)
        parts << (hb == 1 ? QObject::tr("1 hydrogen bond") : QObject::tr("%1 hydrogen bonds").arg(hb));
    if (xb)
        parts << (xb == 1 ? QObject::tr("1 halogen bond") : QObject::tr("%1 halogen bonds").arg(xb));
    if (pi)
        parts << (pi == 1 ? QObject::tr("1 pi stack") : QObject::tr("%1 pi stacks").arg(pi));
    if (cc)
        parts << (cc == 1 ? QObject::tr("1 close contact") : QObject::tr("%1 close contacts").arg(cc));
    if (es)
        parts << (es == 1 ? QObject::tr("1 electrostatic pair") : QObject::tr("%1 electrostatic pairs").arg(es));
    if (disp)
        parts << (disp == 1 ? QObject::tr("1 dispersion pair") : QObject::tr("%1 dispersion pairs").arg(disp));

    QString prefix;
    switch (source) {
    case Source::Geometry:
        prefix = QObject::tr("Geometry");
        break;
    case Source::GfnffParameters:
        prefix = QObject::tr("GFN-FF");
        break;
    case Source::Population:
        prefix = QObject::tr("GFN2 population");
        break;
    }

    if (parts.isEmpty())
        return QObject::tr("%1: no contacts").arg(prefix);
    return QStringLiteral("%1: %2").arg(prefix, parts.join(QStringLiteral(", ")));
}

} // namespace nci
