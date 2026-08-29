// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// SceneController — Qt Quick 3D view-model. Claude Generated.
#include "scenecontroller.h"

#include "atominstancing.h"
#include "bondinstancing.h"
#include "elementdata.h"

#include "src/core/elements.h"

#include <QMap>
#include <QPair>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <limits>

namespace {
constexpr float kSphereBaseRadius = 50.0f; // #Sphere base radius
constexpr float kCylBaseHalfHeight = 50.0f; // #Cylinder half height (100 tall)
constexpr float kCylBaseRadius = 50.0f; // #Cylinder base radius

/// Apply a per-structure colour @p tint to a base scheme colour for an overlay atom.
/// The whole structure reads in the tint's colour family while element identity stays
/// visible: achromatic atoms (carbon grey, hydrogen white) adopt the tint hue directly;
/// chromatic atoms (O/N/...) rotate part-way toward the tint hue so they stay distinct.
/// The base brightness is preserved (so O stays darker than C) and only slightly dimmed
/// so the overlay reads as the secondary set. Claude Generated 2026.
QColor shiftOverlayColor(const QColor& base, const QColor& tint)
{
    int hb, sb, vb, ab;
    base.getHsv(&hb, &sb, &vb, &ab);
    int ht, st, vt, at;
    tint.getHsv(&ht, &st, &vt, &at);
    if (ht < 0)
        ht = 0; // achromatic tint (shouldn't happen for a picked colour) -> red anchor

    int h;
    if (hb < 0) {
        h = ht; // achromatic base -> take the tint hue directly (e.g. "green" carbons)
    } else {
        // Rotate the element hue 60% of the way toward the tint hue (shortest path), so
        // heteroatoms shift into the tint family yet keep a distinct hue.
        const int dh = ((ht - hb + 540) % 360) - 180;
        h = (hb + int(0.6f * dh) + 360) % 360;
    }
    const int s = qBound(80, qMax(sb, st), 255);    // ensure a visible tint, even on greys
    const int v = qBound(40, int(vb * 0.95f), 255); // keep brightness identity, read as secondary
    return QColor::fromHsv(h, s, v);
}

/// Shift @p base toward @p tint by @p amount (0..1), hue only. Unlike
/// shiftOverlayColor() this does not dim the result: a tinted fragment is a
/// primary structure, not a secondary overlay. Achromatic atoms (carbon grey,
/// hydrogen white) take the tint hue directly and get a saturation floor so the
/// shift is visible on them at all; chromatic atoms rotate part-way, keeping
/// oxygen recognisably red and nitrogen blue inside the tinted fragment.
/// Claude Generated 2026.
QColor tintFragmentColor(const QColor& base, const QColor& tint, float amount)
{
    if (!tint.isValid() || amount <= 0.0f)
        return base;
    const float a = qBound(0.0f, amount, 1.0f);

    int hb, sb, vb, ab;
    base.getHsv(&hb, &sb, &vb, &ab);
    int ht, st, vt, at;
    tint.getHsv(&ht, &st, &vt, &at);
    if (ht < 0)
        return base;   // achromatic tint carries no hue to shift toward

    int h;
    if (hb < 0) {
        h = ht;
    } else {
        const int dh = ((ht - hb + 540) % 360) - 180;
        h = (hb + int(a * dh) + 360) % 360;
    }
    const int floorSat = int(a * 140);              // greys must pick up the hue
    const int s = qBound(qMin(floorSat, 255), sb, 255);
    QColor out = QColor::fromHsv(h, qMax(s, floorSat), vb);
    out.setAlpha(ab);
    return out;
}

/// Automatic tint hue of fragment @p index (1-based; fragment 0 is the untinted
/// reference). Golden-angle spacing keeps consecutive fragments far apart in hue.
/// Claude Generated 2026.
QColor automaticFragmentTint(int index)
{
    const float hue = std::fmod(0.08f + 0.381966f * float(index), 1.0f);
    return QColor::fromHsvF(hue, 0.75f, 0.95f);
}

/// Quaternion rotating local +Y onto a unit bond direction (== view.cpp:1345).
QQuaternion bondRotation(const QVector3D& normDir)
{
    const QVector3D localUp(0, 1, 0);
    const QVector3D rotAxis = QVector3D::crossProduct(localUp, normDir);
    if (rotAxis.length() < 0.001f) {
        return normDir.y() > 0 ? QQuaternion()
                               : QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 180.0f);
    }
    const float angle = qAcos(QVector3D::dotProduct(localUp, normDir)) * 180.0f / float(M_PI);
    return QQuaternion::fromAxisAndAngle(rotAxis.normalized(), angle);
}
}

SceneController::SceneController(QObject* parent)
    : QObject(parent)
{
    m_atomInstancing = new AtomInstancing(nullptr);
    m_atomInstancing->setParent(this);
    m_bondInstancing = new BondInstancing(nullptr);
    m_bondInstancing->setParent(this);
    m_arrowShaft = new BondInstancing(nullptr);
    m_arrowShaft->setParent(this);
    m_arrowTip = new BondInstancing(nullptr);
    m_arrowTip->setParent(this);
    m_measureLines = new BondInstancing(nullptr);
    m_measureLines->setParent(this);
    m_wallLines = new BondInstancing(nullptr);
    m_wallLines->setParent(this);
    m_potShells = new BondInstancing(nullptr);
    m_potShells->setParent(this);
    m_wallForceShafts = new BondInstancing(nullptr);
    m_wallForceShafts->setParent(this);
    m_wallForceTips = new BondInstancing(nullptr);
    m_wallForceTips->setParent(this);
    m_nciLines = new BondInstancing(nullptr);
    m_nciLines->setParent(this);
    m_overlayAtoms = new AtomInstancing(nullptr);
    m_overlayAtoms->setParent(this);
    m_overlayBonds = new BondInstancing(nullptr);
    m_overlayBonds->setParent(this);

    // Claude Generated 2026 - Rebuild overlay labels whenever the structure changes
    // (load/reset/clear/clone). Live MD frames are handled in updatePositions();
    // rotation/zoom/pan need no rebuild (viewer3d.qml reprojects reactively).
    connect(this, &SceneController::structureChanged, this, &SceneController::rebuildLabels);
}

QQuick3DInstancing* SceneController::measureLineInstancing() const { return m_measureLines; }
QQuick3DInstancing* SceneController::overlayAtomInstancing() const { return m_overlayAtoms; }
QQuick3DInstancing* SceneController::overlayBondInstancing() const { return m_overlayBonds; }
QQuick3DInstancing* SceneController::wallInstancing() const { return m_wallLines; }
QQuick3DInstancing* SceneController::wallPotShellsInstancing() const { return m_potShells; }
QQuick3DInstancing* SceneController::wallForceShaftsInstancing() const { return m_wallForceShafts; }
QQuick3DInstancing* SceneController::wallForceTipsInstancing() const { return m_wallForceTips; }

void SceneController::setMeasurement(const QVector<QPair<QVector3D, QVector3D>>& lines, const QString& text)
{
    QVector<BondInstancing::Segment> segs;
    segs.reserve(lines.size());
    const float r = 0.06f; // thin measurement line
    const QColor color(90, 220, 255); // bright cyan, distinct from CPK
    for (const auto& ln : lines) {
        const QVector3D dir = ln.second - ln.first;
        const float length = dir.length();
        if (length < 1e-4f)
            continue;
        BondInstancing::Segment s;
        s.center = 0.5f * (ln.first + ln.second);
        s.scale = QVector3D(r / 50.0f, length / 100.0f, r / 50.0f);
        s.rotation = bondRotation(dir / length);
        s.color = color;
        segs.append(s);
    }
    m_measureLines->setSegments(segs);
    m_measurementText = text;
    emit measurementChanged();
}

int SceneController::addOverlayStructure(const QVector<AtomDatum>& atoms,
    const QVector<BondDatum>& bonds, const QColor& tint, float sizeScale)
{
    OverlayStructure ov;
    ov.atoms = atoms;
    ov.bonds = bonds;
    ov.tint = tint;
    ov.sizeScale = sizeScale;
    ov.visible = true;
    m_overlays.append(ov);
    rebuildOverlays();
    return m_overlays.size() - 1;
}

void SceneController::setOverlayTint(int index, const QColor& tint)
{
    if (index < 0 || index >= m_overlays.size())
        return;
    m_overlays[index].tint = tint;
    rebuildOverlays();
}

void SceneController::setOverlaySize(int index, float sizeScale)
{
    if (index < 0 || index >= m_overlays.size())
        return;
    m_overlays[index].sizeScale = sizeScale;
    rebuildOverlays();
}

void SceneController::setOverlayVisible(int index, bool visible)
{
    if (index < 0 || index >= m_overlays.size())
        return;
    m_overlays[index].visible = visible;
    rebuildOverlays();
}

void SceneController::removeOverlayStructure(int index)
{
    if (index < 0 || index >= m_overlays.size())
        return;
    m_overlays.removeAt(index);
    rebuildOverlays();
}

void SceneController::clearOverlay()
{
    if (m_overlays.isEmpty() && !m_overlayVisible)
        return;
    m_overlays.clear();
    m_overlayAtoms->setItems({});
    m_overlayBonds->setSegments({});
    m_overlayVisible = false;
    emit overlayChanged();
}

// Repack every visible overlay structure into the two combined overlay instancing
// buffers. Overlays inherit the global display styles (rendering-mode visibility, the
// sphere radius factor, atom scale, bond thickness, transparency, base colour scheme)
// and apply each structure's individual colour tint + size scale. Called by the list
// mutators and at the end of rebuildGeometry() so global style changes propagate.
void SceneController::rebuildOverlays()
{
    if (m_overlays.isEmpty()) {
        if (m_overlayVisible) {
            m_overlayAtoms->setItems({});
            m_overlayBonds->setSegments({});
            m_overlayVisible = false;
            emit overlayChanged();
        }
        return;
    }

    const float radiusFactor = (m_renderingMode == SpaceFilling) ? 1.0f : 0.30f;
    const float bondRadius = (m_renderingMode == Wireframe) ? qMin(m_bondRadius, 0.06f) : m_bondRadius;
    const float sxz = bondRadius / kCylBaseRadius;

    QVector<AtomInstancing::Item> items;
    QVector<BondInstancing::Segment> segs;
    bool anyVisible = false;

    for (const OverlayStructure& ov : m_overlays) {
        if (!ov.visible || ov.atoms.isEmpty())
            continue;
        anyVisible = true;

        auto tinted = [&](const AtomDatum& a) {
            QColor c = shiftOverlayColor(schemeColor(a), ov.tint);
            c.setAlphaF(m_transparency);
            return c;
        };

        if (m_atomsVisible) {
            items.reserve(items.size() + ov.atoms.size());
            for (const AtomDatum& a : ov.atoms) {
                AtomInstancing::Item it;
                it.position = a.position;
                it.scale = radiusFactor * ov.sizeScale * m_atomScaleFactor * atomDrawRadius(a);
                it.color = tinted(a);
                items.append(it);
            }
        }

        if (m_bondsVisible) {
            for (const BondDatum& b : ov.bonds) {
                if (b.a < 0 || b.b < 0 || b.a >= ov.atoms.size() || b.b >= ov.atoms.size())
                    continue;
                const QVector3D posA = ov.atoms[b.a].position;
                const QVector3D posB = ov.atoms[b.b].position;
                const QVector3D dir = posB - posA;
                const float length = dir.length();
                if (length < 1e-4f)
                    continue;
                const QQuaternion rot = bondRotation(dir / length);
                const QVector3D mid = 0.5f * (posA + posB);
                const float halfLength = length * 0.25f;
                const QVector3D scale(sxz, halfLength / kCylBaseHalfHeight, sxz);
                segs.append({ 0.5f * (posA + mid), scale, rot, tinted(ov.atoms[b.a]) });
                segs.append({ 0.5f * (mid + posB), scale, rot, tinted(ov.atoms[b.b]) });
            }
        }
    }

    m_overlayAtoms->setItems(items);
    m_overlayBonds->setSegments(segs);
    m_overlayVisible = anyVisible;
    emit overlayChanged();
}

// Claude Generated 2026 - Confinement-wall wireframe. Same #Cylinder-segment
// pattern as setMeasurement(): a line is a thin cylinder oriented along its
// direction. Edges are in intrinsic atom coordinates (the molecule's own frame),
// and the Model lives under moleculeRoot so the box rotates with the structure.
namespace {
// Append one cylinder segment along (a -> b) to @p segs with radius @p r.
void appendWallEdge(QVector<BondInstancing::Segment>& segs,
    const QVector3D& a, const QVector3D& b, float r, const QColor& color)
{
    const QVector3D dir = b - a;
    const float length = dir.length();
    if (length < 1e-4f)
        return;
    BondInstancing::Segment s;
    s.center = 0.5f * (a + b);
    s.scale = QVector3D(r / kCylBaseRadius, length / (2.0f * kCylBaseHalfHeight), r / kCylBaseRadius);
    s.rotation = bondRotation(dir / length);
    s.color = color;
    segs.append(s);
}

// Claude Generated 2026 - Fake a dashed line: Qt Quick 3D has no dashed-line
// primitive, so a dash is a short #Cylinder. The dash count is rounded from the
// line length and the step recomputed from it, so the dashes always fit exactly
// and no stub is left at the far end. At least two dashes, so even a short
// contact still reads as dashed rather than solid.
void appendDashedLine(QVector<BondInstancing::Segment>& segs,
    const QVector3D& a, const QVector3D& b, float r, const QColor& color,
    float dashPeriod = 0.30f, float dashFraction = 0.6f)
{
    const QVector3D delta = b - a;
    const float length = delta.length();
    if (length < 1e-3f)
        return;
    const QVector3D dir = delta / length;
    const QQuaternion rot = bondRotation(dir);

    const int count = qMax(2, int(qRound(length / dashPeriod)));
    const float step = length / count;
    const float halfDash = 0.5f * dashFraction * step;
    const float sxz = r / kCylBaseRadius;

    for (int k = 0; k < count; ++k) {
        BondInstancing::Segment s;
        s.center = a + dir * ((k + 0.5f) * step);
        s.scale = QVector3D(sxz, halfDash / kCylBaseHalfHeight, sxz);
        s.rotation = rot;
        s.color = color;
        segs.append(s);
    }
}

// Build 12-edge wireframe for a cuboid [mn..mx].
void buildBoxWireframe(QVector<BondInstancing::Segment>& segs,
    const QVector3D& mn, const QVector3D& mx, float r, const QColor& color)
{
    const QVector3D c000(mn.x(), mn.y(), mn.z()); const QVector3D c100(mx.x(), mn.y(), mn.z());
    const QVector3D c010(mn.x(), mx.y(), mn.z()); const QVector3D c110(mx.x(), mx.y(), mn.z());
    const QVector3D c001(mn.x(), mn.y(), mx.z()); const QVector3D c101(mx.x(), mn.y(), mx.z());
    const QVector3D c011(mn.x(), mx.y(), mx.z()); const QVector3D c111(mx.x(), mx.y(), mx.z());
    appendWallEdge(segs, c000, c100, r, color); appendWallEdge(segs, c010, c110, r, color);
    appendWallEdge(segs, c001, c101, r, color); appendWallEdge(segs, c011, c111, r, color);
    appendWallEdge(segs, c000, c010, r, color); appendWallEdge(segs, c100, c110, r, color);
    appendWallEdge(segs, c001, c011, r, color); appendWallEdge(segs, c101, c111, r, color);
    appendWallEdge(segs, c000, c001, r, color); appendWallEdge(segs, c100, c101, r, color);
    appendWallEdge(segs, c010, c011, r, color); appendWallEdge(segs, c110, c111, r, color);
}

// Build lat/lon wireframe for an origin-centred sphere.
void buildSphereWireframe(QVector<BondInstancing::Segment>& segs,
    float radius, float r, const QColor& color)
{
    if (radius < 0.1f) return;
    const int nLat = 7; const int nLng = 12;
    for (int i = 1; i <= nLat; ++i) {
        const float frac = float(i) / float(nLat + 1);
        const float z = radius * std::cos(frac * float(M_PI));
        const float ringR = radius * std::sin(frac * float(M_PI));
        if (ringR < 1e-3f) continue;
        QVector3D prev(ringR, 0.0f, z);
        for (int k = 1; k <= 48; ++k) {
            const float ang = 2.0f * float(M_PI) * float(k) / 48.0f;
            const QVector3D cur(ringR * std::cos(ang), ringR * std::sin(ang), z);
            appendWallEdge(segs, prev, cur, r, color); prev = cur;
        }
    }
    for (int m = 0; m < nLng; ++m) {
        const float phi = 2.0f * float(M_PI) * float(m) / float(nLng);
        QVector3D prev(0.0f, 0.0f, radius);
        for (int k = 1; k <= 48; ++k) {
            const float theta = float(M_PI) * float(k) / 48.0f;
            const QVector3D cur(radius * std::sin(theta) * std::cos(phi),
                                radius * std::sin(theta) * std::sin(phi),
                                radius * std::cos(theta));
            appendWallEdge(segs, prev, cur, r, color); prev = cur;
        }
    }
}

// Force magnitude at signed distance d from wall boundary (d>0 = outside).
// Harmonic: F ∝ d for d>0 (zero inside); LogFermi: bell-shaped peak at d=0.
float wallForceMag(float d, bool harmonic, float wallBeta)
{
    if (harmonic)
        return qMax(0.0f, d);
    const float x = wallBeta * d;
    const float xc = qBound(-20.0f, x, 20.0f);
    const float sigma = 1.0f / (1.0f + std::exp(-xc));
    return wallBeta * sigma * (1.0f - sigma);
}
}

void SceneController::setWallBox(const QVector3D& min, const QVector3D& max)
{
    m_wallGeom = 2;  // rect
    m_wallMin = min;
    m_wallMax = max;
    rebuildWall();
    m_wallVisible = true;
    emit wallChanged();
}

void SceneController::setWallSphere(float radius)
{
    m_wallGeom = 1;  // sphere
    m_wallRadius = radius;
    rebuildWall();
    m_wallVisible = true;
    emit wallChanged();
}

// Claude Generated 2026 - Recolour the wall wireframe (e.g. red on boundary
// violations). Rebuilds the segment buffer from the stored geometry with the
// new colour so the instancing buffer reflects it immediately.
void SceneController::setWallColor(const QColor& color)
{
    if (m_wallColor == color)
        return;
    m_wallColor = color;
    if (m_wallVisible && m_wallGeom != 0)
        rebuildWall();
}

// Claude Generated 2026 - Wireframe transparency. The alpha is baked into
// the per-segment colour (mirrors atom transparency: SceneController bakes
// m_transparency into the instance colour, the material runs in Blend mode).
// So changing opacity MUST rebuild the segment buffer — the QML material
// baseColor is opaque white and only carries the alphaMode toggle.
void SceneController::setWallOpacity(qreal opacity)
{
    const qreal clamped = qBound(0.0, opacity, 1.0);
    if (qFuzzyCompare(m_wallOpacity, clamped))
        return;
    m_wallOpacity = clamped;
    if (m_wallVisible && m_wallGeom != 0)
        rebuildWall();
    emit wallChanged();
}

void SceneController::setWallVisible(bool on)
{
    if (m_wallVisible == on)
        return;
    if (!on)
        m_wallLines->setSegments({});
    m_wallVisible = on;
    emit wallChanged();
}

void SceneController::rebuildWall()
{
    const float r = 0.08f;
    const QColor color(m_wallColor.red(), m_wallColor.green(), m_wallColor.blue(),
                       int(m_wallOpacity * 255));
    // Build wireframe using the extracted helpers.
    QVector<BondInstancing::Segment> segs;
    if (m_wallGeom == 2)
        buildBoxWireframe(segs, m_wallMin, m_wallMax, r, color);
    else if (m_wallGeom == 1)
        buildSphereWireframe(segs, m_wallRadius, r, color);
    m_wallLines->setSegments(segs);

    // Iso-potential shells: 3 inside + 3 outside, colour-coded blue→teal inside,
    // yellow→red outside. Harmonic = fixed Å; LogFermi = scale with 1/beta.
    if (!m_potVizEnabled || m_wallGeom == 0) {
        m_potShells->setSegments({});
        return;
    }
    float d1, d2, d3;
    if (m_wallHarmonic) {
        d1 = 4.0f; d2 = 2.0f; d3 = 0.8f;
    } else {
        const float beta = qMax(0.1f, m_wallBeta);
        d1 = qBound(0.2f, 4.0f / beta, 8.0f);
        d2 = qBound(0.1f, 2.0f / beta, 6.0f);
        d3 = qBound(0.05f, 0.5f / beta, 4.0f);
    }
    const int ai = int(m_wallOpacity * 255);
    // Inside: blue → cyan → teal (cool, approach zone)
    const QColor cIn1(50,  110, 255, qMin(255, int(ai * 0.22f)));
    const QColor cIn2(60,  200, 255, qMin(255, int(ai * 0.28f)));
    const QColor cIn3(80,  230, 170, qMin(255, int(ai * 0.32f)));
    // Outside: yellow → orange → red (warm, force zone)
    const QColor cOut1(235, 235,  50, qMin(255, int(ai * 0.36f)));
    const QColor cOut2(255, 140,  30, qMin(255, int(ai * 0.42f)));
    const QColor cOut3(255,  40,  40, qMin(255, int(ai * 0.50f)));
    const float rs = 0.05f;

    QVector<BondInstancing::Segment> shells;
    if (m_wallGeom == 2) {
        // expand > 0 = grow outward; expand < 0 = shrink inward
        auto addShell = [&](float expand, const QColor& c) {
            const QVector3D mn = m_wallMin - QVector3D(expand, expand, expand);
            const QVector3D mx = m_wallMax + QVector3D(expand, expand, expand);
            if (mn.x() < mx.x() && mn.y() < mx.y() && mn.z() < mx.z())
                buildBoxWireframe(shells, mn, mx, rs, c);
        };
        addShell(-d1, cIn1); addShell(-d2, cIn2); addShell(-d3, cIn3);
        addShell(+d3, cOut1); addShell(+d2, cOut2); addShell(+d1, cOut3);
    } else if (m_wallGeom == 1) {
        auto addShell = [&](float delta, const QColor& c) {
            buildSphereWireframe(shells, qMax(0.2f, m_wallRadius + delta), rs, c);
        };
        addShell(-d1, cIn1); addShell(-d2, cIn2); addShell(-d3, cIn3);
        addShell(+d3, cOut1); addShell(+d2, cOut2); addShell(+d1, cOut3);
    }
    m_potShells->setSegments(shells);
}

void SceneController::setWallPotentialViz(bool enabled, bool harmonic, double wallTemp, float wallBeta)
{
    const bool paramsChanged = m_wallHarmonic != harmonic || m_wallTemp != wallTemp || m_wallBeta != wallBeta;
    const bool vizChanged    = m_potVizEnabled != enabled;
    m_potVizEnabled = enabled;
    m_wallHarmonic  = harmonic;
    m_wallTemp      = wallTemp;
    m_wallBeta      = wallBeta;
    if (!vizChanged && !paramsChanged) return;
    if (m_wallGeom != 0 && m_wallVisible) {
        rebuildWall();  // rebuilds shells (and clears them if disabled)
        if (m_potArrowsEnabled)
            rebuildWallVectorField();
    } else {
        m_potShells->setSegments({});
    }
    emit wallChanged();
}

void SceneController::setWallVectorField(bool enabled, int resolution)
{
    m_potArrowsEnabled  = enabled;
    m_potArrowResolution = qBound(2, resolution, 10);
    if (!enabled || m_wallGeom == 0 || !m_wallVisible) {
        m_wallForceShafts->setSegments({});
        m_wallForceTips->setSegments({});
    } else {
        rebuildWallVectorField();
    }
    emit wallChanged();
}

void SceneController::rebuildWallVectorField()
{
    if (!m_potArrowsEnabled || m_wallGeom == 0) {
        m_wallForceShafts->setSegments({});
        m_wallForceTips->setSegments({});
        return;
    }
    const int res = m_potArrowResolution;

    // Sample distances: outside always shown (harmonic force > 0 only outside);
    // inside levels added for LogFermi (bell-shaped, non-zero inside too).
    struct Level { float dist; QColor color; };
    const int aBase = int(m_wallOpacity * 220);
    QVector<Level> levels;
    if (!m_wallHarmonic) {
        const float beta = qMax(0.1f, m_wallBeta);
        const float d2 = qBound(0.1f, 2.0f / beta, 6.0f);
        const float d3 = qBound(0.05f, 0.5f / beta, 4.0f);
        levels.append({ -d2, QColor(60, 200, 255, qMin(255, int(aBase * 0.45f))) });
        levels.append({ -d3, QColor(80, 230, 170, qMin(255, int(aBase * 0.55f))) });
    }
    // Compute shared outside distances
    float od1, od2, od3;
    if (m_wallHarmonic) {
        od1 = 4.0f; od2 = 2.0f; od3 = 0.8f;
    } else {
        const float beta = qMax(0.1f, m_wallBeta);
        od1 = qBound(0.2f, 4.0f / beta, 8.0f);
        od2 = qBound(0.1f, 2.0f / beta, 6.0f);
        od3 = qBound(0.05f, 0.5f / beta, 4.0f);
    }
    levels.append({ +od3, QColor(235, 235,  50, qMin(255, int(aBase * 0.65f))) });
    levels.append({ +od2, QColor(255, 140,  30, qMin(255, int(aBase * 0.78f))) });
    levels.append({ +od1, QColor(255,  40,  40, qMin(255, int(aBase * 0.90f))) });

    // Find max force for length normalisation
    float maxF = 0.0f;
    for (const auto& lv : levels)
        maxF = qMax(maxF, wallForceMag(lv.dist, m_wallHarmonic, m_wallBeta));
    if (maxF < 1e-6f) maxF = 1.0f;

    const float maxLen = 1.5f;  // Å, longest arrow
    const float minLen = 0.15f; // Å, shortest visible arrow

    // Accumulate shaft + tip segments directly
    QVector<BondInstancing::Segment> shafts, tips;

    auto pushArrow = [&](const QVector3D& origin, const QVector3D& dir, float fmag, const QColor& c) {
        const float norm = qMin(1.0f, fmag / maxF);
        if (norm < 0.015f) return;
        const float len = minLen + norm * (maxLen - minLen);
        const QQuaternion rot = bondRotation(dir);
        const float sr = len * 0.040f;
        const float shaftLen = len * 0.78f;
        const float tipLen   = len * 0.22f;

        BondInstancing::Segment shaft;
        shaft.center   = origin + dir * (shaftLen * 0.5f);
        shaft.scale    = QVector3D(sr / 50.0f, shaftLen / 100.0f, sr / 50.0f);
        shaft.rotation = rot;
        shaft.color    = c;
        shafts.append(shaft);

        BondInstancing::Segment tip;
        tip.center   = origin + dir * (shaftLen + tipLen * 0.5f);
        tip.scale    = QVector3D(sr * 2.4f / 50.0f, tipLen / 100.0f, sr * 2.4f / 50.0f);
        tip.rotation = rot;
        tip.color    = c;
        tips.append(tip);
    };

    if (m_wallGeom == 1) {
        // Sphere: lat/lon grid on each shell radius, arrows pointing radially inward.
        for (const auto& lv : levels) {
            const float r = m_wallRadius + lv.dist;
            if (r < 0.1f) continue;
            const float fmag = wallForceMag(lv.dist, m_wallHarmonic, m_wallBeta);
            for (int ilat = 0; ilat < res; ++ilat) {
                const float theta = float(M_PI) * (ilat + 0.5f) / float(res);
                for (int ilon = 0; ilon < 2 * res; ++ilon) {
                    const float phi = 2.0f * float(M_PI) * float(ilon) / float(2 * res);
                    const QVector3D P(r * std::sin(theta) * std::cos(phi),
                                     r * std::sin(theta) * std::sin(phi),
                                     r * std::cos(theta));
                    pushArrow(P, -P.normalized(), fmag, lv.color);
                }
            }
        }
    } else if (m_wallGeom == 2) {
        // Box: sample res×res on each of 6 faces; force perpendicular to face (inward).
        const QVector3D mn = m_wallMin, mx = m_wallMax;
        for (const auto& lv : levels) {
            const float d = lv.dist;
            const float fmag = wallForceMag(d, m_wallHarmonic, m_wallBeta);
            auto sampleFace = [&](float faceCoord, int axis, int sign) {
                float u0, u1, v0, v1;
                switch (axis) {
                    case 0: u0=mn.y(); u1=mx.y(); v0=mn.z(); v1=mx.z(); break;
                    case 1: u0=mn.x(); u1=mx.x(); v0=mn.z(); v1=mx.z(); break;
                    default: u0=mn.x(); u1=mx.x(); v0=mn.y(); v1=mx.y(); break;
                }
                for (int i = 0; i < res; ++i) {
                    for (int j = 0; j < res; ++j) {
                        const float u = u0 + (i + 0.5f) * (u1 - u0) / float(res);
                        const float v = v0 + (j + 0.5f) * (v1 - v0) / float(res);
                        QVector3D P, dir;
                        if (axis == 0)      { P = QVector3D(faceCoord, u, v); dir = QVector3D(-float(sign), 0, 0); }
                        else if (axis == 1) { P = QVector3D(u, faceCoord, v); dir = QVector3D(0, -float(sign), 0); }
                        else                { P = QVector3D(u, v, faceCoord); dir = QVector3D(0, 0, -float(sign)); }
                        pushArrow(P, dir, fmag, lv.color);
                    }
                }
            };
            sampleFace(mx.x() + d, 0, +1); sampleFace(mn.x() - d, 0, -1);
            sampleFace(mx.y() + d, 1, +1); sampleFace(mn.y() - d, 1, -1);
            sampleFace(mx.z() + d, 2, +1); sampleFace(mn.z() - d, 2, -1);
        }
    }
    m_wallForceShafts->setSegments(shafts);
    m_wallForceTips->setSegments(tips);
}

QQuick3DInstancing* SceneController::atomInstancing() const { return m_atomInstancing; }
QQuick3DInstancing* SceneController::bondInstancing() const { return m_bondInstancing; }
QQuick3DInstancing* SceneController::arrowShaftInstancing() const { return m_arrowShaft; }
QQuick3DInstancing* SceneController::arrowTipInstancing() const { return m_arrowTip; }

void SceneController::setForceVectorsVisible(bool on)
{
    if (m_forceVectorsVisible == on)
        return;
    m_forceVectorsVisible = on;
    if (!on)
        setForceArrows({}); // clear when turned off
    emit forceVectorsChanged();
}

void SceneController::setForceArrows(const QVector<Arrow>& arrows)
{
    // Each arrow = a #Cylinder shaft + a #Cone tip, oriented local +Y -> dir.
    QVector<BondInstancing::Segment> shafts;
    QVector<BondInstancing::Segment> tips;
    shafts.reserve(arrows.size());
    tips.reserve(arrows.size());
    for (const Arrow& a : arrows) {
        if (a.length < 1e-3f || a.dir.lengthSquared() < 1e-8f)
            continue;
        const QVector3D dir = a.dir.normalized();
        const QQuaternion rot = bondRotation(dir);
        const float shaftLen = a.length * 0.78f;
        const float tipLen = a.length * 0.22f;
        const float shaftR = qMax(0.04f, a.length * 0.045f);
        const float tipR = shaftR * 2.4f;

        // #Cylinder / #Cone: 100 tall (y in [-50,50]), base radius 50.
        BondInstancing::Segment shaft;
        shaft.center = a.origin + dir * (shaftLen * 0.5f);
        shaft.scale = QVector3D(shaftR / 50.0f, shaftLen / 100.0f, shaftR / 50.0f);
        shaft.rotation = rot;
        shaft.color = a.color;
        shafts.append(shaft);

        BondInstancing::Segment tip;
        tip.center = a.origin + dir * (shaftLen + tipLen * 0.5f);
        tip.scale = QVector3D(tipR / 50.0f, tipLen / 100.0f, tipR / 50.0f);
        tip.rotation = rot;
        tip.color = a.color;
        tips.append(tip);
    }
    m_arrowShaft->setSegments(shafts);
    m_arrowTip->setSegments(tips);
}

void SceneController::setStructure(const QVector<AtomDatum>& atoms, const QVector<BondDatum>& bonds,
    bool keepView)
{
    m_atoms = atoms;
    m_bonds = bonds;
    m_fragmentsDirty = true;
    if (keepView) {
        // Structure editing: atom count changed but keep the current view. Don't
        // recompute bounds (that would shift a rotated molecule) or reset the camera;
        // the caller manages the selection.
        rebuildGeometry();
        emit structureChanged();
        return;
    }
    m_selection.clear();
    m_collisionAtoms.clear();
    clearOverlay();             // a fresh primary structure drops any RMSD overlay
    recomputeBounds();
    rebuildGeometry();
    resetView();
    emit structureChanged();
}

void SceneController::updatePositions(const QVector<QVector3D>& positions)
{
    // Fast path for live simulation: same atom count, reuse colours/bonds.
    const int n = qMin(positions.size(), m_atoms.size());
    for (int i = 0; i < n; ++i)
        m_atoms[i].position = positions[i];
    rebuildGeometry();
    if (m_labelMode != 0)
        rebuildLabels();  // labels track live MD/Opt positions
}

// Claude Generated 2026 - replace the bond list and rebuild geometry only. No bounds recompute or
// camera reset, so a live bond break/formation (dynamic bonds during MD/Opt) does not jolt the view.
void SceneController::updateBonds(const QVector<BondDatum>& bonds)
{
    m_bonds = bonds;
    m_fragmentsDirty = true;   // bond breaking/forming splits or merges fragments
    rebuildGeometry();
}

void SceneController::clear()
{
    m_atoms.clear();
    m_bonds.clear();
    m_selection.clear();
    m_fragmentsDirty = true;
    rebuildGeometry();
    emit structureChanged();
}

void SceneController::recomputeBounds()
{
    if (m_atoms.isEmpty()) {
        m_sceneCenter = QVector3D();
        m_sceneExtent = 10.0f;
        return;
    }
    QVector3D lo(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max());
    QVector3D hi = -lo;
    for (const AtomDatum& a : m_atoms) {
        lo.setX(qMin(lo.x(), a.position.x()));
        lo.setY(qMin(lo.y(), a.position.y()));
        lo.setZ(qMin(lo.z(), a.position.z()));
        hi.setX(qMax(hi.x(), a.position.x()));
        hi.setY(qMax(hi.y(), a.position.y()));
        hi.setZ(qMax(hi.z(), a.position.z()));
    }
    m_sceneCenter = 0.5f * (lo + hi);
    m_sceneExtent = qMax(2.0f, 0.5f * (hi - lo).length());
}

// Base colour for an element/charge under the current scheme, ignoring the transient
// selection/hover/collision state. Shared by atomColor() and the overlay tint path.
// Claude Generated 2026 - Stable, distinct colour per bead/residue type. Uses a
// deterministic FNV-1a hash (not qHash, which is per-process seeded) so a given
// type always maps to the same hue across runs. Spreads hues by the golden angle.
QColor SceneController::typeColor(const QString& type) const
{
    // A colour the user picked for this type wins over the derived hue.
    const auto it = m_typeColors.constFind(type);
    if (it != m_typeColors.constEnd() && it.value().isValid())
        return it.value();

    uint h = 2166136261u;
    for (const QChar c : type) { h ^= c.unicode(); h *= 16777619u; }
    const float hue = (h % 3600) / 3600.0f;             // 0.0 .. 1.0
    const float sat = 0.55f + (h >> 8 & 0x3) * 0.10f;   // 0.55 .. 0.85
    return QColor::fromHsvF(hue, qBound(0.0f, sat, 1.0f), 0.90f);
}

// Claude Generated 2026 - Which bead types the loaded structure actually contains,
// and how many beads each. Drives the Display panel's per-type colour selector:
// the list is built from the structure, not from a fixed table.
QVector<QPair<QString, int>> SceneController::beadTypes() const
{
    QHash<QString, int> counts;
    for (const AtomDatum& a : m_atoms) {
        if (!a.type.isEmpty())
            ++counts[a.type];
    }
    QVector<QPair<QString, int>> out;
    out.reserve(counts.size());
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
        out.append({ it.key(), it.value() });
    std::sort(out.begin(), out.end(),
        [](const QPair<QString, int>& a, const QPair<QString, int>& b) { return a.first < b.first; });
    return out;
}

void SceneController::setTypeColorOverride(const QString& type, const QColor& color)
{
    if (type.isEmpty())
        return;
    if (color.isValid())
        m_typeColors.insert(type, color);
    else
        m_typeColors.remove(type);
    // Colours reach atoms, bonds and overlays through schemeColor(), so this is a
    // full geometry rebuild rather than the atoms-only path.
    rebuildGeometry();
}

void SceneController::clearTypeColorOverrides()
{
    if (m_typeColors.isEmpty())
        return;
    m_typeColors.clear();
    rebuildGeometry();
}

void SceneController::setTypeColorOverrides(const QHash<QString, QColor>& overrides)
{
    if (m_typeColors == overrides)
        return;
    m_typeColors = overrides;
    rebuildGeometry();
}

// Claude Generated 2026 - Fragments = connected components of the bond graph.
// Ordered by descending atom count, so fragment 0 is the largest: in a host-guest
// complex that is the host, and leaving it untinted is what makes the guests read
// as guests. Recomputed lazily; every structure or bond change marks it dirty.
void SceneController::ensureFragments() const
{
    if (!m_fragmentsDirty)
        return;
    m_fragmentsDirty = false;
    m_fragmentOf.assign(m_atoms.size(), -1);
    m_fragmentInfo.clear();
    if (m_atoms.isEmpty())
        return;

    QVector<QVector<int>> adjacency(m_atoms.size());
    for (const BondDatum& b : m_bonds) {
        if (b.a >= 0 && b.a < m_atoms.size() && b.b >= 0 && b.b < m_atoms.size()) {
            adjacency[b.a].append(b.b);
            adjacency[b.b].append(b.a);
        }
    }

    QVector<QVector<int>> components;
    QVector<int> raw(m_atoms.size(), -1);
    for (int start = 0; start < m_atoms.size(); ++start) {
        if (raw[start] >= 0)
            continue;
        const int id = components.size();
        QVector<int> members { start };
        QVector<int> stack { start };
        raw[start] = id;
        while (!stack.isEmpty()) {
            const int atom = stack.takeLast();
            for (int nb : adjacency[atom]) {
                if (raw[nb] < 0) {
                    raw[nb] = id;
                    members.append(nb);
                    stack.append(nb);
                }
            }
        }
        components.append(members);
    }

    // Largest first; ties keep file order so the mapping is reproducible.
    QVector<int> order(components.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&components](int lhs, int rhs) {
        if (components[lhs].size() != components[rhs].size())
            return components[lhs].size() > components[rhs].size();
        return components[lhs].first() < components[rhs].first();
    });

    for (int rank = 0; rank < order.size(); ++rank) {
        const QVector<int>& members = components[order[rank]];
        for (int atom : members)
            m_fragmentOf[atom] = rank;

        // Hill notation (C, then H, then the rest alphabetically) so a fragment is
        // recognisable as "the host" or "the guest" at a glance. Coarse-grained
        // beads have no element, so they are counted by their type label instead.
        QMap<QString, int> counts;
        bool anyElement = false;
        for (int atom : members) {
            const QString& e = m_atoms[atom].element;
            if (!e.isEmpty()) {
                ++counts[e];
                anyElement = true;
            } else if (!m_atoms[atom].type.isEmpty()) {
                ++counts[m_atoms[atom].type];
            }
        }
        QString formula;
        const auto append = [&formula](const QString& symbol, int n) {
            formula += n > 1 ? QStringLiteral("%1%2").arg(symbol).arg(n) : symbol;
        };
        if (anyElement) {
            if (counts.contains(QStringLiteral("C")))
                append(QStringLiteral("C"), counts.take(QStringLiteral("C")));
            if (counts.contains(QStringLiteral("H")))
                append(QStringLiteral("H"), counts.take(QStringLiteral("H")));
        }
        for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
            append(it.key(), it.value());

        m_fragmentInfo.append({ formula, int(members.size()) });
    }
}

QVector<SceneController::FragmentInfo> SceneController::fragments() const
{
    ensureFragments();
    return m_fragmentInfo;
}

QColor SceneController::fragmentColor(int fragment) const
{
    if (fragment <= 0)
        return QColor();   // the largest fragment is the untinted reference
    const auto it = m_fragmentColors.constFind(fragment);
    if (it != m_fragmentColors.constEnd() && it.value().isValid())
        return it.value();
    return automaticFragmentTint(fragment);
}

void SceneController::setFragmentTint(bool on, float defaultStrength)
{
    const float clamped = qBound(0.0f, defaultStrength, 1.0f);
    if (m_fragmentTint == on && qFuzzyCompare(m_fragmentTintStrength + 1.0f, clamped + 1.0f))
        return;
    m_fragmentTint = on;
    m_fragmentTintStrength = clamped;
    rebuildGeometry();
}

void SceneController::setFragmentColorOverride(int fragment, const QColor& color)
{
    if (fragment <= 0)
        return;
    if (color.isValid())
        m_fragmentColors.insert(fragment, color);
    else
        m_fragmentColors.remove(fragment);
    rebuildGeometry();
}

bool SceneController::hasFragmentColorOverride(int fragment) const
{
    const auto it = m_fragmentColors.constFind(fragment);
    return it != m_fragmentColors.constEnd() && it.value().isValid();
}

void SceneController::clearFragmentColorOverrides()
{
    if (m_fragmentColors.isEmpty())
        return;
    m_fragmentColors.clear();
    rebuildGeometry();
}

void SceneController::setFragmentScale(float nonReferenceScale)
{
    const float clamped = qBound(0.1f, nonReferenceScale, 3.0f);
    if (qFuzzyCompare(m_fragmentScale + 1.0f, clamped + 1.0f))
        return;
    m_fragmentScale = clamped;
    rebuildGeometry();
}

float SceneController::fragmentScaleFor(int fragment) const
{
    const auto it = m_fragmentScales.constFind(fragment);
    if (it != m_fragmentScales.constEnd() && it.value() > 0.0f)
        return it.value();
    // The reference fragment keeps its size unless the user says otherwise, so a
    // complex opens up by shrinking the guests rather than by rescaling everything.
    return fragment <= 0 ? 1.0f : m_fragmentScale;
}

void SceneController::setFragmentScaleOverride(int fragment, float scale)
{
    if (fragment < 0)
        return;
    if (scale > 0.0f)
        m_fragmentScales.insert(fragment, qBound(0.1f, scale, 3.0f));
    else
        m_fragmentScales.remove(fragment);
    rebuildGeometry();
}

void SceneController::clearFragmentScaleOverrides()
{
    if (m_fragmentScales.isEmpty())
        return;
    m_fragmentScales.clear();
    rebuildGeometry();
}

float SceneController::atomDrawRadiusFor(int atomIndex) const
{
    if (atomIndex < 0 || atomIndex >= m_atoms.size())
        return 0.0f;
    const float base = atomDrawRadius(m_atoms[atomIndex]);
    ensureFragments();
    if (m_fragmentInfo.size() < 2 || atomIndex >= m_fragmentOf.size())
        return base;
    return base * fragmentScaleFor(m_fragmentOf[atomIndex]);
}

float SceneController::fragmentTintStrengthFor(int fragment) const
{
    if (fragment <= 0)
        return 0.0f;   // the reference fragment is never tinted
    const auto it = m_fragmentStrengths.constFind(fragment);
    if (it != m_fragmentStrengths.constEnd() && it.value() >= 0.0f)
        return it.value();
    return m_fragmentTintStrength;
}

void SceneController::setFragmentTintStrengthOverride(int fragment, float strength)
{
    if (fragment <= 0)
        return;
    if (strength >= 0.0f)
        m_fragmentStrengths.insert(fragment, qBound(0.0f, strength, 1.0f));
    else
        m_fragmentStrengths.remove(fragment);
    rebuildGeometry();
}

void SceneController::clearFragmentOverrides()
{
    if (m_fragmentColors.isEmpty() && m_fragmentScales.isEmpty() && m_fragmentStrengths.isEmpty())
        return;
    m_fragmentColors.clear();
    m_fragmentScales.clear();
    m_fragmentStrengths.clear();
    rebuildGeometry();
}

QColor SceneController::applyFragmentTint(const QColor& base, int atomIndex) const
{
    if (!m_fragmentTint)
        return base;
    ensureFragments();
    if (m_fragmentInfo.size() < 2 || atomIndex < 0 || atomIndex >= m_fragmentOf.size())
        return base;   // a single fragment has nothing to be distinguished from
    const int fragment = m_fragmentOf[atomIndex];
    if (fragment <= 0)
        return base;
    return tintFragmentColor(base, fragmentColor(fragment), fragmentTintStrengthFor(fragment));
}

QColor SceneController::schemeColorFor(int atomIndex) const
{
    if (atomIndex < 0 || atomIndex >= m_atoms.size())
        return QColor();
    return applyFragmentTint(schemeColor(m_atoms[atomIndex]), atomIndex);
}

QColor SceneController::schemeColor(const AtomDatum& a) const
{
    switch (m_colorScheme) {
    case Monochrome:
        return m_monochrome;
    case ByCharge: {
        const float q = qBound(-1.0f, a.charge, 1.0f);
        return (q >= 0) ? QColor::fromRgbF(1.0, 1.0 - q, 1.0 - q)  // white -> red
                        : QColor::fromRgbF(1.0 + q, 1.0 + q, 1.0); // white -> blue
    }
    case ByType:
        // Coarse-grained beads have no element; colour by type. Real atoms (no
        // type label) fall back to CPK so the scheme is harmless for them.
        return a.type.isEmpty() ? elem::cpkColor(a.element) : typeColor(a.type);
    case CPK:
    case Custom:
    default:
        return elem::cpkColor(a.element);
    }
}

// Claude Generated 2026 - Per-atom draw radius: honour an explicit radius (VTF
// coarse-grained beads carry their own), else fall back to the element vdW table.
float SceneController::atomDrawRadius(const AtomDatum& a) const
{
    return a.radius > 0.0f ? a.radius : elem::vdwRadius(a.element);
}

QColor SceneController::atomColor(int index) const
{
    // Collision wins over everything (red), then selection (magenta); otherwise
    // scheme-based colour. Alpha from transparency.
    if (m_collisionAtoms.contains(index)) {
        QColor r(235, 60, 60);
        r.setAlphaF(1.0f);
        return r;
    }
    if (m_selection.contains(index)) {
        QColor h(255, 0, 255);
        h.setAlphaF(1.0f);
        return h;
    }
    // Hover feedback: brighten the atom under the cursor (below selection).
    if (index == m_hoverAtom) {
        QColor hl = schemeColorFor(index).lighter(170);
        hl.setAlphaF(m_transparency);
        return hl;
    }
    QColor c = schemeColorFor(index);
    c.setAlphaF(m_transparency);
    return c;
}

// Atoms only — cheap path for selection/hover changes (skips the bonds rebuild).
void SceneController::rebuildAtoms()
{
    QVector<AtomInstancing::Item> items;
    if (m_atomsVisible && m_primaryVisible) {
        // Ball-and-stick shrinks spheres; space-filling uses full vdW radius.
        const float radiusFactor = (m_renderingMode == SpaceFilling) ? 1.0f : 0.30f;
        items.reserve(m_atoms.size());
        for (int i = 0; i < m_atoms.size(); ++i) {
            AtomInstancing::Item it;
            it.position = m_atoms[i].position;
            it.scale = radiusFactor * m_atomScaleFactor * atomDrawRadiusFor(i);
            it.color = atomColor(i);
            items.append(it);
        }
    }
    m_atomInstancing->setItems(items);
}

void SceneController::setHoverAtom(int index)
{
    if (index >= m_atoms.size())
        index = -1;
    if (index == m_hoverAtom)
        return;
    m_hoverAtom = index;
    rebuildAtoms();
}

void SceneController::rebuildGeometry()
{
    rebuildAtoms();

    // --- bonds (two half-cylinders, coloured per atom) ---
    QVector<BondInstancing::Segment> segs;
    if (m_bondsVisible && m_primaryVisible) {
        const float bondRadius = (m_renderingMode == Wireframe)
            ? qMin(m_bondRadius, 0.06f)
            : m_bondRadius;
        segs.reserve(m_bonds.size() * 2);
        ensureFragments();
        const bool scaleByFragment = m_fragmentInfo.size() > 1;
        for (const BondDatum& b : m_bonds) {
            if (b.a < 0 || b.b < 0 || b.a >= m_atoms.size() || b.b >= m_atoms.size())
                continue;
            const QVector3D posA = m_atoms[b.a].position;
            const QVector3D posB = m_atoms[b.b].position;
            const QVector3D dir = posB - posA;
            const float length = dir.length();
            if (length < 1e-4f)
                continue;
            const QQuaternion rot = bondRotation(dir / length);
            const QVector3D mid = 0.5f * (posA + posB);
            const float halfLength = length * 0.25f;
            // A bond always connects two atoms of the same fragment, so its
            // thickness follows that fragment's scale unambiguously.
            const float bondScale = scaleByFragment && b.a < m_fragmentOf.size()
                ? fragmentScaleFor(m_fragmentOf[b.a])
                : 1.0f;
            const float sxz = bondRadius * bondScale / kCylBaseRadius;
            const QVector3D scale(sxz, halfLength / kCylBaseHalfHeight, sxz);

            QColor cA = schemeColorFor(b.a);
            QColor cB = schemeColorFor(b.b);
            cA.setAlphaF(m_transparency);
            cB.setAlphaF(m_transparency);

            segs.append({ 0.5f * (posA + mid), scale, rot, cA });
            segs.append({ 0.5f * (mid + posB), scale, rot, cB });
        }
    }
    m_bondInstancing->setSegments(segs);

    // Overlays inherit the global styles just touched here; repack them too (cheap
    // early-return when there are none, so the MD updatePositions path stays fast).
    rebuildOverlays();

    // The NCI dashes are trimmed to the drawn sphere surfaces, so a rendering-mode
    // or atom-scale change moves their endpoints. Cheap no-op when nothing is shown.
    rebuildNci();
}

// ---- appearance setters ----
void SceneController::setColorScheme(int scheme)
{
    if (m_colorScheme == scheme)
        return;
    m_colorScheme = scheme;
    rebuildGeometry();
}

void SceneController::setMonochromeColor(const QColor& c)
{
    m_monochrome = c;
    if (m_colorScheme == Monochrome)
        rebuildGeometry();
}

void SceneController::setPrimaryVisible(bool on)
{
    if (m_primaryVisible == on)
        return;
    m_primaryVisible = on;
    rebuildGeometry();
}

void SceneController::setHighQualityAA(bool on)
{
    if (m_highQualityAA == on)
        return;
    m_highQualityAA = on;
    emit appearanceChanged();  // QML re-binds the SceneEnvironment AA mode/quality
}

void SceneController::setTransparentBackground(bool on)
{
    if (m_transparentBackground == on)
        return;
    m_transparentBackground = on;
    emit appearanceChanged();  // QML re-binds the SceneEnvironment background mode
}

void SceneController::cloneStateFrom(const SceneController* src)
{
    if (!src)
        return;
    // Geometry + overlays (transient state — selection/hover/collision/measurement/force
    // vectors — is intentionally NOT copied, for a clean export image).
    m_atoms = src->m_atoms;
    m_bonds = src->m_bonds;
    m_overlays = src->m_overlays;

    // Appearance
    m_colorScheme = src->m_colorScheme;
    m_renderingMode = src->m_renderingMode;
    m_monochrome = src->m_monochrome;
    m_background = src->m_background;
    m_atomScaleFactor = src->m_atomScaleFactor;
    m_bondRadius = src->m_bondRadius;
    m_transparency = src->m_transparency;
    m_atomsVisible = src->m_atomsVisible;
    m_bondsVisible = src->m_bondsVisible;
    m_primaryVisible = src->m_primaryVisible;

    // Effects
    m_ssao = src->m_ssao;
    m_ssaoStrength = src->m_ssaoStrength;
    m_bloom = src->m_bloom;
    m_hdr = src->m_hdr;
    m_exposure = src->m_exposure;
    m_tonemap = src->m_tonemap;
    m_fog = src->m_fog;
    m_fogDensity = src->m_fogDensity;
    m_fogDistance = src->m_fogDistance;
    m_shadows = src->m_shadows;
    for (int i = 0; i < 4; ++i)
        m_corner[i] = src->m_corner[i];

    // Camera / transform (copied directly so the export frames exactly as on screen)
    m_sceneCenter = src->m_sceneCenter;
    m_sceneExtent = src->m_sceneExtent;
    m_rootRotation = src->m_rootRotation;
    m_cameraDistance = src->m_cameraDistance;
    m_pan = src->m_pan;
    m_fov = src->m_fov;

    // Confinement walls
    m_wallVisible = src->m_wallVisible;
    m_wallOpacity = src->m_wallOpacity;
    m_wallGeom = src->m_wallGeom;
    m_wallMin = src->m_wallMin;
    m_wallMax = src->m_wallMax;
    m_wallRadius = src->m_wallRadius;
    m_wallColor = src->m_wallColor;
    m_potVizEnabled = src->m_potVizEnabled;
    m_wallHarmonic = src->m_wallHarmonic;
    m_wallTemp = src->m_wallTemp;
    m_wallBeta = src->m_wallBeta;
    m_potArrowsEnabled = src->m_potArrowsEnabled;
    m_potArrowResolution = src->m_potArrowResolution;

    m_typeColors = src->m_typeColors;   // per-bead-type colours belong to the look
    m_fragmentTint = src->m_fragmentTint;
    m_fragmentTintStrength = src->m_fragmentTintStrength;
    m_fragmentColors = src->m_fragmentColors;
    m_fragmentScale = src->m_fragmentScale;
    m_fragmentScales = src->m_fragmentScales;
    m_fragmentStrengths = src->m_fragmentStrengths;
    m_fragmentsDirty = true;

    // Non-covalent interaction overlay: a deliberate display option like the walls,
    // so an exported image shows it too (unlike the transient interaction hints).
    m_nciSegments = src->m_nciSegments;
    m_nciVisible = src->m_nciVisible;
    m_nciLabelsVisible = src->m_nciLabelsVisible;

    rebuildGeometry();          // atoms + bonds + overlays
    rebuildWall();
    rebuildWallVectorField();
    rebuildNci();
    emit appearanceChanged();
    emit effectsChanged();
    emit structureChanged();
    emit transformChanged();
    emit overlayChanged();
    emit wallChanged();
    emit nciChanged();
}

// ---- Non-covalent interaction overlay (Claude Generated 2026) ----
//
// The contact lines are supplied in intrinsic atom coordinates, so the QML Model
// sits under moleculeRoot and rotates/zooms with the structure without any C++
// recompute. Each line is drawn as a row of short cylinders (see appendDashedLine)
// and carries its alpha inside the per-instance colour - the QML material must be
// alphaMode: Blend, or PrincipledMaterial's Opaque default discards it silently
// (the same trap the wall wireframe documents).

QQuick3DInstancing* SceneController::nciInstancing() const { return m_nciLines; }

void SceneController::setNciContacts(const QVector<NciSegment>& contacts)
{
    m_nciSegments = contacts;
    rebuildNci();
}

void SceneController::setNciVisible(bool on)
{
    if (m_nciVisible == on)
        return;
    m_nciVisible = on;
    rebuildNci();
}

void SceneController::setNciLabelsVisible(bool on)
{
    if (m_nciLabelsVisible == on)
        return;
    m_nciLabelsVisible = on;
    rebuildNci();
}

void SceneController::rebuildNci()
{
    QVector<BondInstancing::Segment> segs;
    QVariantList labels;

    if (m_nciVisible && !m_nciSegments.isEmpty()) {
        // Space Filling draws full van der Waals spheres, so a line between the atom
        // centres would vanish inside them. Pull each end back to the drawn sphere
        // surface, using the radius the atoms are actually rendered with.
        const float radiusFactor = (m_renderingMode == SpaceFilling) ? 1.0f : 0.30f;
        const auto drawnRadius = [&](int index) -> float {
            if (!m_atomsVisible || index < 0 || index >= m_atoms.size())
                return 0.0f;
            return radiusFactor * m_atomScaleFactor * atomDrawRadiusFor(index);
        };

        for (const NciSegment& c : m_nciSegments) {
            QVector3D from = c.a;
            QVector3D to = c.b;
            const QVector3D delta = to - from;
            const float length = delta.length();
            if (length < 1e-3f)
                continue;
            const QVector3D dir = delta / length;

            const float trimA = drawnRadius(c.atomA);
            const float trimB = drawnRadius(c.atomB);
            // Keep a visible stub if the two spheres almost touch.
            if (trimA + trimB < length - 0.15f) {
                from += dir * trimA;
                to -= dir * trimB;
            }

            appendDashedLine(segs, from, to, c.radius, c.color);

            if (m_nciLabelsVisible && !c.label.isEmpty()) {
                const QVector3D mid = 0.5f * (c.a + c.b);
                QVariantMap m;
                m["px"] = mid.x();
                m["py"] = mid.y();
                m["pz"] = mid.z();
                m["text"] = c.label;
                QColor bright = c.color;
                bright.setAlpha(255);
                m["color"] = bright.name();
                labels.append(m);
            }
        }
    }

    m_nciLines->setSegments(segs);
    m_nciLabels = labels;
    emit nciChanged();
}

void SceneController::setRenderingMode(int mode)
{
    m_renderingMode = mode;
    m_atomsVisible = (mode == BallAndStick || mode == SpaceFilling);
    m_bondsVisible = (mode != SpaceFilling);
    rebuildGeometry();
    emit appearanceChanged();
}

void SceneController::setAtomScaleFactor(float s)
{
    m_atomScaleFactor = s;
    rebuildGeometry();
}

void SceneController::setBondThickness(float r)
{
    m_bondRadius = r;
    rebuildGeometry();
}

void SceneController::setAtomTransparency(float a)
{
    m_transparency = qBound(0.0f, a, 1.0f);
    rebuildGeometry();
    emit appearanceChanged(); // blendEnabled may have flipped -> update material alphaMode
}

void SceneController::setBackgroundColor(const QColor& c)
{
    if (m_background == c)
        return;
    m_background = c;
    emit appearanceChanged();
}

void SceneController::setSelection(const QVector<int>& indices)
{
    m_selection = indices;
    rebuildAtoms(); // only atom colours change
    if (m_labelSelectionOnly && m_labelMode != 0)
        rebuildLabels();  // selection-scoped labels follow the selection
}

// Claude Generated 2026 - Per-atom overlay labels. Builds the {position,text} list;
// viewer3d.qml projects each entry to screen reactively, so rotation/zoom/pan need
// no recompute here — only structure/selection/mode changes rebuild the list.
void SceneController::setLabelMode(int mode)
{
    if (m_labelMode == mode)
        return;
    m_labelMode = mode;
    rebuildLabels();
}

void SceneController::setLabelSelectionOnly(bool on)
{
    if (m_labelSelectionOnly == on)
        return;
    m_labelSelectionOnly = on;
    rebuildLabels();
}

void SceneController::rebuildLabels()
{
    QVariantList out;
    if (m_labelMode != 0 && !m_atoms.isEmpty()) {
        auto labelFor = [this](int i) -> QString {
            const AtomDatum& a = m_atoms[i];
            switch (m_labelMode) {
            case 1: return a.element.isEmpty() ? a.type : a.element;  // Element
            case 2: return a.type;                                    // Type
            case 3: return QString::number(i);                        // Index
            default: return QString();
            }
        };
        auto add = [&](int i) {
            const QString t = labelFor(i);
            if (t.isEmpty())
                return;
            const QVector3D& p = m_atoms[i].position;
            QVariantMap m;
            m["px"] = p.x(); m["py"] = p.y(); m["pz"] = p.z();
            m["text"] = t;
            out.append(m);
        };
        if (m_labelSelectionOnly) {
            for (int idx : m_selection)
                if (idx >= 0 && idx < m_atoms.size())
                    add(idx);
        } else {
            for (int i = 0; i < m_atoms.size(); ++i)
                add(i);
        }
    }
    m_atomLabels = out;
    emit labelsChanged();
}

// Claude Generated 2026 - Structure editing: recolour clashing atoms red. Cheap
// atoms-only rebuild, mirroring setSelection.
void SceneController::setCollisionAtoms(const QVector<int>& indices)
{
    m_collisionAtoms = indices;
    rebuildAtoms();
}

// Claude Generated 2026 - Rubber-band overlay rect (viewport pixels); QML draws it.
void SceneController::setRubberBand(const QRectF& rectPx, bool active)
{
    if (m_rubberBandActive == active && m_rubberBandRect == rectPx)
        return;
    m_rubberBandActive = active;
    m_rubberBandRect = rectPx;
    emit rubberBandChanged();
}

// Claude Generated 2026 - Edit-mode hint text (empty = hidden); QML draws a 2D HUD.
void SceneController::setEditHint(const QString& text)
{
    if (m_editHint == text)
        return;
    m_editHint = text;
    emit editHintChanged();
}

// ---- effects setters ----
void SceneController::setSsao(bool on, float strength)
{
    m_ssao = on;
    m_ssaoStrength = strength;
    emit effectsChanged();
}
void SceneController::setBloom(bool on) { m_bloom = on; emit effectsChanged(); }
void SceneController::setHdr(bool on) { m_hdr = on; emit effectsChanged(); }
void SceneController::setExposure(float e) { m_exposure = e; emit effectsChanged(); }
void SceneController::setTonemap(bool on) { m_tonemap = on; emit effectsChanged(); }
void SceneController::setFog(bool on, float density)
{
    m_fog = on;
    m_fogDensity = density;
    emit effectsChanged();
}
void SceneController::setFogDistance(float d)
{
    m_fogDistance = qBound(0.0f, d, 1.0f);
    emit effectsChanged();
}
void SceneController::setShadows(bool on) { m_shadows = on; emit effectsChanged(); }
void SceneController::setCornerLight(int index, bool on)
{
    if (index < 0 || index > 3 || m_corner[index] == on)
        return;
    m_corner[index] = on;
    emit effectsChanged();
}

// ---- camera transform ----
void SceneController::setRootRotation(const QQuaternion& q)
{
    m_rootRotation = q;
    emit transformChanged();
}

void SceneController::setRootRotationOnly(const QQuaternion& rotation)
{
    if (m_rootRotation == rotation)
        return;
    m_rootRotation = rotation;
    emit transformChanged();
}
void SceneController::setCameraDistance(float d)
{
    m_cameraDistance = qMax(0.5f, d);
    emit transformChanged();
}
void SceneController::setPan(const QVector3D& pan)
{
    m_pan = pan;
    emit transformChanged();
}

void SceneController::setCameraTransform(const QQuaternion& rotation, const QVector3D& pan, float distance)
{
    bool changed = false;
    if (m_rootRotation != rotation) {
        m_rootRotation = rotation;
        changed = true;
    }
    if (m_pan != pan) {
        m_pan = pan;
        changed = true;
    }
    const float clampedDistance = qMax(0.5f, distance);
    if (qAbs(m_cameraDistance - clampedDistance) > 1e-6f) {
        m_cameraDistance = clampedDistance;
        changed = true;
    }
    if (changed)
        emit transformChanged();
}

void SceneController::resetView()
{
    m_rootRotation = QQuaternion();
    m_pan = QVector3D();
    m_cameraDistance = m_sceneExtent * 3.0f;
    emit transformChanged();
}
// Claude Generated 2026 - shift the currently displayed atoms so the mass-weighted
// centre-of-mass lands at the origin, then reset the camera to a clean framing.
// Uses curcuma Elements::AtomicMass / Elements::String2Element for consistent masses.
void SceneController::centerAtOrigin()
{
    if (m_atoms.isEmpty()) return;
    QVector3D com;
    float totalMass = 0.0f;
    for (const AtomDatum& a : m_atoms) {
        const int z = Elements::String2Element(a.element.toLower().toStdString());
        const float mass = (z > 0 && z < static_cast<int>(Elements::AtomicMass.size()))
            ? static_cast<float>(Elements::AtomicMass[z]) : 12.011f;
        com += a.position * mass;
        totalMass += mass;
    }
    if (totalMass > 0.0f) com /= totalMass;
    for (AtomDatum& a : m_atoms)
        a.position -= com;
    recomputeBounds();
    rebuildGeometry();
    resetView();
    emit structureChanged();
}

void SceneController::fitToBounds(const QVector3D& center, float radius)
{
    m_pan = center - m_sceneCenter;
    m_cameraDistance = qMax(2.0f, radius) * 3.0f;
    emit transformChanged();
}

// ---- picking / grab math ----
// The camera is axis-aligned (no rotation), at cameraWorldPos() looking down -Z with
// vertical FoV m_fov. The molecule rotates under the root node (rootRotation about
// sceneCenter). We replicate that projection here so picking/grab need no Quick3D
// camera/viewport C++ types (which are private in this Qt build).

QVector3D SceneController::modelToWorld(const QVector3D& local) const
{
    return m_sceneCenter + m_rootRotation.rotatedVector(local - m_sceneCenter);
}

namespace {
// Project a world point to viewport pixels for the axis-aligned camera. Returns
// false if behind the camera. halfH = tan(fov/2), halfW = halfH * aspect.
bool projectToScreen(const QVector3D& world, const QVector3D& camPos, float fovDeg,
    float viewW, float viewH, float& outX, float& outY)
{
    const float depth = camPos.z() - world.z(); // +Z in front of a -Z-looking camera
    if (depth <= 1e-4f || viewW <= 0 || viewH <= 0)
        return false;
    const float halfH = std::tan(fovDeg * float(M_PI) / 360.0f);
    const float halfW = halfH * (viewW / viewH);
    const float ndcX = (world.x() - camPos.x()) / (depth * halfW);
    const float ndcY = (world.y() - camPos.y()) / (depth * halfH);
    outX = (ndcX + 1.0f) * 0.5f * viewW;
    outY = (1.0f - ndcY) * 0.5f * viewH;
    return true;
}
}

// excludeIndex: skip one atom (the builder's live-dragged atom would always
// occlude the pick under the cursor). Claude Generated 2026.
int SceneController::pickAtom(float sx, float sy, float viewW, float viewH, int excludeIndex) const
{
    if (m_atoms.isEmpty() || viewW <= 0 || viewH <= 0)
        return -1;
    const QVector3D camPos = cameraWorldPos();
    const float halfH = std::tan(m_fov * float(M_PI) / 360.0f);
    const float halfW = halfH * (viewW / viewH);
    const float ndcX = 2.0f * sx / viewW - 1.0f;
    const float ndcY = 1.0f - 2.0f * sy / viewH;
    // Ray in world space: camera looks down -Z, right=+X, up=+Y.
    const QVector3D rayDir = QVector3D(ndcX * halfW, ndcY * halfH, -1.0f).normalized();

    int best = -1;
    float bestT = 1e20f;
    for (int i = 0; i < m_atoms.size(); ++i) {
        if (i == excludeIndex)
            continue;
        const QVector3D center = modelToWorld(m_atoms[i].position);
        const float radius = elem::vdwRadius(m_atoms[i].element) * 1.5f; // generous hit
        const QVector3D oc = camPos - center;
        const float b = 2.0f * QVector3D::dotProduct(oc, rayDir);
        const float c = QVector3D::dotProduct(oc, oc) - radius * radius;
        const float disc = b * b - 4.0f * c;
        if (disc < 0.0f)
            continue;
        const float sq = std::sqrt(disc);
        float t = (-b - sq) * 0.5f;
        if (t < 0.0f)
            t = (-b + sq) * 0.5f;
        if (t > 0.0f && t < bestT) {
            bestT = t;
            best = i;
        }
    }
    return best;
}

// Claude Generated 2026 - atoms whose projected centres fall inside a pixel rectangle
// (rubber-band/box selection). Atoms behind the camera are skipped.
QVector<int> SceneController::atomsInScreenRect(const QRectF& rectPx, float viewW, float viewH) const
{
    QVector<int> result;
    if (m_atoms.isEmpty() || viewW <= 0 || viewH <= 0 || !rectPx.isValid())
        return result;
    const QVector3D camPos = cameraWorldPos();
    for (int i = 0; i < m_atoms.size(); ++i) {
        float sx = 0, sy = 0;
        if (projectToScreen(modelToWorld(m_atoms[i].position), camPos, m_fov, viewW, viewH, sx, sy)
            && rectPx.contains(sx, sy))
            result.append(i);
    }
    return result;
}

QVector3D SceneController::computeGrabForce(float mx, float my, int atomIndex,
    float viewW, float viewH, double grabStrength) const
{
    if (atomIndex < 0 || atomIndex >= m_atoms.size() || viewW <= 0 || viewH <= 0)
        return QVector3D();
    const QVector3D camPos = cameraWorldPos();
    const QVector3D atomWorld = modelToWorld(m_atoms[atomIndex].position);
    float atomSx = 0, atomSy = 0;
    if (!projectToScreen(atomWorld, camPos, m_fov, viewW, viewH, atomSx, atomSy))
        return QVector3D();

    const float dx = mx - atomSx;
    const float dy = my - atomSy;

    // Pixels -> world distance at the atom's depth, then Angstrom -> Bohr.
    static constexpr float Angstrom2Bohr = 1.8897259886f;
    const float dist = (atomWorld - camPos).length();
    const float worldPerPxAng = dist * std::tan(m_fov * float(M_PI) / 360.0f) / (viewH * 0.5f);
    const float worldPerPxBohr = worldPerPxAng * Angstrom2Bohr;

    // camera basis (world): right=+X, up=+Y; screen Y is down -> negate dy.
    const QVector3D worldForce = (dx * QVector3D(1, 0, 0) - dy * QVector3D(0, 1, 0))
        * float(worldPerPxBohr * grabStrength);
    // Back to model-local (intrinsic) coords the simulation operates in.
    return m_rootRotation.inverted().rotatedVector(worldForce);
}

// Claude Generated 2026 - Structure editing translation. Shares computeGrabForce's
// pixels->world-at-depth scale but returns an Angstrom translation (no Bohr, no
// strength). The X/Y components move in the view plane; dDepthPx moves along the
// camera axis (+Z = toward the camera). The result is rotated into model-local
// (intrinsic) coords so it can be added directly to stored atom positions.
// Claude Generated 2026 - Inverse of pickAtom's ray at a reference depth: cast
// the same ray and intersect it with the camera-facing plane (z = const, the
// camera looks down -Z) through the reference point, then rotate back into
// model-local coordinates (inverse of modelToWorld).
QVector3D SceneController::screenToModelPoint(float sx, float sy,
    const QVector3D& depthRefLocal, float viewW, float viewH) const
{
    if (viewW <= 0 || viewH <= 0)
        return depthRefLocal;
    const QVector3D camPos = cameraWorldPos();
    const float halfH = std::tan(m_fov * float(M_PI) / 360.0f);
    const float halfW = halfH * (viewW / viewH);
    const float ndcX = 2.0f * sx / viewW - 1.0f;
    const float ndcY = 1.0f - 2.0f * sy / viewH;
    const QVector3D rayDir = QVector3D(ndcX * halfW, ndcY * halfH, -1.0f).normalized();
    const QVector3D refWorld = modelToWorld(depthRefLocal);
    const float t = (refWorld.z() - camPos.z()) / rayDir.z();  // rayDir.z() < 0 always
    const QVector3D world = camPos + rayDir * t;
    return m_sceneCenter + m_rootRotation.inverted().rotatedVector(world - m_sceneCenter);
}

QVector3D SceneController::screenDragToModelDelta(float dxPx, float dyPx, float dDepthPx,
    const QVector3D& refLocal, float viewW, float viewH) const
{
    if (viewW <= 0 || viewH <= 0)
        return QVector3D();
    const QVector3D camPos = cameraWorldPos();
    const QVector3D refWorld = modelToWorld(refLocal);
    const float dist = (refWorld - camPos).length();
    const float worldPerPxAng = dist * std::tan(m_fov * float(M_PI) / 360.0f) / (viewH * 0.5f);

    // right=+X, up=+Y; screen Y is down -> negate dy. Depth along +Z (toward camera):
    // dragging up (dDepthPx < 0) pulls the selection toward the viewer.
    const QVector3D worldDelta = (dxPx * QVector3D(1, 0, 0)
                                  - dyPx * QVector3D(0, 1, 0)
                                  - dDepthPx * QVector3D(0, 0, 1))
        * worldPerPxAng;
    return m_rootRotation.inverted().rotatedVector(worldDelta);
}
