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
#include <Qt3DExtras/QOrbitCameraController>
#include "customframegraph.h"  // Claude Generated - Phase 5A

class SelectionManager;  // Forward declaration
class MeasurementOverlay;  // Claude Generated - Phase 2B - Forward declaration
class BondEditor;  // Claude Generated - Phase 4B - Forward declaration
class PBRMaterial;  // Claude Generated - Phase 4A - Forward declaration

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

    // Frame navigation support for trajectories
    void setFrameCount(int frameCount) { m_frameCount = frameCount; }
    int getFrameCount() const { return m_frameCount; }
    void setCurrentFrame(int frameIndex) { m_currentFrame = frameIndex; }
    int getCurrentFrame() const { return m_currentFrame; }
    
    // XYZ trajectory support
    void setTrajectoryData(const QVector<QVector<Atom>>& atoms, const QVector<QVector<Bond>>& bonds);
    void showTrajectoryFrame(int frameIndex);
    
    // VTF trajectory support (existing)
    void setVTFTrajectoryData(const QVector<QVector<Atom>>& atoms, const QVector<QVector<Bond>>& bonds);

public slots:
    void resetCamera();
    void resetView();
    void resetViewToMolecule();  // Reset auf Molekülzentrum
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

    // Claude Generated - Focus & Zoom commands
    void centerOnAtom(int atomIndex);
    void zoomToSelection(const QVector<int>& atomIndices);
    void fitAllInView();
    void getSelectedBounds(QVector3D& center, float& radius);  // Helper for focus commands

signals:
    void frameChanged(int frameIndex);
    void trajectoryLoaded(int frameCount);
    void selectionChanged(const QVector<int>& selectedAtoms);  // Claude Generated - Phase 2A

public:
    void clearScenePublic();  // Public wrapper for file loading

private slots:
    void onAtomPicked(Qt3DRender::QPickEvent *pickEvent);  // Claude Generated - Phase 2A - Handle ObjectPicker clicks
    void onBondPicked(Qt3DRender::QPickEvent *pickEvent);  // Claude Generated - Phase 4B - Handle bond picking
    void onAutoSaveTimer();  // Claude Generated - Phase 4B - Auto-save XYZ with debouncing
    void onStructureChanged();  // Claude Generated - Phase 4B - Handle bond editor changes

private:
    Qt3DExtras::Qt3DWindow *m_view;
    QWidget *m_container;
    QWidget *m_controlPanel;  // Claude Generated - Integrated control panel
    CustomFrameGraph *m_frameGraph = nullptr;  // Claude Generated - Phase 5A: Multi-pass rendering

    // Claude Generated - Frame control widgets (shown/hidden based on frame count)
    QWidget *m_frameControlWidget = nullptr;
    QSlider *m_frameSlider = nullptr;
    QLabel *m_frameLabel = nullptr;
    QSpinBox *m_frameJumpBox = nullptr;

    Qt3DCore::QEntity *m_rootEntity;
    Qt3DRender::QCamera *m_camera;
    Qt3DExtras::QOrbitCameraController *m_cameraController;

    void setupViewer();
    void setupControlPanel();  // Claude Generated - Setup integrated control panel
    QFrame* createSeparator();  // Claude Generated - Helper to create vertical separator in panel
    void updateFramePositions(int frameIndex);  // Claude Generated - Fast position-only update for animation
    void clearScene();  // Private implementation
    void onAnimationTick();  // Claude Generated - Timer callback for animation
    void updateMeasurementDisplay();  // Claude Generated - Update distance/angle display
    void refreshVisualization();  // Claude Generated - Refresh without camera reset

    // Claude Generated - Incremental update methods (Fix 2) - more efficient than full refresh
    void updateMaterials();     // Update only atom colors/transparency/shininess
    void updateAtomGeometry();  // Update only atom scales (transform)
    void updateBondGeometry();  // Update only bond thickness (transform scale)

    Qt3DCore::QEntity* createMoleculeEntity(const QVector<Atom>& atoms, const QVector<Bond>& bonds);
    QColor getAtomColor(const QString& element, float charge = 0.0f);  // Claude Generated - changed to non-static for ColorScheme support
    float getAtomRadius(const QString& element);  // Claude Generated - changed to non-static for scaling support
    float getCovalentRadius(const QString& element);  // Claude Generated - Covalent radii for bond detection

    static const float DEFAULT_BOND_DISTANCE; // Maximaler Abstand für automatische Bindungserkennung
    QVector<Bond> detectBonds(const QVector<Atom>& atoms);
    void setDefaultView();
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
    int m_firstSelectedAtomForBond = -1;  // Track first atom when adding bond

    // Claude Generated - Phase 4B: Auto-save system
    QString m_currentFilePath;  // Path to current XYZ file
    QTimer *m_autoSaveTimer = nullptr;  // Debouncing timer (500ms)
    bool m_autoSaveEnabled = true;  // Enable/disable auto-save
    bool m_hasUnsavedChanges = false;  // Track unsaved state

    // Mouse interaction - Claude Generated
    bool m_leftMousePressed = false;
    bool m_rightMousePressed = false;
    QPoint m_lastMousePos;

    void handleMouseRotation(const QPoint& currentPos);
    void handleMousePan(const QPoint& currentPos);
    void handleMouseZoom(int delta);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;  // Claude Generated - Handle viewport resize for frame graph
};

#endif // MOLECULEVIEWER_H