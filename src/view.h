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
#include <QHash>
#include <QPair>
#include <QVector>
#include "simulationframe.h"  // Claude Generated - Zero-copy simulation payload
#include "viewpreset.h"  // Claude Generated 2026 - reproducible camera/display presets
#include "imagemetadata.h"  // Claude Generated 2026 - export image provenance
#include "ncitypes.h"  // Claude Generated 2026 - non-covalent interaction results

class SelectionManager;  // Forward declaration
class MeasurementOverlay;  // Claude Generated - Phase 2B (Quick3D port pending, M2)
class BondEditor;  // Claude Generated - Phase 4B - Forward declaration
class PerformanceOptimizer;  // Claude Generated - LOD wire-up
class SceneController;  // Claude Generated 2026 - Qt Quick 3D scene view-model
class Settings;  // Claude Generated 2026 - operator metadata + view presets for export
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
        Custom,         // User-defined colors
        ByType          // Color by bead/residue type label (coarse-grained VTF)
        // NOTE: append new schemes at the end — the numeric value is persisted
        // in VisualizationSettings::colorScheme.
    };

    // Claude Generated 2026 - Per-atom text labels drawn as a 2D overlay.
    enum class AtomLabel {
        None,           // no labels
        Element,        // element symbol (bead type when there is no element)
        Type,           // bead/residue type label (coarse-grained VTF)
        Index           // 0-based atom index
    };

    struct Atom {
        QVector3D position;
        QString element;
        float charge = 0.0f;  // Claude Generated - for charge-based coloring
        // Claude Generated 2026 - coarse-grained (VTF bead) support:
        float radius = 0.0f;  // per-atom draw radius; 0 = fall back to element vdW
        QString type;         // bead/residue type label; drives "By Type" colouring
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

    /// Claude Generated 2026 - Merge a molecule into the current scene (single frame
    /// only): append its atoms/bonds, select them, and start placement (no camera jump).
    void appendMolecule(const QVector<Atom>& atoms, const QVector<Bond>& bonds);

    /// Per-structure overlay descriptor for setOverlayWorkspace() (RMSD workspace). Claude Generated 2026.
    struct OverlaySpec {
        QVector<Atom> atoms;
        QColor tint;
        float sizeScale = 0.8f;
        bool visible = true;
    };

    /**
     * @brief Replace the whole RMSD overlay set in one call (workspace rebuild).
     *
     * The reference structure is drawn as the primary molecule; every entry in
     * @p overlays is an aligned structure that inherits the global display styles plus
     * its own colour tint / size / visibility. @p refVisible hides/shows the primary.
     * @p resetView true => the reference changed: the primary is reset to @p refAtoms
     * (camera reframes). false => only the overlay set changed: the primary is left
     * untouched (no camera jump). Empty @p refAtoms with resetView=false just clears the
     * overlays (keeps the current primary).
     */
    void setOverlayWorkspace(const QVector<Atom>& refAtoms, const QVector<Bond>& refBonds,
        bool refVisible, const QVector<OverlaySpec>& overlays, bool resetView);

    // Cheap per-overlay live edits (index into the current overlay set; no geometry rebuild).
    void setOverlayTint(int index, const QColor& tint);
    void setOverlaySize(int index, float sizeScale);
    void setOverlayVisible(int index, bool visible);
    void setPrimaryVisible(bool visible);   // hide/show the reference (primary) structure
    void clearOverlays();
    int overlayCount() const;

private:
    int addOverlay(const QVector<Atom>& targetAtoms, const QColor& tint, float sizeScale,
        const QVector<Bond>& targetBonds = {});

public:

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

    // Claude Generated 2026 - Per-atom overlay labels (element/type/index).
    void setAtomLabelMode(AtomLabel mode);
    void setLabelSelectionOnly(bool on);  // true = only label selected atoms

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
    void centerAtOrigin();       // Translate all frames so COM = origin, reset camera
    void showFrame(int frameIndex);  // Show specific frame
    void nextFrame();               // Show next frame
    void previousFrame();           // Show previous frame

    // Claude Generated - Screenshot/Export functionality
    void saveScreenshot(const QString& filename, int scaleFactor = 1);
    void saveScreenshotDialog();
    /// High-quality image export: render the scene offscreen at an arbitrary resolution
    /// (true supersampling, not upscaling) and save it. @p background: 0 = scene colour,
    /// 1 = white, 2 = transparent (alpha PNG). Claude Generated 2026.
    /// High-quality image export: render the scene offscreen at an arbitrary resolution
    /// (true supersampling, not upscaling) and save it. @p background: 0 = scene colour,
    /// 1 = white, 2 = transparent (alpha PNG). @p metadata is written as PNG text chunks.
    /// Claude Generated 2026.
    /// @p background: 0 = scene colour, 1 = white, 2 = transparent, 3 = @p bgColor.
    bool exportImage(const QString& path, int width, int height, int background, bool ssaa,
                     const ImageMetadata& metadata, const QColor& bgColor = QColor());
    void exportImageDialog(const QString& startDir = QString(), Settings* settings = nullptr);

    /// Quick, dialog-free export (the viewer-bar "Photo" button): saves
    /// <stem>_<timestamp>.png (transparent, 2× viewport, metadata embedded) into
    /// @p startDir and emits imageExported(). Returns the saved path, or an empty
    /// string on failure. Claude Generated 2026.
    QString quickExportImage(const QString& startDir = QString(), Settings* settings = nullptr);

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
    int getMeasurementMode() const { return m_measurementMode; }
    void selectAtom(int index, bool append = false);
    SelectionManager* getSelectionManager() const { return m_selectionManager; }

    // ----- Structure editing (Explore-mode "Edit" toggle) -------------------
    // Claude Generated 2026 - direct coordinate editing: mark atoms/molecules, move
    // them, copy/paste, merge a file, with collision feedback. Distinct from the
    // simulation grab-force (which injects forces into a running MD/Opt).
    /// Enable/disable the Edit interaction mode (mutually exclusive with measure/bond-edit).
    void setEditMode(bool on);
    bool editMode() const { return m_editMode; }
    /// Select all atoms of the connected fragment that @p seedAtom belongs to.
    void selectFragment(int seedAtom, bool append = false);
    /// Bulk-select a list of atom indices (used by fragment/paste/merge).
    void selectAtoms(const QVector<int>& indices, bool append = false);
    /// Copy the current selection (atoms + internal bonds) into the clipboard.
    void copySelection();
    /// Paste the clipboard into the current frame (offset, selected, ready to move).
    void pasteClipboard();
    /// Delete the selected atoms (and incident bonds) from the current frame.
    void deleteSelection();
    /// Translate the moving selection along the net overlap direction until clash-free.
    void resolveClashes();
    /// True when structural edits (add/remove atoms) are allowed (single frame only).
    bool canEditStructure() const { return m_frameCount <= 1; }
    int getCollisionCount() const { return m_collisionAtoms.size(); }
    /// Rotate the scene (or, with @p nudge in Edit mode, translate the selection) from a
    /// WASD/QE key. Driven by MainWindow's application-level key filter (focus-independent).
    void rotateSceneByKey(int key, bool nudge);
    /// Pin the cursor at the press point during a move-drag (relative/infinite drag).
    /// Default on; togglable from the Edit menu. (Cursor warping needs X11; on Wayland
    /// it may be a no-op — the drag still works, the cursor just isn't pinned.)
    void setDragCursorLock(bool on) { m_dragCursorLock = on; }
    bool dragCursorLock() const { return m_dragCursorLock; }
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

    // Claude Generated 2026 - External-edit entry points for structure sync
    // (atom table / text editor). Both mutate the current frame, update the scene
    // bounds-preservingly (no camera jump), and emit moleculeUpdated().
    /** @brief Set one atom's element + position in the current frame (table edit). */
    void setAtomInCurrentFrame(int index, const QString& element, const QVector3D& position);
    /** @brief Replace the whole current-frame geometry from parsed atoms (text
     *  "Apply"). Single-frame structures only (canEditStructure); re-detects bonds.
     *  Returns false if rejected (multi-frame or empty). */
    bool applyStructureFromAtoms(const QVector<Atom>& atoms);

    /**
     * @brief Update atom positions for live simulation. When dynamic bonds are enabled, the bond
     * graph is re-detected from the new geometry each frame so bond breaking/formation in MD/Opt
     * reactions is reflected by the drawn bonds.
     */
    void updateSimulationFrame(SimulationFramePtr frame);

    /** @brief Enable/disable per-frame bond re-detection during live MD/Opt (default on).
     *  Claude Generated 2026 - shows bond breaking/formation in reactions. */
    void setDynamicBonds(bool on) { m_dynamicBonds = on; }
    bool dynamicBonds() const { return m_dynamicBonds; }

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

    // Claude Generated 2026 - Reproducible view presets (camera + display).
    ViewPreset currentViewPreset(ZoomMode zoomMode = ZoomMode::Absolute) const;
    void applyViewPreset(const ViewPreset& preset, bool applyCamera = true, bool applyDisplay = true);
    /// Set only the camera rotation (Quick orientation buttons).
    void setCameraOrientation(const QQuaternion& rotation);

signals:
    void frameChanged(int frameIndex);
    void trajectoryLoaded(int frameCount);
    void selectionChanged(const QVector<int>& selectedAtoms);

    void moleculeUpdated(const QVector<MoleculeViewer::Atom>& atoms,
        const QVector<MoleculeViewer::Bond>& bonds);

    /// Claude Generated 2026 - New NCI contact list for the current frame.
    void nciResultChanged(const nci::Result& result);

    /// Claude Generated 2026 - The set of bead types changed (structure loaded or
    /// edited), so a type-colour selector has to be rebuilt.
    void beadTypesChanged();

    /// Claude Generated 2026 - The interaction colour palette changed.
    void nciPaletteChanged();

    /// Claude Generated 2026 - The fragment decomposition changed (structure loaded
    /// or bonds edited), so a fragment selector has to be rebuilt.
    void fragmentsChanged();

    void atomForceRequested(int atomIndex, QVector3D force, double alpha, int maxShells);
    void atomGrabReleased();
    void grabStatusChanged(QString message);

    // Claude Generated 2026 - Display-dock sync: emitted when these change via any
    // path (bar combo, dock, shortcut) so the other UI follows. + a request to
    // surface the Display dock (the bar's "Display" button).
    void renderingModeChanged(MoleculeViewer::RenderingMode mode);
    void colorSchemeChanged(MoleculeViewer::ColorScheme scheme);
    void measurementModeChanged(int mode);  // 0=off,1=distance,2=angle,3=dihedral
    void displayOptionsRequested();
    // Claude Generated 2026 - Structure editing.
    void editModeChanged(bool on);
    void collisionCountChanged(int count);  // clashing atoms in the current frame
    // Emitted just BEFORE a structural edit (move/paste/merge/delete) so a listener
    // can snapshot the pre-edit geometry for undo (restore via the Snapshots tab).
    void editSnapshotRequested(const QString& label);
    // Claude Generated 2026 - confinement-wall boundary violations for the
    // current frame. Emitted when the count changes; 0 = all atoms inside.
    void wallViolationChanged(int count);
    // Claude Generated 2026 - emitted after applyViewPreset() so the DisplayPanel
    // can re-sync its controls without the dock being raised.
    void viewPresetApplied();

    // Claude Generated 2026 - emitted after an image was successfully exported
    // (exportImageDialog / quickExportImage), so the image-gallery dock can list
    // it for batch trimming.
    void imageExported(const QString& path);

    // Claude Generated 2026 - the viewer-bar "Photo" button asks the host to run a
    // quick export (the host supplies the working dir + operator settings).
    void quickExportRequested();

public slots:
    void setSimulationActive(bool on);
    bool simulationActive() const { return m_simulationActive; }  // for the WASD/QE key filter
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

    /** Confinement-wall overlay driven by the Simulation config (curcuma harmonic
     *  walls). @p on enables it; @p type is 0=none,1=spheric,2=rect. The box is
     *  drawn in intrinsic atom coordinates and rotates with the molecule. The
     *  Display-panel toggle (setWallVisibleOverride) can hide it independently. */
    void setConfinementBox(bool on, int type, const QVector3D& min,
                           const QVector3D& max, float radius);
    /** Independent show/hide for the wall wireframe (Display panel checkbox). */
    void setWallVisibleOverride(bool on);
    /** Wireframe transparency 0..1 (Display panel slider). */
    void setWallOpacity(qreal opacity);
    bool isWallVisible() const { return m_wallEnabled && m_wallVisibleOverride; }
    bool getWallVisibleOverride() const { return m_wallVisibleOverride; }
    qreal getWallOpacity() const;
    /** Count of current-frame atoms outside the configured wall region. */
    int getWallViolationCount() const { return m_wallViolationCount; }
    /** Enable/disable the iso-potential shell overlay (Display panel checkbox). */
    void setWallPotentialViz(bool enabled);
    bool getPotVizEnabled() const { return m_potVizEnabled; }
    /** Update the potential parameters driving the shell distances; rebuilds
     *  the shells (and arrows if enabled) when viz is active. */
    void setWallPotentialParams(bool harmonic, double wallTemp, float wallBeta);
    /** Enable/disable the wall force vector field and set its resolution. */
    void setWallVectorField(bool enabled, int resolution);
    bool getPotArrowsEnabled() const { return m_potArrowsEnabled; }
    int  getPotArrowResolution() const { return m_potArrowResolution; }

    // Claude Generated 2026 - Non-covalent interaction (NCI) overlay.
    /** Overlay source: 0 = off, 1 = geometry, 2 = GFN-FF parameters,
     *  3 = population analysis. Geometry recomputes per frame; the two calculated
     *  sources are pushed in from the analysis worker via setNciResult(). */
    void setNciSource(int source);
    int getNciSource() const { return m_nciSource; }
    /** Detection thresholds and type filters (Display panel). */
    void setNciOptions(const nci::Options& options);
    const nci::Options& getNciOptions() const { return m_nciOptions; }
    /** Show the interaction distance at the midpoint of each contact. */
    void setNciLabelsVisible(bool on);
    bool getNciLabelsVisible() const { return m_nciLabelsVisible; }
    /** Adopt a calculated result (GFN-FF / population) and draw it. */
    void setNciResult(const nci::Result& result);
    const nci::Result& getNciResult() const { return m_nciResult; }
    /** Per-atom charges from a calculation, feeding the "By Charge" colour scheme. */
    void setAtomCharges(const QVector<float>& charges);
    /** Overlay colour of one interaction class (see nci::paletteEntries()). */
    void setNciKindColor(int paletteKey, const QColor& color);
    QColor getNciKindColor(int paletteKey) const;
    /** Drop all overrides and go back to the default palette. */
    void resetNciKindColors();
    const nci::Palette& getNciPalette() const { return m_nciPalette; }
    void setNciPalette(const nci::Palette& palette);

    // Claude Generated 2026 - Coarse-grained bead types (VTF). The type list comes
    // from the loaded structure, so the UI can offer exactly the types present.
    /** Distinct bead type labels with their bead counts; empty for all-atom structures. */
    QVector<QPair<QString, int>> getBeadTypes() const;
    /** Colour the "By Type" scheme currently draws this type with. */
    QColor getBeadTypeColor(const QString& type) const;
    /** Override the colour of one bead type; an invalid colour restores the automatic one. */
    void setBeadTypeColor(const QString& type, const QColor& color);
    /** Drop all overrides and go back to the derived colours. */
    void resetBeadTypeColors();
    /** Only the user-set overrides, for persisting them. */
    QHash<QString, QColor> getBeadTypeColors() const;
    void setBeadTypeColors(const QHash<QString, QColor>& colors);

    // Claude Generated 2026 - Fragment tinting, for host-guest systems: every
    // connected component of the bond graph gets a colour shift so guest molecules
    // stand out against the host without losing element identity.
    /** Fragments of the current structure as (formula, atom count), largest first. */
    QVector<QPair<QString, int>> getFragments() const;
    void setFragmentTint(bool on, float strength);
    bool getFragmentTint() const { return m_fragmentTint; }
    float getFragmentTintStrength() const { return m_fragmentTintStrength; }
    /** Tint colour of a fragment; invalid for fragment 0 (the untinted reference). */
    QColor getFragmentColor(int fragment) const;
    /** Set a fragment's hue; an invalid colour restores the automatic one. */
    void setFragmentColor(int fragment, const QColor& color);
    /** Whether this fragment carries a picked hue rather than the automatic one. */
    bool hasFragmentColorOverride(int fragment) const;
    /** Draw scale of the non-reference fragments (1.0 = unchanged). */
    void setFragmentScale(float scale);
    float getFragmentScale() const { return m_fragmentScale; }
    /** Effective scale of one fragment (override, else the global value). */
    float getFragmentScaleFor(int fragment) const;
    /** A scale <= 0 clears the override for that fragment. */
    void setFragmentScaleOverride(int fragment, float scale);
    /** Tint strength of one fragment; a negative value clears its override. */
    float getFragmentTintStrengthFor(int fragment) const;
    void setFragmentTintStrengthOverride(int fragment, float strength);
    /** Clear every per-fragment override (colour, tint strength, size). */
    void resetFragmentOverrides();

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

    // Assemble reproducibility/authorship metadata from the current view + operator
    // settings. Shared by exportImageDialog and quickExportImage. Claude Generated 2026.
    ImageMetadata buildImageMetadata(Settings* settings, const QString& presetName);

    // Push the current frame's atoms/bonds into the SceneController.
    // resetCamera=false keeps the camera for in-place rebuilds (refresh).
    void syncSceneToController(int frameIndex, bool resetCamera, bool fullRebuild,
        bool keepView = false);
    void applyAppearanceToController();  // mirror appearance/effect state into the scene
    void updateFramePositions(int frameIndex);  // fast position-only update (animation)

    void clearScene();          // Private implementation
    void refreshVisualization();// Refresh without camera reset

    // Element data helpers (kept for getCurrentFrame* and bond detection).
    float getCovalentRadius(const QString& element);
    QVector<Bond> detectBonds(const QVector<Atom>& atoms);
    // Claude Generated 2026 - per-frame bond re-detection with hysteresis (form tighter than break)
    // so thermally vibrating bonds near the cutoff don't flicker on/off every frame.
    QVector<Bond> detectBondsHysteresis(const QVector<Atom>& atoms, const QVector<Bond>& previous);

    void setDefaultView();
    QVector3D modelToWorld(const QVector3D& localPos) const;

    // Force-vector overlay (opt-in)
    void buildForceAdjacency();
    void updateForceVectors();  // recompute arrows from the current grab force

    // Measurement overlay (M2): recompute lines + value from the selected atoms.
    void updateMeasurement();
    // Bond editing via an atom pair (M2): add/delete/cycle the bond between a and b.
    void performBondEdit(int a, int b);

    // Mouse interaction (drives the SceneController transform).
    int pickAtomAtScreenPos(const QPoint &screenPos) const;
    QVector3D computeGrabForce(const QPoint &mousePos, int atomIndex) const;
    void handleMouseRotation(const QPoint& currentPos);
    // Claude Generated 2026 - apply an incremental model rotation (degrees about the
    // view's horizontal/vertical/roll axes); shared by mouse drag and keyboard rotation.
    void applyModelRotation(float horizDeg, float vertDeg, float rollDeg = 0.0f);
    void handleMousePan(const QPoint& currentPos);
    void handleMouseZoom(int delta);

    // --- Qt Quick 3D backing ---
    QQuickView* m_quickView = nullptr;
    QWidget* m_container = nullptr;
    SceneController* m_scene = nullptr;
    QWidget* m_controlPanel = nullptr;

    // Claude Generated - Frame control widgets (shown/hidden based on frame count)
    QWidget *m_frameControlWidget = nullptr;
    QWidget *m_playbackWidget = nullptr;  // play/pause/fps/loop — only for multi-frame files
    QSlider *m_frameSlider = nullptr;
    QLabel *m_frameLabel = nullptr;
    QSpinBox *m_frameJumpBox = nullptr;

    // 4 screen-fixed corner lights (state mirrored into the scene controller).
    bool m_cornerLightEnabled[4] = {true, true, false, false};
    QColor m_backgroundColor{32, 36, 44};

    // Claude Generated 2026 - viewer-bar "Photo" quick-export options (transparent
    // toggle + background colour preset). m_photoBgColor invalid = use scene colour.
    bool m_photoTransparent = true;
    QColor m_photoBgColor;

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
    float m_fogIntensity = 0.7f;
    float m_fogDistance = 0.2f;
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

    // Claude Generated 2026 - Structure editing state
    bool m_editMode = false;
    bool m_movingSelection = false;       // a drag-move of the selection is in progress
    bool m_moveSnapshotTaken = false;     // pre-move undo snapshot taken for this drag
    bool m_emptyPressPending = false;     // left-press on empty space (clears on release)
    bool m_rubberBanding = false;         // box-select drag in progress
    QPoint m_rubberStart;                 // box-select anchor (viewport pixels)
    bool m_dragCursorLock = true;         // pin cursor at press point during move-drag
    QPoint m_dragAnchorGlobal;            // global cursor pos to warp back to (cursor lock)
    QVector3D m_moveRefLocal;             // selection centroid (intrinsic) for drag depth scale
    QVector<int> m_collisionAtoms;        // clashing atoms in the current frame
    QVector<Atom> m_clipboardAtoms;       // copy/paste buffer
    QVector<Bond> m_clipboardBonds;       // bonds internal to the clipboard (re-indexed)
    static constexpr float kClashFactor = 0.6f;  // clash if dist < factor * (vdw_i + vdw_j)
    static constexpr float kNudgeStep = 0.1f;    // arrow-key nudge (Angstrom)
    // Helpers
    QVector3D selectionCentroidLocal() const;     // mean position of selected atoms (current frame)
    void computeCollisions();                      // recolour clashes + emit collisionCountChanged
    void moveSelection(const QVector3D& modelDelta); // translate selected atoms, redraw, recheck
    void finalizeEdit();                           // re-detect bonds + recompute collisions after a move/edit

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
    bool m_dynamicBonds = true;  // Claude Generated 2026 - re-detect bonds each live frame (reactions)

    // Claude Generated 2026 - Non-covalent interaction overlay state.
    int m_nciSource = 0;               // 0=off, 1=geometry, 2=gfnff, 3=population
    bool m_nciLabelsVisible = true;
    nci::Options m_nciOptions;
    nci::Result m_nciResult;
    QVector<QVector<int>> m_nciRings;  // ring perception cache (topology, not geometry)
    bool m_nciRingsValid = false;
    bool m_nciLiveContacts = false;    // next refresh got a raw list from the force field
    nci::Palette m_nciPalette;        // user overrides of the interaction colours
    bool m_fragmentTint = false;      // mirrored for view presets
    float m_fragmentTintStrength = 0.6f;
    float m_fragmentScale = 1.0f;     // mirrored for view presets
    /// Recompute (geometry source) or re-fit (calculated sources) the overlay for
    /// the current frame and push it to the scene.
    void refreshNciOverlay();
    /// Translate the current contact list into scene overlay segments.
    void pushNciToScene();
    /// Drop the ring cache after a topology change (bond edit, dynamic bonds).
    void invalidateNciTopology() { m_nciRingsValid = false; }
    /// Above this atom count the per-frame geometric scan is skipped unless the
    /// user explicitly asks for it (the close-contact pass is a full O(N^2)).
    static constexpr int kNciAtomLimit = 5000;

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
    QPoint m_rightPressPos;      // where the right button went down (click vs pan-drag)
    bool m_rightDragged = false; // right-click (no drag) clears the selection

    // Claude Generated 2026 - Interactive-sim grab state
    bool m_simulationActive = false;
    int m_grabbedAtom = -1;
    double m_grabStrength = 0.1;
    double m_grabAlpha = 0.4;
    int m_grabMaxShells = 3;

    // Force-vector overlay (opt-in)
    bool m_forceVectorsVisible = false;
    QVector<QVector<int>> m_forceAdjacency;

    // Iso-potential shell + vector-field viz state (mirrors SceneController params).
    bool m_potVizEnabled = false;
    bool m_wallHarmonic = true;
    double m_wallTemp = 298.15;
    float m_wallBeta = 6.0f;
    bool m_potArrowsEnabled = false;
    int  m_potArrowResolution = 4;

    // Confinement-wall overlay (driven by Simulation config; hideable via the
    // Display panel). m_wallEnabled reflects the config; m_wallVisibleOverride is
    // the Display-panel checkbox. The wireframe is shown only when both are true.
    bool m_wallEnabled = false;
    int m_wallType = 0;
    QVector3D m_wallMin, m_wallMax;
    float m_wallRadius = 0.0f;
    bool m_wallVisibleOverride = true;
    int m_wallViolationCount = 0;       // atoms currently outside the wall region
    void applyWallVisibility();  // rebuild/hide geometry from current state
    /// Recompute out-of-bounds atom count from the current frame; recolours the
    /// wall wireframe (red on violations) and emits wallViolationChanged.
    void computeWallViolations();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
};

#endif // MOLECULEVIEWER_H
