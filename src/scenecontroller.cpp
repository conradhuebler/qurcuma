// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// SceneController — Qt Quick 3D view-model. Claude Generated.
#include "scenecontroller.h"

#include "atominstancing.h"
#include "bondinstancing.h"
#include "elementdata.h"

#include <QPair>
#include <QtMath>
#include <limits>

namespace {
constexpr float kSphereBaseRadius = 50.0f; // #Sphere base radius
constexpr float kCylBaseHalfHeight = 50.0f; // #Cylinder half height (100 tall)
constexpr float kCylBaseRadius = 50.0f; // #Cylinder base radius

/// "Shift" a CPK element colour for the RMSD overlay: rotate the hue and add a
/// tint + slight darkening, so the second structure is clearly distinct yet still
/// element-coloured. Works on greys/whites too (they get a base hue + saturation).
QColor shiftOverlayColor(const QColor& base)
{
    int h, s, v, a;
    base.getHsv(&h, &s, &v, &a);
    if (h < 0)
        h = 30;                       // achromatic (C grey, H white) -> warm base hue
    h = (h + 30) % 360;               // rotate hue
    s = qBound(70, s + 60, 255);      // ensure a visible tint, even on near-greys
    v = int(v * 0.88f);               // slightly darker -> reads as the secondary set
    return QColor::fromHsv(h, s, v);
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
    m_overlayAtoms = new AtomInstancing(nullptr);
    m_overlayAtoms->setParent(this);
    m_overlayBonds = new BondInstancing(nullptr);
    m_overlayBonds->setParent(this);
}

QQuick3DInstancing* SceneController::measureLineInstancing() const { return m_measureLines; }
QQuick3DInstancing* SceneController::overlayAtomInstancing() const { return m_overlayAtoms; }
QQuick3DInstancing* SceneController::overlayBondInstancing() const { return m_overlayBonds; }
QQuick3DInstancing* SceneController::wallInstancing() const { return m_wallLines; }

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

void SceneController::setOverlayStructure(const QVector<AtomDatum>& atoms,
    const QVector<BondDatum>& bonds)
{
    // Second structure (RMSD target) under moleculeRoot: opaque, with HSV-shifted
    // element colours so it is clearly the "other" molecule yet element-coded.
    // Slightly smaller spheres so it nests inside the solid reference atoms.
    auto tinted = [&](const QString& element) {
        return shiftOverlayColor(elem::cpkColor(element));
    };

    QVector<AtomInstancing::Item> items;
    items.reserve(atoms.size());
    for (const AtomDatum& a : atoms) {
        AtomInstancing::Item it;
        it.position = a.position;
        it.scale = 0.27f * m_atomScaleFactor * elem::vdwRadius(a.element);
        it.color = tinted(a.element);
        items.append(it);
    }
    m_overlayAtoms->setItems(items);

    QVector<BondInstancing::Segment> segs;
    const float sxz = m_bondRadius / kCylBaseRadius;
    for (const BondDatum& b : bonds) {
        if (b.a < 0 || b.b < 0 || b.a >= atoms.size() || b.b >= atoms.size())
            continue;
        const QVector3D posA = atoms[b.a].position;
        const QVector3D posB = atoms[b.b].position;
        const QVector3D dir = posB - posA;
        const float length = dir.length();
        if (length < 1e-4f)
            continue;
        const QQuaternion rot = bondRotation(dir / length);
        const QVector3D mid = 0.5f * (posA + posB);
        const float halfLength = length * 0.25f;
        const QVector3D scale(sxz, halfLength / kCylBaseHalfHeight, sxz);
        segs.append({ 0.5f * (posA + mid), scale, rot, tinted(atoms[b.a].element) });
        segs.append({ 0.5f * (mid + posB), scale, rot, tinted(atoms[b.b].element) });
    }
    m_overlayBonds->setSegments(segs);

    m_overlayVisible = !atoms.isEmpty();
    emit overlayChanged();
}

void SceneController::clearOverlay()
{
    if (!m_overlayVisible)
        return;
    m_overlayAtoms->setItems({});
    m_overlayBonds->setSegments({});
    m_overlayVisible = false;
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
    const float r = 0.08f;       // slightly thicker than measurement lines
    // Alpha is baked into the per-segment colour (mirrors atom transparency:
    // SceneController bakes m_transparency into the instance colour and the
    // material runs in Blend mode). RGB = m_wallColor (grey/red), alpha =
    // m_wallOpacity. The QML material binds alphaMode to Blend when opacity<1.
    const QColor color(m_wallColor.red(), m_wallColor.green(), m_wallColor.blue(),
                       int(m_wallOpacity * 255));
    if (m_wallGeom == 2) {
        // 12 edges of the cuboid [m_wallMin .. m_wallMax].
        QVector<BondInstancing::Segment> segs;
        segs.reserve(12);
        const QVector3D& mn = m_wallMin;
        const QVector3D& mx = m_wallMax;
        const QVector3D c000(mn.x(), mn.y(), mn.z());
        const QVector3D c100(mx.x(), mn.y(), mn.z());
        const QVector3D c010(mn.x(), mx.y(), mn.z());
        const QVector3D c110(mx.x(), mx.y(), mn.z());
        const QVector3D c001(mn.x(), mn.y(), mx.z());
        const QVector3D c101(mx.x(), mn.y(), mx.z());
        const QVector3D c011(mn.x(), mx.y(), mx.z());
        const QVector3D c111(mx.x(), mx.y(), mx.z());
        appendWallEdge(segs, c000, c100, r, color);
        appendWallEdge(segs, c010, c110, r, color);
        appendWallEdge(segs, c001, c101, r, color);
        appendWallEdge(segs, c011, c111, r, color);
        appendWallEdge(segs, c000, c010, r, color);
        appendWallEdge(segs, c100, c110, r, color);
        appendWallEdge(segs, c001, c011, r, color);
        appendWallEdge(segs, c101, c111, r, color);
        appendWallEdge(segs, c000, c001, r, color);
        appendWallEdge(segs, c100, c101, r, color);
        appendWallEdge(segs, c010, c011, r, color);
        appendWallEdge(segs, c110, c111, r, color);
        m_wallLines->setSegments(segs);
    } else if (m_wallGeom == 1) {
        // Lat/long wireframe, origin-centred, radius m_wallRadius.
        QVector<BondInstancing::Segment> segs;
        const int nLat = 7;   // latitude rings (excluding poles)
        const int nLng = 12;  // longitude half-rings through both poles
        for (int i = 1; i <= nLat; ++i) {
            const float frac = float(i) / float(nLat + 1);
            const float z = m_wallRadius * std::cos(frac * float(M_PI));
            const float ringR = m_wallRadius * std::sin(frac * float(M_PI));
            if (ringR < 1e-3f)
                continue;
            const int seg = 48;
            QVector3D prev(QVector3D(ringR, 0.0f, z));
            for (int k = 1; k <= seg; ++k) {
                const float ang = 2.0f * float(M_PI) * float(k) / float(seg);
                const QVector3D cur(ringR * std::cos(ang), ringR * std::sin(ang), z);
                appendWallEdge(segs, prev, cur, r, color);
                prev = cur;
            }
        }
        for (int m = 0; m < nLng; ++m) {
            const float phi = 2.0f * float(M_PI) * float(m) / float(nLng);
            const int seg = 48;
            QVector3D prev(0.0f, 0.0f, m_wallRadius);
            for (int k = 1; k <= seg; ++k) {
                const float theta = float(M_PI) * float(k) / float(seg);
                const QVector3D cur(m_wallRadius * std::sin(theta) * std::cos(phi),
                                     m_wallRadius * std::sin(theta) * std::sin(phi),
                                     m_wallRadius * std::cos(theta));
                appendWallEdge(segs, prev, cur, r, color);
                prev = cur;
            }
        }
        m_wallLines->setSegments(segs);
    }
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

void SceneController::setStructure(const QVector<AtomDatum>& atoms, const QVector<BondDatum>& bonds)
{
    m_atoms = atoms;
    m_bonds = bonds;
    m_selection.clear();
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
}

void SceneController::clear()
{
    m_atoms.clear();
    m_bonds.clear();
    m_selection.clear();
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

QColor SceneController::atomColor(int index) const
{
    // Selection wins; otherwise scheme-based colour. Alpha from transparency.
    if (m_selection.contains(index)) {
        QColor h(255, 0, 255);
        h.setAlphaF(1.0f);
        return h;
    }
    QColor c;
    const AtomDatum& a = m_atoms[index];
    // Hover feedback: brighten the atom under the cursor (below selection).
    if (index == m_hoverAtom) {
        QColor base = (m_colorScheme == Monochrome) ? m_monochrome : elem::cpkColor(a.element);
        QColor hl = base.lighter(170);
        hl.setAlphaF(m_transparency);
        return hl;
    }
    switch (m_colorScheme) {
    case Monochrome:
        c = m_monochrome;
        break;
    case ByCharge: {
        const float q = qBound(-1.0f, a.charge, 1.0f);
        if (q >= 0)
            c = QColor::fromRgbF(1.0, 1.0 - q, 1.0 - q); // white -> red
        else
            c = QColor::fromRgbF(1.0 + q, 1.0 + q, 1.0); // white -> blue
        break;
    }
    case CPK:
    case Custom:
    default:
        c = elem::cpkColor(a.element);
        break;
    }
    c.setAlphaF(m_transparency);
    return c;
}

// Atoms only — cheap path for selection/hover changes (skips the bonds rebuild).
void SceneController::rebuildAtoms()
{
    QVector<AtomInstancing::Item> items;
    if (m_atomsVisible) {
        // Ball-and-stick shrinks spheres; space-filling uses full vdW radius.
        const float radiusFactor = (m_renderingMode == SpaceFilling) ? 1.0f : 0.30f;
        items.reserve(m_atoms.size());
        for (int i = 0; i < m_atoms.size(); ++i) {
            AtomInstancing::Item it;
            it.position = m_atoms[i].position;
            it.scale = radiusFactor * m_atomScaleFactor * elem::vdwRadius(m_atoms[i].element);
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
    if (m_bondsVisible) {
        const float bondRadius = (m_renderingMode == Wireframe)
            ? qMin(m_bondRadius, 0.06f)
            : m_bondRadius;
        const float sxz = bondRadius / kCylBaseRadius;
        segs.reserve(m_bonds.size() * 2);
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
            const QVector3D scale(sxz, halfLength / kCylBaseHalfHeight, sxz);

            QColor cA = (m_colorScheme == Monochrome) ? m_monochrome : elem::cpkColor(m_atoms[b.a].element);
            QColor cB = (m_colorScheme == Monochrome) ? m_monochrome : elem::cpkColor(m_atoms[b.b].element);
            cA.setAlphaF(m_transparency);
            cB.setAlphaF(m_transparency);

            segs.append({ 0.5f * (posA + mid), scale, rot, cA });
            segs.append({ 0.5f * (mid + posB), scale, rot, cB });
        }
    }
    m_bondInstancing->setSegments(segs);
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
void SceneController::resetView()
{
    m_rootRotation = QQuaternion();
    m_pan = QVector3D();
    m_cameraDistance = m_sceneExtent * 3.0f;
    emit transformChanged();
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

int SceneController::pickAtom(float sx, float sy, float viewW, float viewH) const
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
