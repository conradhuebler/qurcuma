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

    // Visibility per rendering mode.
    Q_PROPERTY(bool atomsVisible READ atomsVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool bondsVisible READ bondsVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool blendEnabled READ blendEnabled NOTIFY appearanceChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY appearanceChanged)

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

    // RMSD overlay structure (second structure under moleculeRoot). Opaque, but its
    // element colours are HSV-shifted (hue rotate + tint/darken) so it reads as the
    // "other" molecule while staying element-identifiable.
    QQuick3DInstancing* overlayAtomInstancing() const;
    QQuick3DInstancing* overlayBondInstancing() const;
    bool overlayVisible() const { return m_overlayVisible; }
    void setOverlayStructure(const QVector<AtomDatum>& atoms, const QVector<BondDatum>& bonds);
    void clearOverlay();

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

    bool atomsVisible() const { return m_atomsVisible; }
    bool bondsVisible() const { return m_bondsVisible; }
    bool blendEnabled() const { return m_transparency < 0.999f; }
    QColor backgroundColor() const { return m_background; }
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
    void setStructure(const QVector<AtomDatum>& atoms, const QVector<BondDatum>& bonds);
    void updatePositions(const QVector<QVector3D>& positions); // sim fast path (same count)
    void clear();
    int atomCount() const { return m_atoms.size(); }
    const QVector<AtomDatum>& atoms() const { return m_atoms; }

    // --- appearance ---
    void setColorScheme(int scheme);
    void setMonochromeColor(const QColor& c);
    void setRenderingMode(int mode);
    void setAtomScaleFactor(float s);
    void setBondThickness(float r);
    void setAtomTransparency(float a);
    void setBackgroundColor(const QColor& c);
    void setSelection(const QVector<int>& indices);
    void setHoverAtom(int index);  // mouse-over highlight (-1 = none)

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
    void resetView();              // frame the whole molecule
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

signals:
    void appearanceChanged();
    void effectsChanged();
    void transformChanged();
    void structureChanged();
    void forceVectorsChanged();
    void measurementChanged();
    void overlayChanged();
    void wallChanged();

private:
    void rebuildGeometry();        // recompute atom items + bond segments
    void rebuildAtoms();           // recompute only atom items (selection/hover)
    void recomputeBounds();
    QColor atomColor(int index) const;

    AtomInstancing* m_atomInstancing = nullptr;
    BondInstancing* m_bondInstancing = nullptr;
    BondInstancing* m_arrowShaft = nullptr; // force-vector shafts (#Cylinder)
    BondInstancing* m_arrowTip = nullptr;   // force-vector tips (#Cone)
    bool m_forceVectorsVisible = false;
    BondInstancing* m_measureLines = nullptr; // measurement lines (#Cylinder)
    QString m_measurementText;
    BondInstancing* m_wallLines = nullptr; // confinement-wall wireframe (#Cylinder)
    bool m_wallVisible = false;
    qreal m_wallOpacity = 0.6;     // wireframe alpha (0=invisible .. 1=opaque)
    int m_wallGeom = 0;            // 0=none, 1=sphere, 2=rect (rebuild source)
    QVector3D m_wallMin, m_wallMax;
    float m_wallRadius = 0.0f;
    QColor m_wallColor{ 200, 200, 205 };  // grey; recoloured red on violations
    void rebuildWall();            // regenerate segments from m_wallGeom + m_wallColor
    AtomInstancing* m_overlayAtoms = nullptr; // RMSD overlay structure spheres
    BondInstancing* m_overlayBonds = nullptr; // RMSD overlay structure cylinders
    bool m_overlayVisible = false;

    QVector<AtomDatum> m_atoms;
    QVector<BondDatum> m_bonds;
    QVector<int> m_selection;
    int m_hoverAtom = -1;

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
