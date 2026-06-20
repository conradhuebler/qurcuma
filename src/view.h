// viewer.h
#ifndef MOLECULEVIEWER_H
#define MOLECULEVIEWER_H

// Claude Generated 2026 - Renderer migration: MoleculeViewer is now backed by Qt
// Quick 3D (Vulkan RHI) via an embedded QQuickView + SceneController, replacing the
// former Qt3D implementation. The PUBLIC API (Atom/Bond, setters/getters, signals,
// slots) is preserved verbatim so MainWindow and all consumers are unaffected.
#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QSpinBox>
#include <QFrame>
#include <QColor>
#include <QQuaternion>
#include <QVector3D>
#include <QVector>
#include "simulationframe.h"  // Claude Generated - Zero-copy simulation payload

class SelectionManager;  // Forward declaration
class MeasurementOverlay;  // Claude Generated - Phase 2B (Quick3D port pending, M2)
class BondEditor;  // Claude Generated - Phase 4B - Forward declaration
class PerformanceOptimizer;  // Claude Generated - LOD wire-up
class SceneController;  // Claude Generated 2026 - Qt Quick 3D scene view-model
class QQuickView;

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

    /**
     * @brief Overlay two structures simultaneously (RMSD/align comparison view).
     */
    void showOverlay(const QVector<Atom>& refAtoms, const QVector<Bond>& refBonds,
        const QVector<Atom>& targetAtoms, const QVector<Bond>& targetBonds = {});

    void resetSimDirty() { m_moleculeDirty = false; }

    // Claude Generated - Visual settings setters
    void setRenderingMode(RenderingMode mode);
    RenderingMode getRenderingMode() const { return m_renderingMode; }

    void setMaterialMode(MaterialMode mode);
    MaterialMode getMaterialMode() const { return m_materialMode; }

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
    void setFogIntensity(float intensity);   // fog strength (density)
    float getFogIntensity() const { return m_fogIntensity; }
    void setFogDistance(float distance);     // where the fog starts (0=near .. 1=far)
    float getFogDistance() const { return m_fogDistance; }

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

    // Claude Generated 2026 - Configurable GPU-instancing threshold (kept for API
    // compatibility; Quick3D always instances, so this is informational only).
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
    void setMeasurementMode(int mode);  // 0=None, 1=Distance, 2=Angle, 3=Dihedral
    void selectAtom(int index, bool append = false);
    SelectionManager* getSelectionManager() const { return m_selectionManager; }
    MeasurementOverlay* getMeasurementOverlay() const { return m_measurementOverlay; }

    // Claude Generated - Phase 4B: Bond editing
    BondEditor* getBondEditor() const { return m_bondEditor; }
    void setBondEditMode(int mode);  // 0=None, 1=AddBond, 2=DeleteBond, 3=ChangeBondOrder

    // Claude Generated - Phase 2C: Atom data accessors for AtomListPanel
    QVector<QVector3D> getAtomPositions() const;
    QVector<QString> getAtomElements() const;
    QVector<float> getAtomCharges() const;

    QVector<Atom> getCurrentFrameAtoms() const;

    QVector<Bond> getCurrentFrameBonds() const {
        if (m_currentFrame >= 0 && m_currentFrame < m_trajectoryBonds.size())
            return m_trajectoryBonds[m_currentFrame];
        return {};
    }

    /**
     * @brief Update atom positions for live simulation without scene rebuild or bond detection.
     */
    void updateSimulationFrame(SimulationFramePtr frame);

    /**
     * @brief Enable/disable bulk picking. Quick3D picking is ray-based (always available),
     * so this is a no-op kept for API compatibility.
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
    void selectionChanged(const QVector<int>& selectedAtoms);

    void moleculeUpdated(const QVector<MoleculeViewer::Atom>& atoms,
        const QVector<MoleculeViewer::Bond>& bonds);

    void atomForceRequested(int atomIndex, QVector3D force, double alpha, int maxShells);
    void atomGrabReleased();
    void grabStatusChanged(QString message);

public slots:
    void setSimulationActive(bool on);
    void setGrabStrength(double s) { m_grabStrength = s; }
    void setGrabAlpha(double a) { m_grabAlpha = a; }
    void setGrabMaxShells(int n) { m_grabMaxShells = n; }

    /** Kept for API compatibility (Quick3D ray picking always works). */
    void ensurePickersForGrab();

    // Claude Generated - User-controlled viewer appearance.
    void setBackgroundColor(const QColor& color);
    QColor getBackgroundColor() const { return m_backgroundColor; }
    void setCornerLightEnabled(int index, bool on);
    bool isCornerLightEnabled(int index) const;

    /** Opt-in: visualize the injected grab force as arrows (grabbed atom + the
     *  shell-distributed neighbour forces the integrator actually applies). */
    void setForceVectorsVisible(bool on);
    bool getForceVectorsVisible() const { return m_forceVectorsVisible; }

public:
    void clearScenePublic();  // Public wrapper for file loading

private slots:
    void onAutoSaveTimer();  // Claude Generated - Phase 4B - Auto-save XYZ with debouncing
    void onStructureChanged();  // Claude Generated - Phase 4B - Handle bond editor changes
    void onAnimationTick();  // Claude Generated - Timer callback for animation

private:
    void setupViewer();         // Build the QQuickView + SceneController + container
    void setupControlPanel();   // Claude Generated - Integrated control panel (top bar)
    QFrame* createSeparator();  // Helper to create vertical separator in panel

    // Push the current frame's atoms/bonds into the SceneController.
    // resetCamera=false keeps the camera for in-place rebuilds (refresh).
    void syncSceneToController(int frameIndex, bool resetCamera, bool fullRebuild);
    void applyAppearanceToController();  // mirror appearance/effect state into the scene
    void updateFramePositions(int frameIndex);  // fast position-only update (animation)

    void clearScene();          // Private implementation
    void refreshVisualization();// Refresh without camera reset

    // Element data helpers (kept for getCurrentFrame* and bond detection).
    QColor getAtomColor(const QString& element, float charge = 0.0f);
    float getAtomRadius(const QString& element) const;
    float getCovalentRadius(const QString& element);
    QVector<Bond> detectBonds(const QVector<Atom>& atoms);

    void setDefaultView();
    QVector3D modelToWorld(const QVector3D& localPos) const;

    // Force-vector overlay (opt-in)
    void buildForceAdjacency();
    void updateForceVectors();  // recompute arrows from the current grab force

    // Mouse interaction (drives the SceneController transform).
    int pickAtomAtScreenPos(const QPoint &screenPos) const;
    QVector3D computeGrabForce(const QPoint &mousePos, int atomIndex) const;
    void handleMouseRotation(const QPoint& currentPos);
    void handleMousePan(const QPoint& currentPos);
    void handleMouseZoom(int delta);

    // --- Qt Quick 3D backing ---
    QQuickView* m_quickView = nullptr;
    QWidget* m_container = nullptr;
    SceneController* m_scene = nullptr;
    QWidget* m_controlPanel = nullptr;

    // Claude Generated - Frame control widgets (shown/hidden based on frame count)
    QWidget *m_frameControlWidget = nullptr;
    QSlider *m_frameSlider = nullptr;
    QLabel *m_frameLabel = nullptr;
    QSpinBox *m_frameJumpBox = nullptr;

    // 4 screen-fixed corner lights (state mirrored into the scene controller).
    bool m_cornerLightEnabled[4] = {true, true, false, false};
    QColor m_backgroundColor{32, 36, 44};

    QVector3D m_moleculeCenter;
    float m_moleculeRadius = 10.0f;

    // Frame navigation state
    int m_frameCount = 1;
    int m_currentFrame = 0;
    QVector<QVector<Atom>> m_trajectoryAtoms;
    QVector<QVector<Bond>> m_trajectoryBonds;

    // Claude Generated - Visual settings state
    RenderingMode m_renderingMode = RenderingMode::BallAndStick;
    ColorScheme m_colorScheme = ColorScheme::CPK;
    MaterialMode m_materialMode = MaterialMode::Phong;
    float m_glowIntensity = 1.0f;
    float m_atomTransparency = 1.0f;
    float m_atomShininess = 80.0f;
    float m_atomScaleFactor = 1.0f;
    float m_bondThickness = 0.15f;
    bool m_fogEnabled = false;
    float m_fogIntensity = 0.5f;
    float m_fogDistance = 0.4f;
    bool m_ssaoEnabled = true;
    float m_ssaoIntensity = 1.0f;
    float m_ssaoRadius = 0.05f;
    float m_ssaoBias = 0.025f;
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
    int m_measurementMode = 0;

    SelectionManager *m_selectionManager = nullptr;
    MeasurementOverlay *m_measurementOverlay = nullptr;  // nullptr until Quick3D port (M2)
    BondEditor *m_bondEditor = nullptr;
    int m_bondEditMode = 0;
    PerformanceOptimizer *m_perfOpt = nullptr;

    // Claude Generated - Phase 4B: Auto-save system
    QString m_currentFilePath;
    QTimer *m_autoSaveTimer = nullptr;
    bool m_autoSaveEnabled = true;
    bool m_hasUnsavedChanges = false;

    bool m_moleculeDirty = false;

    // Instancing threshold kept for API compatibility (informational).
    static constexpr int kAtomInstancingThresholdDefault = 500;
    int m_instancingThreshold = kAtomInstancingThresholdDefault;

    // Rotation / camera state
    RotationMode m_rotationMode = RotationMode::Model;
    QQuaternion m_modelRotation;

    // Mouse interaction
    bool m_leftMousePressed = false;
    bool m_rightMousePressed = false;
    QPoint m_lastMousePos;
    QPoint m_leftPressPos;       // where the left button went down (click vs drag)
    bool m_leftDragged = false;  // set once the cursor moves past the click threshold

    // Claude Generated 2026 - Interactive-sim grab state
    bool m_simulationActive = false;
    int m_grabbedAtom = -1;
    double m_grabStrength = 0.1;
    double m_grabAlpha = 0.4;
    int m_grabMaxShells = 3;

    // Force-vector overlay (opt-in)
    bool m_forceVectorsVisible = false;
    QVector<QVector<int>> m_forceAdjacency;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
};

#endif // MOLECULEVIEWER_H
