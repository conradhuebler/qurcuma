// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// SceneController — the Qt Quick 3D "view-model" behind MoleculeViewer. Owns the
// atom/bond instancing objects, computes per-instance geometry (bond orientation
// quaternion mirrors the former Qt3D path), and exposes effect + camera/transform
// state as Q_PROPERTYs that viewer3d.qml binds to. C++ (view.cpp) drives mouse,
// picking and grab; QML stays declarative. Claude Generated.
#pragma once

#include <QColor>
#include <QObject>
#include <QQuaternion>
#include <QRectF>
#include <QVector3D>
#include <QVector>

class AtomInstancing;
class BondInstancing;
class QQuick3DInstancing;

class SceneController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuick3DInstancing* atomInstancing READ atomInstancing CONSTANT)
    Q_PROPERTY(QQuick3DInstancing* bondInstancing READ bondInstancing CONSTANT)
    Q_PROPERTY(QQuick3DInstancing* arrowShaftInstancing READ arrowShaftInstancing CONSTANT)
    Q_PROPERTY(QQuick3DInstancing* arrowTipInstancing READ arrowTipInstancing CONSTANT)
    Q_PROPERTY(bool forceVectorsVisible READ forceVectorsVisible NOTIFY forceVectorsChanged)
    // Measurement overlay (distance/angle/dihedral) + RMSD overlay structure (M2)
    Q_PROPERTY(QQuick3DInstancing* measureLineInstancing READ measureLineInstancing CONSTANT)
    Q_PROPERTY(QString measurementText READ measurementText NOTIFY measurementChanged)
    Q_PROPERTY(bool measurementActive READ measurementActive NOTIFY measurementChanged)
    Q_PROPERTY(QQuick3DInstancing* overlayAtomInstancing READ overlayAtomInstancing CONSTANT)
    Q_PROPERTY(QQuick3DInstancing* overlayBondInstancing READ overlayBondInstancing CONSTANT)
    Q_PROPERTY(bool overlayVisible READ overlayVisible NOTIFY overlayChanged)
    // Confinement-wall wireframe (harmonic walls from the interactive MD config).
    // Lives under moleculeRoot so it rotates with the structure; the wall bounds
    // are in intrinsic atom coordinates (the molecule's own frame).
    Q_PROPERTY(QQuick3DInstancing* wallInstancing READ wallInstancing CONSTANT)
    Q_PROPERTY(bool wallVisible READ wallVisible NOTIFY wallChanged)
    // Wall wireframe transparency (0=invisible .. 1=opaque). The per-segment
    // colour carries the grey/red RGB; the material baseColor alpha binds to
    // this so opacity is adjustable without rebuilding the geometry.
    Q_PROPERTY(qreal wallOpacity READ wallOpacity WRITE setWallOpacity NOTIFY wallChanged)
    // Iso-potential shell overlay: 3 concentric shells showing the force gradient
    // around the confinement wall. Optional, enabled via Display panel.
    Q_PROPERTY(QQuick3DInstancing* wallPotShellsInstancing READ wallPotShellsInstancing CONSTANT)
    Q_PROPERTY(bool wallPotShellsVisible READ wallPotShellsVisible NOTIFY wallChanged)
    // Wall force vector field: arrows sampled on the boundary and outer shells.
    Q_PROPERTY(QQuick3DInstancing* wallForceShaftsInstancing READ wallForceShaftsInstancing CONSTANT)
    Q_PROPERTY(QQuick3DInstancing* wallForceTipsInstancing READ wallForceTipsInstancing CONSTANT)
    Q_PROPERTY(bool wallForceArrowsVisible READ wallForceArrowsVisible NOTIFY wallChanged)
    // Rubber-band (box) selection rectangle in viewport pixels (structure editing).
    Q_PROPERTY(bool rubberBandActive READ rubberBandActive NOTIFY rubberBandChanged)
    Q_PROPERTY(QRectF rubberBandRect READ rubberBandRect NOTIFY rubberBandChanged)
    // Edit-mode key/mouse hint (2D overlay); empty string = hidden.
    Q_PROPERTY(QString editHint READ editHint NOTIFY editHintChanged)

    // Visibility per rendering mode.
    Q_PROPERTY(bool atomsVisible READ atomsVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool bondsVisible READ bondsVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool blendEnabled READ blendEnabled NOTIFY appearanceChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY appearanceChanged)
    // Image export: SSAA VeryHigh (vs interactive MSAA) + transparent clear colour.
    Q_PROPERTY(bool highQualityAA READ highQualityAA NOTIFY appearanceChanged)
    Q_PROPERTY(bool transparentBackground READ transparentBackground NOTIFY appearanceChanged)

    // Post-processing (these finally do something, vs the old Qt3D stubs).
    Q_PROPERTY(bool ssaoEnabled READ ssaoEnabled NOTIFY effectsChanged)
    Q_PROPERTY(float ssaoStrength READ ssaoStrength NOTIFY effectsChanged)
    Q_PROPERTY(bool bloomEnabled READ bloomEnabled NOTIFY effectsChanged)
    Q_PROPERTY(bool hdrEnabled READ hdrEnabled NOTIFY effectsChanged)
    Q_PROPERTY(float exposure READ exposure NOTIFY effectsChanged)
    Q_PROPERTY(bool tonemapEnabled READ tonemapEnabled NOTIFY effectsChanged)
    Q_PROPERTY(bool fogEnabled READ fogEnabled NOTIFY effectsChanged)
    Q_PROPERTY(float fogDensity READ fogDensity NOTIFY effectsChanged)
    Q_PROPERTY(float fogDistance READ fogDistance NOTIFY effectsChanged)
    Q_PROPERTY(bool shadowsEnabled READ shadowsEnabled NOTIFY effectsChanged)
    Q_PROPERTY(bool cornerLight0 READ cornerLight0 NOTIFY effectsChanged)
    Q_PROPERTY(bool cornerLight1 READ cornerLight1 NOTIFY effectsChanged)
    Q_PROPERTY(bool cornerLight2 READ cornerLight2 NOTIFY effectsChanged)
    Q_PROPERTY(bool cornerLight3 READ cornerLight3 NOTIFY effectsChanged)

    // Camera / model transform (driven by the widget's mouse handlers).
    Q_PROPERTY(QVector3D pivotPosition READ pivotPosition NOTIFY transformChanged)
    Q_PROPERTY(QQuaternion rootRotation READ rootRotation NOTIFY transformChanged)
    Q_PROPERTY(float cameraDistance READ cameraDistance NOTIFY transformChanged)
    Q_PROPERTY(QVector3D sceneCenter READ sceneCenter NOTIFY structureChanged)
    Q_PROPERTY(float sceneExtent READ sceneExtent NOTIFY structureChanged)
    Q_PROPERTY(float fieldOfView READ fieldOfView CONSTANT)

public:
    explicit SceneController(QObject* parent = nullptr);

    // Renderer-agnostic atom/bond input (viewer translates MoleculeViewer::Atom).
    struct AtomDatum {
        QVector3D position;
        QString element;
        float charge = 0.0f;
    };
    struct BondDatum {
        int a = 0;
        int b = 0;
        int order = 1;
    };

    // One aligned overlay structure (RMSD target). Holds the geometry plus the
    // per-structure modifiers; the global display styles are applied at rebuild time.
    // Claude Generated 2026.
    struct OverlayStructure {
        QVector<AtomDatum> atoms;
        QVector<BondDatum> bonds;
        QColor tint{ Qt::green };   // per-structure colour modifier (hue/sat shift over the base scheme)
        float sizeScale = 0.8f;     // size relative to the reference's effective scale (1.0 = identical)
        bool visible = true;
    };

    enum ColorScheme { CPK = 0, Monochrome = 1, ByCharge = 2, Custom = 3 };
    enum RenderingMode { BallAndStick = 0, Wireframe = 1, SpaceFilling = 2, SticksOnly = 3 };

    QQuick3DInstancing* atomInstancing() const;
    QQuick3DInstancing* bondInstancing() const;
    QQuick3DInstancing* arrowShaftInstancing() const;
    QQuick3DInstancing* arrowTipInstancing() const;
    bool forceVectorsVisible() const { return m_forceVectorsVisible; }

    // World-space force arrow (origin at atom, pointing along the force direction).
    struct Arrow {
        QVector3D origin;
        QVector3D dir; // unit
        float length;  // scene units (Angstrom)
        QColor color;
    };
    void setForceArrows(const QVector<Arrow>& arrows);
    void setForceVectorsVisible(bool on);

    // Measurement overlay
    QQuick3DInstancing* measureLineInstancing() const;
    QString measurementText() const { return m_measurementText; }
    bool measurementActive() const { return !m_measurementText.isEmpty(); }
    /// World-space measurement lines (consecutive selected atoms) + result label.
    void setMeasurement(const QVector<QPair<QVector3D, QVector3D>>& lines, const QString& text);

    // RMSD overlay structures: a list of aligned targets drawn under moleculeRoot. Each
    // inherits the global display styles (rendering mode, atom scale, bond thickness,
    // transparency, base colour scheme) but carries an individual colour tint (hue/sat
    // shift over the base, element identity preserved) and an individual size scale. All
    // visible overlays are packed into the two combined overlay instancing buffers.
    QQuick3DInstancing* overlayAtomInstancing() const;
    QQuick3DInstancing* overlayBondInstancing() const;
    bool overlayVisible() const { return m_overlayVisible; }
    int overlayCount() const { return m_overlays.size(); }
    /// Append an aligned overlay structure; returns its index. @p bonds may be empty.
    int addOverlayStructure(const QVector<AtomDatum>& atoms, const QVector<BondDatum>& bonds,
        const QColor& tint, float sizeScale = 0.8f);
    void setOverlayTint(int index, const QColor& tint);
    void setOverlaySize(int index, float sizeScale);
    void setOverlayVisible(int index, bool visible);
    void removeOverlayStructure(int index);
    void clearOverlay();   // clears the whole overlay list

    // Confinement-wall wireframe. Geometry is supplied in intrinsic atom
    // coordinates; the Model lives under moleculeRoot so it rotates with the
    // molecule. setWallVisible(false) clears the geometry.
    QQuick3DInstancing* wallInstancing() const;
    bool wallVisible() const { return m_wallVisible; }
    qreal wallOpacity() const { return m_wallOpacity; }
    void setWallOpacity(qreal opacity);
    /// Rectangular wall: 12 edges of the cuboid [min, max].
    void setWallBox(const QVector3D& min, const QVector3D& max);
    /// Spheric wall (origin-centred): lat/long wireframe of the given radius.
    void setWallSphere(float radius);
    /// Recolour the current wall wireframe (e.g. red on boundary violations).
    void setWallColor(const QColor& color);
    void setWallVisible(bool on);
    // Iso-potential shell overlay (optional visual gradient around the wall).
    // 3 inside shells (blue→teal) + 3 outside shells (yellow→red).
    QQuick3DInstancing* wallPotShellsInstancing() const;
    bool wallPotShellsVisible() const { return m_wallVisible && m_potVizEnabled && m_wallGeom != 0; }
    /// Enable/disable the shells and update the potential parameters.
    void setWallPotentialViz(bool enabled, bool harmonic, double wallTemp, float wallBeta);
    // Wall force vector field: arrows at grid-sampled points showing force direction
    // and magnitude. Resolution = number of points per axis per face (box) or per ring (sphere).
    QQuick3DInstancing* wallForceShaftsInstancing() const;
    QQuick3DInstancing* wallForceTipsInstancing() const;
    bool wallForceArrowsVisible() const { return m_wallVisible && m_potArrowsEnabled && m_wallGeom != 0; }
    void setWallVectorField(bool enabled, int resolution);

    bool atomsVisible() const { return m_atomsVisible; }
    bool bondsVisible() const { return m_bondsVisible; }
    bool blendEnabled() const { return m_transparency < 0.999f; }
    QColor backgroundColor() const { return m_background; }
    bool highQualityAA() const { return m_highQualityAA; }
    bool transparentBackground() const { return m_transparentBackground; }
    bool ssaoEnabled() const { return m_ssao; }
    float ssaoStrength() const { return m_ssaoStrength; }
    bool bloomEnabled() const { return m_bloom; }
    bool hdrEnabled() const { return m_hdr; }
    float exposure() const { return m_exposure; }
    bool tonemapEnabled() const { return m_tonemap; }
    bool fogEnabled() const { return m_fog; }
    float fogDensity() const { return m_fogDensity; }
    float fogDistance() const { return m_fogDistance; }
    bool shadowsEnabled() const { return m_shadows; }
    bool cornerLight0() const { return m_corner[0]; }
    bool cornerLight1() const { return m_corner[1]; }
    bool cornerLight2() const { return m_corner[2]; }
    bool cornerLight3() const { return m_corner[3]; }

    QVector3D pivotPosition() const { return m_sceneCenter + m_pan; }
    QQuaternion rootRotation() const { return m_rootRotation; }
    float cameraDistance() const { return m_cameraDistance; }
    QVector3D sceneCenter() const { return m_sceneCenter; }
    float sceneExtent() const { return m_sceneExtent; }
    float fieldOfView() const { return m_fov; }

    // --- structure / animation (called by the viewer) ---
    // keepView=true (structure editing: append/delete atoms) skips bounds recompute +
    // camera reset + selection clear, so the molecule stays put under the current view.
    void setStructure(const QVector<AtomDatum>& atoms, const QVector<BondDatum>& bonds,
        bool keepView = false);
    void updatePositions(const QVector<QVector3D>& positions); // sim fast path (same count)
    void updateBonds(const QVector<BondDatum>& bonds);         // Claude Generated 2026 - swap bonds only (no bounds/camera change)
    void clear();
    int atomCount() const { return m_atoms.size(); }
    const QVector<AtomDatum>& atoms() const { return m_atoms; }

    // --- appearance ---
    void setColorScheme(int scheme);
    void setMonochromeColor(const QColor& c);
    void setPrimaryVisible(bool on);   // hide/show the primary (reference) structure
    void setHighQualityAA(bool on);          // SSAA VeryHigh (image export)
    void setTransparentBackground(bool on);  // transparent clear (image export)
    /// Deep-copy the render state from @p src (geometry, overlays, appearance, effects,
    /// camera, walls) into a fresh controller — used to render an offscreen export view
    /// without sharing scene-graph nodes with the live viewer. Claude Generated 2026.
    void cloneStateFrom(const SceneController* src);
    void setRenderingMode(int mode);
    void setAtomScaleFactor(float s);
    void setBondThickness(float r);
    void setAtomTransparency(float a);
    void setBackgroundColor(const QColor& c);
    void setSelection(const QVector<int>& indices);
    void setHoverAtom(int index);  // mouse-over highlight (-1 = none)
    // Claude Generated 2026 - Structure editing: atoms that currently clash with a
    // moved/placed selection. Drawn RED (priority above the magenta selection) so the
    // user sees what to push apart. Empty = no clashes.
    void setCollisionAtoms(const QVector<int>& indices);
    const QVector<int>& collisionAtoms() const { return m_collisionAtoms; }
    // Claude Generated 2026 - Rubber-band (box) selection. The rect is in viewport
    // pixels; QML draws it as a 2D overlay. atomsInScreenRect() returns the atoms whose
    // projected centres fall inside a pixel rectangle (for selection on release).
    bool rubberBandActive() const { return m_rubberBandActive; }
    QRectF rubberBandRect() const { return m_rubberBandRect; }
    void setRubberBand(const QRectF& rectPx, bool active);
    QVector<int> atomsInScreenRect(const QRectF& rectPx, float viewW, float viewH) const;
    // Edit-mode hint overlay (empty = hidden).
    QString editHint() const { return m_editHint; }
    void setEditHint(const QString& text);

    // --- effects ---
    void setSsao(bool on, float strength);
    void setBloom(bool on);
    void setHdr(bool on);
    void setExposure(float e);
    void setTonemap(bool on);
    void setFog(bool on, float density);
    void setFogDistance(float d); // 0 = fog starts close, 1 = fog starts far
    void setShadows(bool on);
    void setCornerLight(int index, bool on);

    // --- camera transform (from mouse handlers) ---
    void setRootRotation(const QQuaternion& q);
    void setCameraDistance(float d);
    void setPan(const QVector3D& pan);
    QVector3D pan() const { return m_pan; }
    /// Set all camera transform parameters atomically (single transformChanged).
    void setCameraTransform(const QQuaternion& rotation, const QVector3D& pan, float distance);
    /// Set only the rotation (Quick orientation buttons); pan and distance stay.
    void setRootRotationOnly(const QQuaternion& rotation);
    void resetView();              // frame the whole molecule
    void centerAtOrigin();         // shift atoms to COM=origin, reset view
    void fitToBounds(const QVector3D& center, float radius);

    // --- picking / grab math (C++ replicates the axis-aligned camera projection,
    //     so no private QQuick3DViewport/QQuick3DCamera headers are needed) ---
    /// Atom local (intrinsic) position -> world (applies the molecule-root rotation).
    QVector3D modelToWorld(const QVector3D& local) const;
    QVector3D cameraWorldPos() const { return m_sceneCenter + m_pan + QVector3D(0, 0, m_cameraDistance); }
    /// Pick nearest atom under viewport pixel (sx,sy); returns index or -1.
    int pickAtom(float sx, float sy, float viewW, float viewH) const;
    /// Grab force in Eh/Bohr (model-local) from mouse vs atom's projected position.
    QVector3D computeGrabForce(float mx, float my, int atomIndex,
        float viewW, float viewH, double grabStrength) const;
    /// Claude Generated 2026 - Structure editing: convert a screen-pixel drag into a
    /// model-local translation (in Angstrom). @p dxPx/@p dyPx move in the view plane,
    /// @p dDepthPx moves along the camera axis (depth). @p refLocal is the selection
    /// centroid (intrinsic coords) used to set the pixel->world scale at its depth.
    QVector3D screenDragToModelDelta(float dxPx, float dyPx, float dDepthPx,
        const QVector3D& refLocal, float viewW, float viewH) const;

signals:
    void appearanceChanged();
    void effectsChanged();
    void transformChanged();
    void structureChanged();
    void forceVectorsChanged();
    void measurementChanged();
    void overlayChanged();
    void wallChanged();
    void rubberBandChanged();
    void editHintChanged();

private:
    void rebuildGeometry();        // recompute atom items + bond segments
    void rebuildAtoms();           // recompute only atom items (selection/hover)
    void rebuildOverlays();        // repack the overlay list into the overlay buffers
    void recomputeBounds();
    QColor atomColor(int index) const;
    // Base scheme colour for an element/charge (CPK/Monochrome/ByCharge), ignoring the
    // transient selection/hover/collision state — used as the tint base for overlays.
    QColor schemeColor(const QString& element, float charge) const;

    AtomInstancing* m_atomInstancing = nullptr;
    BondInstancing* m_bondInstancing = nullptr;
    BondInstancing* m_arrowShaft = nullptr; // force-vector shafts (#Cylinder)
    BondInstancing* m_arrowTip = nullptr;   // force-vector tips (#Cone)
    bool m_forceVectorsVisible = false;
    BondInstancing* m_measureLines = nullptr; // measurement lines (#Cylinder)
    QString m_measurementText;
    BondInstancing* m_wallLines = nullptr; // confinement-wall wireframe (#Cylinder)
    BondInstancing* m_potShells = nullptr; // iso-potential shell overlay (#Cylinder)
    bool m_wallVisible = false;
    qreal m_wallOpacity = 0.6;     // wireframe alpha (0=invisible .. 1=opaque)
    int m_wallGeom = 0;            // 0=none, 1=sphere, 2=rect (rebuild source)
    QVector3D m_wallMin, m_wallMax;
    float m_wallRadius = 0.0f;
    QColor m_wallColor{ 200, 200, 205 };  // grey; recoloured red on violations
    bool m_potVizEnabled = false;   // show iso-potential shells
    bool m_wallHarmonic = true;     // harmonic (true) or LogFermi (false)
    double m_wallTemp = 298.15;     // energy/force scale (wall_temp, K)
    float m_wallBeta = 6.0f;        // LogFermi steepness β (Å⁻¹)
    BondInstancing* m_wallForceShafts = nullptr; // wall force vector field shafts
    BondInstancing* m_wallForceTips   = nullptr; // wall force vector field tips (cone)
    bool m_potArrowsEnabled = false;
    int  m_potArrowResolution = 4;
    void rebuildWall();            // regenerate segments from m_wallGeom + m_wallColor
    void rebuildWallVectorField(); // regenerate force arrows
    AtomInstancing* m_overlayAtoms = nullptr; // combined overlay spheres (all structures)
    BondInstancing* m_overlayBonds = nullptr; // combined overlay cylinders (all structures)
    bool m_overlayVisible = false;            // true if any overlay structure is visible
    QVector<OverlayStructure> m_overlays;     // aligned RMSD targets (per-structure tint/size)

    QVector<AtomDatum> m_atoms;
    QVector<BondDatum> m_bonds;
    QVector<int> m_selection;
    QVector<int> m_collisionAtoms;  // Claude Generated 2026 - clashing atoms (drawn red)
    int m_hoverAtom = -1;
    bool m_rubberBandActive = false;        // Claude Generated 2026 - box-select overlay
    QRectF m_rubberBandRect;                // viewport pixels
    QString m_editHint;                     // Claude Generated 2026 - edit-mode hint HUD

    // appearance state
    int m_colorScheme = CPK;
    int m_renderingMode = BallAndStick;
    QColor m_monochrome{ 200, 200, 200 };
    QColor m_background{ 28, 34, 48 };
    float m_atomScaleFactor = 1.0f;
    float m_bondRadius = 0.15f;
    float m_transparency = 1.0f;
    bool m_atomsVisible = true;
    bool m_bondsVisible = true;
    bool m_primaryVisible = true;   // primary (reference) structure shown? (RMSD workspace)
    bool m_highQualityAA = false;        // SSAA VeryHigh (export) vs MSAA High (interactive)
    bool m_transparentBackground = false; // transparent clear colour (export/compositing)

    // effects state
    bool m_ssao = true;
    float m_ssaoStrength = 1.0f;
    bool m_bloom = true;
    bool m_hdr = true;
    float m_exposure = 1.0f;
    bool m_tonemap = true;
    bool m_fog = false;
    float m_fogDensity = 0.7f;
    float m_fogDistance = 0.2f; // 0..1: where the depth fog starts across the molecule
    bool m_shadows = false;
    bool m_corner[4] = { true, true, false, false };

    // transform state
    QVector3D m_sceneCenter;
    float m_sceneExtent = 10.0f;
    QQuaternion m_rootRotation;
    float m_cameraDistance = 30.0f;
    QVector3D m_pan;
    float m_fov = 45.0f; // vertical FoV (degrees); matches the former Qt3D camera + viewer3d.qml
};
