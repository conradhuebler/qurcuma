// viewer.h
#ifndef MOLECULEVIEWER_H
#define MOLECULEVIEWER_H

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QSpinBox>
#include <QFrame>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QPhongMaterial>  // Claude Generated - For storing material references
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QObjectPicker>    // Claude Generated - For 3D atom picking
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <QVector3D>
#include "simulationframe.h"  // Claude Generated - Zero-copy simulation payload

class SelectionManager;  // Forward declaration
class MeasurementOverlay;  // Claude Generated - Phase 2B - Forward declaration
class BondEditor;  // Claude Generated - Phase 4B - Forward declaration
class PBRMaterial;  // Claude Generated - Phase 4A - Forward declaration
class PerformanceOptimizer;  // Claude Generated - LOD wire-up
class AtomInstancingSystem;  // Claude Generated - Phase 3.1 - GPU instancing
class BondInstancingSystem;  // Claude Generated - Phase 3.2 - GPU instancing
class ForceOverlay;          // Claude Generated 2026 - Force visualization overlay
namespace Qt3DRender { class QPointLight; }  // Claude Generated - corner lights

class MoleculeViewer : public QWidget
{
    Q_OBJECT

public:
    // Claude Generated - Rendering modes for molecular visualization
    enum class RenderingMode {
        BallAndStick,    // Default: Atoms as spheres, bonds as cylinders
        Wireframe,       // Only bonds (thin cylinders)
        SpaceFilling,    // Van der Waals spheres only
        SticksOnly       // Thin cylinders for bonds, no atom spheres
    };

    // Claude Generated - Phase 4A: Material rendering modes
    enum class MaterialMode {
        Phong,           // Traditional Phong lighting (default)
        PBR              // Physically-Based Rendering (Cook-Torrance)
    };

    // Claude Generated - Color schemes for atom coloring
    enum class ColorScheme {
        CPK,            // Standard CPK colors (element-based)
        Monochrome,     // Single color (uniform gray)
        ByCharge,       // Color by atomic charge (if available)
        Custom          // User-defined colors
    };

    struct Atom {
        QVector3D position;
        QString element;
        float charge = 0.0f;  // Claude Generated - for charge-based coloring
    };

    struct Bond {
        int atom1;
        int atom2;
        int bondOrder;
    };

    explicit MoleculeViewer(QWidget *parent = nullptr);
    ~MoleculeViewer();

    void addMolecule(const QVector<Atom>& atoms, const QVector<Bond>& bonds);
    void addMolecule(const QVector<Atom>& atoms) { addMolecule(atoms, {}); }

    // Claude Generated 2026 - Reset the throttled-emit dirty flag. Must be
    // called by MainWindow whenever a new SimulationWorker is wired up, so the
    // first frame of the *new* run emits moleculeUpdated (and thus re-syncs
    // SimulationControlWidget::m_atoms) instead of being silently dropped
    // because the flag was still set from the previous run.
    void resetSimDirty() { m_moleculeDirty = false; }

    // Claude Generated - Visual settings setters
    void setRenderingMode(RenderingMode mode);
    RenderingMode getRenderingMode() const { return m_renderingMode; }

    // Claude Generated - Phase 4A: Material mode switching
    void setMaterialMode(MaterialMode mode);
    MaterialMode getMaterialMode() const { return m_materialMode; }

    // Claude Generated - Phase 4 Final: Glow intensity for selection highlighting
    void setGlowIntensity(float intensity);
    float getGlowIntensity() const { return m_glowIntensity; }

    void setColorScheme(ColorScheme scheme);
    ColorScheme getColorScheme() const { return m_colorScheme; }

    void setAtomTransparency(float alpha);  // 0.0 (transparent) to 1.0 (opaque)
    float getAtomTransparency() const { return m_atomTransparency; }

    void setAtomShininess(float shininess);
    float getAtomShininess() const { return m_atomShininess; }

    void setAtomScaleFactor(float scale);  // Global atom size multiplier
    float getAtomScaleFactor() const { return m_atomScaleFactor; }

    void setBondThickness(float thickness);
    float getBondThickness() const { return m_bondThickness; }

    // Claude Generated - Fog/Depth effect
    void setFogEnabled(bool enabled);
    bool getFogEnabled() const { return m_fogEnabled; }
    void setFogIntensity(float intensity);
    float getFogIntensity() const { return m_fogIntensity; }

    // Claude Generated - Phase 5A: SSAO post-processing
    void setSSAOEnabled(bool enabled);
    bool getSSAOEnabled() const { return m_ssaoEnabled; }
    void setSSAOIntensity(float intensity);
    float getSSAOIntensity() const { return m_ssaoIntensity; }
    void setSSAORadius(float radius);
    float getSSAORadius() const { return m_ssaoRadius; }
    void setSSAOBias(float bias);
    float getSSAOBias() const { return m_ssaoBias; }

    // Claude Generated - Phase 5B: Bloom and HDR post-processing
    void setBloomEnabled(bool enabled);
    bool getBloomEnabled() const { return m_bloomEnabled; }
    void setBloomThreshold(float threshold);
    float getBloomThreshold() const { return m_bloomThreshold; }
    void setBloomIntensity(float intensity);
    float getBloomIntensity() const { return m_bloomIntensity; }
    void setHDREnabled(bool enabled);
    bool getHDREnabled() const { return m_hdrEnabled; }
    void setExposure(float exposure);
    float getExposure() const { return m_exposure; }

    // Claude Generated 2026 - Rotation mode toggle (Model vs. Camera-Orbit)
    enum class RotationMode {
        Model = 0,        // Rotate molecule, camera stays put (default)
        CameraOrbit = 1   // Rotate camera around molecule center
    };
    void setRotationMode(int mode);
    int getRotationMode() const { return static_cast<int>(m_rotationMode); }

    // Claude Generated 2026 - Configurable GPU-instancing threshold.
    // Below threshold: per-atom entities with pickers. At/above: single instanced draw (picking disabled).
    void setInstancingThreshold(int n);
    int getInstancingThreshold() const { return m_instancingThreshold; }

    // Frame navigation support for trajectories
    void setFrameCount(int frameCount) { m_frameCount = frameCount; }
    int getFrameCount() const { return m_frameCount; }
    void setCurrentFrame(int frameIndex) { m_currentFrame = frameIndex; }
    int getCurrentFrame() const { return m_currentFrame; }
    
    // Trajectory data (XYZ, VTF, etc.) — call with multiple frames
    void setTrajectoryData(const QVector<QVector<Atom>>& atoms, const QVector<QVector<Bond>>& bonds);

public slots:
    void resetView();
    void resetViewToMolecule();  // Reset to molecule center (fallback to default if none loaded)
    void showFrame(int frameIndex);  // Show specific frame
    void nextFrame();               // Show next frame
    void previousFrame();           // Show previous frame

    // Claude Generated - Screenshot/Export functionality
    void saveScreenshot(const QString& filename, int scaleFactor = 1);
    void saveScreenshotDialog();

    // Claude Generated - Trajectory animation
    void startAnimation();
    void stopAnimation();
    void setAnimationFPS(int fps);
    int getAnimationFPS() const { return m_animationFPS; }
    void setAnimationLoop(bool loop) { m_animationLoop = loop; }

    // Claude Generated - Atom selection and measurement
    void clearSelection();
    const QVector<int>& getSelectedAtoms() const { return m_selectedAtoms; }
    void setMeasurementMode(int mode);  // 0=None, 1=Distance, 2=Angle, 3=Dihedral - Claude Generated Phase 2B
    void selectAtom(int index, bool append = false);  // Claude Generated - Phase 2A - Direct selection from 3D picker
    SelectionManager* getSelectionManager() const { return m_selectionManager; }  // Claude Generated - Phase 2A
    MeasurementOverlay* getMeasurementOverlay() const { return m_measurementOverlay; }  // Claude Generated - Phase 2B

    // Claude Generated - Phase 4B: Bond editing
    BondEditor* getBondEditor() const { return m_bondEditor; }
    void setBondEditMode(int mode);  // 0=None, 1=AddBond, 2=DeleteBond, 3=ChangeBondOrder

    // Claude Generated - Phase 2C: Atom data accessors for AtomListPanel
    QVector<QVector3D> getAtomPositions() const;
    QVector<QString> getAtomElements() const;
    QVector<float> getAtomCharges() const;

    /**
     * @brief Get current frame atoms as complete Atom structs (position + element + charge).
     * Claude Generated - Interactive Simulation Integration
     * Used by simulation integration to pass the current molecule to the simulation engine.
     */
    QVector<Atom> getCurrentFrameAtoms() const;

    // Claude Generated 2026 - Companion getter to getCurrentFrameAtoms; the
    // bond graph is normally stable across a simulation run, but the sim
    // dock needs the live copy before starting a new run.
    QVector<Bond> getCurrentFrameBonds() const {
        if (m_currentFrame >= 0 && m_currentFrame < m_trajectoryBonds.size())
            return m_trajectoryBonds[m_currentFrame];
        return {};
    }

    /**
     * @brief Update atom positions for live simulation without scene rebuild or bond detection.
     * Claude Generated - Zero-copy variant: positions come via QSharedPointer<SimulationFrame>.
     * Reuses existing bonds; falls back to addMolecule if atom count changed.
     */
    void updateSimulationFrame(SimulationFramePtr frame);

    /**
     * @brief Enable/disable per-atom and per-bond QObjectPickers in bulk.
     * Claude Generated - Picker CPU-cost reduction: pickers traverse scene for each mouse event.
     * During live simulation picking is rarely used; disabling saves ray-cast cost per frame.
     * Wire to SimulationControlWidget::simulationRunningChanged so pickers auto-toggle.
     */
    void setPickingActive(bool active);

    // Claude Generated - Focus & Zoom commands
    void centerOnAtom(int atomIndex);
    void zoomToSelection(const QVector<int>& atomIndices);
    void fitAllInView();
    void getSelectedBounds(QVector3D& center, float& radius);  // Helper for focus commands

signals:
    void frameChanged(int frameIndex);
    void trajectoryLoaded(int frameCount);
    void selectionChanged(const QVector<int>& selectedAtoms);  // Claude Generated - Phase 2A

    /** Claude Generated 2026 - Phase 6: fired whenever the viewer swaps its
     *  molecule (addMolecule / setTrajectoryData / setVTFTrajectoryData).
     *  Consumers like the simulation dock can re-sync their state. */
    void moleculeUpdated(const QVector<MoleculeViewer::Atom>& atoms,
        const QVector<MoleculeViewer::Bond>& bonds);

    /** Claude Generated 2026 - Interactive Sim Phase 5
     *  Emitted while the user drags a grabbed atom.
     *  @p force is a screen-space→world delta in Eh/Bohr. */
    void atomForceRequested(int atomIndex, QVector3D force, double alpha, int maxShells);
    /** Emitted on mouse release — consumer should clear any pending force. */
    void atomGrabReleased();
    /** Emitted when hovering over grab-capable atoms — status message for UI. */
    void grabStatusChanged(QString message);

public slots:
    /** Enable/disable sim-mode grabbing. In sim mode an atom click+drag
     *  produces atomForceRequested() instead of a selection change. */
    void setSimulationActive(bool on);  // Implemented in .cpp - handles camera controller re-enable
    void setGrabStrength(double s) { m_grabStrength = s; }
    void setGrabAlpha(double a) { m_grabAlpha = a; }
    void setGrabMaxShells(int n) { m_grabMaxShells = n; }

    /** Ensure pickers exist for interactive grab.
     *  Called when simulation starts - creates pickers if they don't exist (e.g., for large molecules).
     *  Claude Generated - Fix for grab feedback on instanced molecules. */
    void ensurePickersForGrab();

    // Claude Generated - User-controlled viewer appearance.
    /** Change 3D view background color. */
    void setBackgroundColor(const QColor& color);
    QColor getBackgroundColor() const { return m_backgroundColor; }
    /** Enable/disable one of the 4 corner lights (index 0..3). */
    void setCornerLightEnabled(int index, bool on);
    bool isCornerLightEnabled(int index) const;

public:
    void clearScenePublic();  // Public wrapper for file loading

private slots:
    void onAtomPicked(Qt3DRender::QPickEvent *pickEvent);  // Claude Generated - Phase 2A - Handle ObjectPicker clicks
    void onAtomPressedForGrab(Qt3DRender::QPickEvent *pickEvent);  // Claude Generated 2026 - Phase 5 - Sim grab init
    void onAtomHoverEntered();  // Claude Generated - Visual feedback for grab (signal has no args)
    void onAtomHoverExited();   // Claude Generated - Clear grab feedback (signal has no args)
    void onBondPicked(Qt3DRender::QPickEvent *pickEvent);  // Claude Generated - Phase 4B - Handle bond picking
    void onAutoSaveTimer();  // Claude Generated - Phase 4B - Auto-save XYZ with debouncing
    void onStructureChanged();  // Claude Generated - Phase 4B - Handle bond editor changes

private:
    Qt3DExtras::Qt3DWindow *m_view;
    QWidget *m_container;
    QWidget *m_controlPanel;  // Claude Generated - Integrated control panel

    // Claude Generated - Frame control widgets (shown/hidden based on frame count)
    QWidget *m_frameControlWidget = nullptr;
    QSlider *m_frameSlider = nullptr;
    QLabel *m_frameLabel = nullptr;
    QSpinBox *m_frameJumpBox = nullptr;

    Qt3DCore::QEntity *m_rootEntity;
    Qt3DCore::QEntity *m_modelEntity = nullptr;  // Model rotation parent
    Qt3DCore::QTransform *m_modelTransform = nullptr;  // Model rotation transform
    QQuaternion m_modelRotation;  // Accumulated model rotation
    Qt3DRender::QCamera *m_camera;

    // Claude Generated - 4 world-fixed corner lights (upper cube corners).
    // Indices: 0=top-front-left, 1=top-front-right, 2=top-back-left, 3=top-back-right.
    // Parented to m_lightRoot (a persistent sub-entity of m_rootEntity) so that
    // clearScene() can skip them — otherwise scene rebuilds would destroy the
    // lights and leave dangling QDirectionalLight* pointers.
    Qt3DCore::QEntity* m_lightRoot = nullptr;
    Qt3DRender::QPointLight* m_cornerLights[4] = {nullptr, nullptr, nullptr, nullptr};
    bool m_cornerLightEnabled[4] = {true, true, false, false};
    QColor m_backgroundColor{32, 36, 44};  // Slightly lifted dark, not pure black

    void setupViewer();
    void setupControlPanel();  // Claude Generated - Setup integrated control panel
    QFrame* createSeparator();  // Claude Generated - Helper to create vertical separator in panel
    void updateFramePositions(int frameIndex);  // Claude Generated - Fast position-only update for animation
    void updateAtomPositionsOnly(int frameIndex);  // Claude Generated - Simulation: atoms only, skip bonds
    void updateBondsHybrid(int frameIndex);  // Claude Generated - Hybrid: cached rotation, update center+length
    void prepareSimulationBonds(); // Claude Generated - Precompute bond rotations once before simulation
    void clearScene();  // Private implementation
    void onAnimationTick();  // Claude Generated - Timer callback for animation
    void refreshVisualization();  // Claude Generated - Refresh without camera reset

    // Claude Generated - Incremental update methods (Fix 2) - more efficient than full refresh
    void updateMaterials();     // Update only atom colors/transparency/shininess
    void updateAtomGeometry();  // Update only atom scales (transform)
    void updateBondGeometry();  // Update only bond thickness (transform scale)

    // Claude Generated 2026 - Phase 1 Fog: push fog params into all active materials
    void propagateFogToMaterials();

    Qt3DCore::QEntity* createMoleculeEntity(const QVector<Atom>& atoms, const QVector<Bond>& bonds);
    QColor getAtomColor(const QString& element, float charge = 0.0f);  // Claude Generated - changed to non-static for ColorScheme support
    float getAtomRadius(const QString& element) const;  // Claude Generated - changed to non-static for scaling support
    float getCovalentRadius(const QString& element);  // Claude Generated - Covalent radii for bond detection

    QVector<Bond> detectBonds(const QVector<Atom>& atoms);
    void setDefaultView();
    // Update camera position uniform in instancing shaders after camera moves.
    void updateInstancingCameraPosition();
    // Transform model-local position to world space (applies model rotation).
    QVector3D modelToWorld(const QVector3D& localPos) const;

    QVector3D m_moleculeCenter;
    float m_moleculeRadius;

    // Frame navigation state
    int m_frameCount = 1;
    int m_currentFrame = 0;
    QVector<QVector<Atom>> m_trajectoryAtoms;  // Store all frames
    QVector<QVector<Bond>> m_trajectoryBonds;  // Store all frames

    // Claude Generated - Visual settings state
    RenderingMode m_renderingMode = RenderingMode::BallAndStick;
    ColorScheme m_colorScheme = ColorScheme::CPK;
    MaterialMode m_materialMode = MaterialMode::Phong;  // Claude Generated - Phase 4A
    float m_glowIntensity = 1.0f;         // Claude Generated - Phase 4 Final: Selection glow (1.0-2.0)
    float m_atomTransparency = 1.0f;      // Fully opaque by default
    float m_atomShininess = 80.0f;        // Default Phong shininess
    float m_atomScaleFactor = 1.0f;       // Default: no scaling
    float m_bondThickness = 0.15f;        // Default bond radius
    bool m_fogEnabled = false;            // Fog effect off by default
    float m_fogIntensity = 0.5f;          // Fog intensity 0-1
    // Claude Generated - Phase 5A: SSAO post-processing
    bool m_ssaoEnabled = true;
    float m_ssaoIntensity = 1.0f;
    float m_ssaoRadius = 0.05f;
    float m_ssaoBias = 0.025f;
    // Claude Generated - Phase 5B: Bloom and HDR post-processing
    bool m_bloomEnabled = true;
    float m_bloomThreshold = 0.8f;
    float m_bloomIntensity = 1.0f;
    bool m_hdrEnabled = true;
    float m_exposure = 1.0f;

    // Claude Generated - Animation state
    QTimer *m_animationTimer = nullptr;
    bool m_isAnimating = false;
    int m_animationFPS = 10;
    bool m_animationLoop = true;

    // Claude Generated - Selection and measurement state
    QVector<int> m_selectedAtoms;

    // Claude Generated - Entity references for incremental updates (Fix 2)
    QVector<Qt3DCore::QEntity*> m_atomEntities;      // References to atom sphere entities
    QVector<Qt3DCore::QEntity*> m_bondEntities;      // References to bond cylinder entities
    // Claude Generated - Cached transforms: avoids componentsOfType<QTransform>() scan per frame
    QVector<Qt3DCore::QTransform*> m_atomTransforms; // Direct pointers; populated in createMoleculeEntity
    QVector<Qt3DCore::QTransform*> m_bondTransforms; // Direct pointers; populated in createMoleculeEntity
    QVector<QQuaternion> m_bondRotations; // Claude Generated - Hybrid update: cached rotations, update center+length only
    QVector<Qt3DRender::QMaterial*> m_atomMaterials;  // Claude Generated - Phase 4 Final: Support both Phong and PBR materials
    int m_measurementMode = 0;  // 0=None, 1=Distance, 2=Angle

    // Claude Generated - Phase 2A: Selection management
    SelectionManager *m_selectionManager = nullptr;
    QMap<Qt3DRender::QObjectPicker*, int> m_atomPickerToIndex;  // Map ObjectPicker to atom index for picking

    // Claude Generated - Phase 2B: Measurement overlay
    MeasurementOverlay *m_measurementOverlay = nullptr;

    // Claude Generated - Phase 4B: Bond editing system
    BondEditor *m_bondEditor = nullptr;
    QMap<Qt3DRender::QObjectPicker*, int> m_bondPickerToIndex;  // Map ObjectPicker to bond index for picking
    int m_bondEditMode = 0;  // 0=None, 1=AddBond (click 2 atoms), 2=DeleteBond, 3=ChangeBondOrder

    // Claude Generated - Phase 4B: Auto-save system
    QString m_currentFilePath;  // Path to current XYZ file
    QTimer *m_autoSaveTimer = nullptr;  // Debouncing timer (500ms)
    bool m_autoSaveEnabled = true;  // Enable/disable auto-save
    bool m_hasUnsavedChanges = false;  // Track unsaved state

    // Claude Generated 2026 - Throttle moleculeUpdated emits from the in-place
    // simulation update path. Updated by MD/Opt runs (set true on first frame),
    // cleared by fresh loads (setTrajectoryData / addMolecule). The downstream
    // SimulationControlWidget cache depends on this signal to stay in lockstep
    // with the viewer's current positions.
    bool m_moleculeDirty = false;

    // Claude Generated - LOD: adaptive sphere/cylinder tessellation per atom count
    PerformanceOptimizer *m_perfOpt = nullptr;

    // Claude Generated - Phase 3.1: GPU instanced atom renderer.
    // Active when atoms.size() >= kAtomInstancingThreshold and atoms are drawn.
    AtomInstancingSystem *m_atomInstancing = nullptr;
    static constexpr int kAtomInstancingThresholdDefault = 500;
    int m_instancingThreshold = kAtomInstancingThresholdDefault;  // Claude Generated 2026 - configurable

    // Claude Generated 2026 - Rotation mode state (Model is default)
    RotationMode m_rotationMode = RotationMode::Model;

    // Claude Generated - Phase 3.2: GPU instanced bond renderer.
    // Activated alongside atom instancing. Per-frame update just uploads centers + lengths.
    BondInstancingSystem *m_bondInstancing = nullptr;

    // Mouse interaction - Claude Generated
    bool m_leftMousePressed = false;

    // Claude Generated 2026 - Phase 5: Interactive-sim grab state
    bool m_simulationActive = false;
    int m_grabbedAtom = -1;
    double m_grabStrength = 0.1;   // Eh/Bohr per screen pixel (Bohr-corrected scale)
    double m_grabAlpha = 0.4;
    int m_grabMaxShells = 3;
    bool m_rightMousePressed = false;
    QPoint m_lastMousePos;

    // Claude Generated 2026 - Force visualization overlay
    ForceOverlay *m_forceOverlay = nullptr;
    QVector<QVector<int>> m_forceAdjacency;

    // Ray-casting picking for atom grab (works for instanced + non-instanced)
    int pickAtomAtScreenPos(const QPoint &screenPos) const;
    QVector3D screenToWorldDirection(const QPoint &screenPos) const;
    // Compute grab force from screen-space mouse-to-atom distance
    QVector3D computeGrabForce(const QPoint &mousePos, int atomIndex) const;
    void buildForceAdjacency();
    void updateForceOverlay();

    void handleMouseRotation(const QPoint& currentPos);
    void handleMousePan(const QPoint& currentPos);
    void handleMouseZoom(int delta);
    void updateModelTransformFromRotation();  // Pivot-rotate model around molecule center

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;  // Claude Generated - Handle viewport resize for frame graph
};

#endif // MOLECULEVIEWER_H