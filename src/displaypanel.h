// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// DisplayPanel — the docked "Display" panel: single home for all 3D-viewer
// appearance/effects/lighting/tools, organized as collapsible sections. Replaces
// the former modal VisualizationSettingsDialog (its wiring/presets/persistence are
// ported here verbatim). All controls drive MoleculeViewer's public setters live.
// Claude Generated 2026.
#pragma once

#include <QWidget>

#include "settings.h"
#include "view.h"

class QComboBox;
class QGroupBox;
class QSlider;
class QLabel;
class QPushButton;
class QToolButton;
class QCheckBox;
class QDoubleSpinBox;
class QSpinBox;
class QListWidget;
class QVBoxLayout;

class DisplayPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DisplayPanel(MoleculeViewer* viewer, Settings* settings = nullptr, QWidget* parent = nullptr);

    /// Re-read all control values from the viewer (called after shortcuts, presets
    /// or external changes so the panel stays in sync). Read-only: the viewer is
    /// the single source of truth, this never writes viewer state.
    void syncFromViewer();

signals:
    void centerOnLoadChanged(bool enabled);
    /** Emitted when the "Show potential gradient" checkbox changes. */
    void potGradientChanged(bool enabled);
    /** Emitted when the "Show force vectors" checkbox or resolution spinbox changes. */
    void potVectorFieldChanged(bool enabled, int resolution);
    /** Emitted when the NCI overlay source changes (0=off, 1=geometry,
     *  2=GFN-FF parameters, 3=population analysis). The calculated sources need a
     *  run of the analysis worker, which MainWindow owns. */
    void nciSourceChanged(int source);
    /** Emitted when the live-during-MD option changes; MainWindow forwards it to
     *  the simulation worker (it has to force GFN-FF's HB/XB list refresh). */
    void nciLiveMdChanged(bool enabled);

private slots:
    // Style
    void onRenderingModeChanged(int index);
    void onColorSchemeChanged(int index);
    void onAtomTransparencyChanged(int value);
    void onAtomShininessChanged(double value);
    void onAtomScaleChanged(double value);
    void onBondThicknessChanged(double value);
    // Effects
    void onFogEnabledChanged(bool enabled);
    void onFogIntensityChanged(int value);
    void onSSAOEnabledChanged(bool enabled);
    void onSSAOIntensityChanged(int value);
    void onSSAORadiusChanged(double value);
    void onSSAOBiasChanged(double value);
    void onBloomEnabledChanged(bool enabled);
    void onBloomThresholdChanged(double value);
    void onBloomIntensityChanged(int value);
    void onHDREnabledChanged(bool enabled);
    void onExposureChanged(double value);
    // Tools / interaction
    void onRotationModeChanged(int index);
    void onInstancingThresholdChanged(int value);
    // Footer / presets
    void onResetDefaults();
    void onSaveAsDefault();
    void onLoadDefaults();
    void onLoadPreset(int index);
    void onSavePreset();
    void onDeletePreset();
    void loadQuickPreset(const QString& presetName);

private:
    void setupUI();
    void refreshPresetList();
    // Section content builders (reused from the former dialog).
    void createRenderingGroup(QVBoxLayout* layout);
    void createMaterialGroup(QVBoxLayout* layout);
    void createSizeGroup(QVBoxLayout* layout);
    void createAppearanceGroup(QVBoxLayout* layout); // SSAO/Bloom/HDR/Fog
    void createLightingGroup(QVBoxLayout* layout);   // corner lights + background (new)
    void createToolsGroup(QVBoxLayout* layout);      // measure/bond-edit/force + interaction (new)
    void createBeadTypeGroup(QVBoxLayout* layout);  // per-bead-type colours (CG beads)
    void createFragmentGroup(QVBoxLayout* layout);  // per-fragment tint (host-guest)
    void createNciGroup(QVBoxLayout* layout);       // non-covalent interaction overlay
    /// Rebuild the bead-type selector from the loaded structure.
    void refreshBeadTypes();
    /// Re-read the interaction colours from the viewer into the selector.
    void refreshNciPalette();
    /// Rebuild the fragment selector from the loaded structure.
    void refreshFragments();
    /// Load the selected fragment's own values into the per-fragment controls.
    void refreshSelectedFragment();
    /// Collect the NCI widgets into nci::Options and push them to the viewer.
    void applyNciOptions();
    void createPresetsGroup(QVBoxLayout* layout);

    // Style
    QComboBox* m_renderingModeCombo = nullptr;
    QComboBox* m_colorSchemeCombo = nullptr;
    QSlider* m_transparencySlider = nullptr;
    QLabel* m_transparencyLabel = nullptr;
    QDoubleSpinBox* m_shininessSpinBox = nullptr;
    QDoubleSpinBox* m_atomScaleSpinBox = nullptr;
    QDoubleSpinBox* m_bondThicknessSpinBox = nullptr;

    // Effects
    QCheckBox* m_fogEnabledCheckBox = nullptr;
    QSlider* m_fogIntensitySlider = nullptr;
    QLabel* m_fogIntensityLabel = nullptr;
    QSlider* m_fogDistanceSlider = nullptr;
    QCheckBox* m_ssaoEnabledCheckBox = nullptr;
    QSlider* m_ssaoIntensitySlider = nullptr;
    QLabel* m_ssaoIntensityLabel = nullptr;
    QDoubleSpinBox* m_ssaoRadiusSpinBox = nullptr;
    QDoubleSpinBox* m_ssaoBiasSpinBox = nullptr;
    QCheckBox* m_bloomEnabledCheckBox = nullptr;
    QDoubleSpinBox* m_bloomThresholdSpinBox = nullptr;
    QSlider* m_bloomIntensitySlider = nullptr;
    QLabel* m_bloomIntensityLabel = nullptr;
    QCheckBox* m_hdrEnabledCheckBox = nullptr;
    QDoubleSpinBox* m_exposureSpinBox = nullptr;

    // Lighting
    QToolButton* m_cornerLightButtons[4] = { nullptr, nullptr, nullptr, nullptr };
    QPushButton* m_bgColorButton = nullptr;

    // Tools / interaction
    QCheckBox* m_measureCheck = nullptr;  // measurement on/off (type auto-detected by atom count)
    QComboBox* m_bondEditCombo = nullptr;
    QCheckBox* m_forceVectorsCheck = nullptr;
    QCheckBox* m_wallCheck = nullptr;          // confinement-wall wireframe show/hide
    QSlider* m_wallOpacitySlider = nullptr;    // confinement-wall wireframe transparency
    QLabel* m_wallOpacityLabel = nullptr;
    QCheckBox* m_potGradientCheck = nullptr;   // iso-potential shell overlay show/hide
    QCheckBox* m_potArrowCheck = nullptr;      // wall force vector field show/hide
    QSpinBox*  m_potArrowResSpin = nullptr;    // vector field resolution (points per axis)
    QComboBox* m_rotationModeCombo = nullptr;
    QSpinBox* m_instancingThresholdSpin = nullptr;

    // Non-covalent interactions
    QComboBox* m_nciSourceCombo = nullptr;
    QCheckBox* m_nciHBondCheck = nullptr;
    QCheckBox* m_nciXBondCheck = nullptr;
    QCheckBox* m_nciPiCheck = nullptr;
    QCheckBox* m_nciContactCheck = nullptr;
    QCheckBox* m_nciElectrostaticCheck = nullptr;  // GFN-FF source only
    QCheckBox* m_nciDispersionCheck = nullptr;     // GFN-FF source only
    QDoubleSpinBox* m_nciHbDistanceSpin = nullptr;
    QSpinBox* m_nciHbAngleSpin = nullptr;
    QCheckBox* m_nciLabelCheck = nullptr;
    QCheckBox* m_nciLiveMdCheck = nullptr;
    QComboBox* m_nciKindCombo = nullptr;
    QPushButton* m_nciKindColorButton = nullptr;

    // Coarse-grained bead types
    QGroupBox* m_beadTypeGroup = nullptr;
    QLabel* m_beadInfoLabel = nullptr;
    QComboBox* m_beadTypeCombo = nullptr;
    QPushButton* m_beadColorButton = nullptr;
    QLabel* m_beadSchemeHint = nullptr;

    // Fragments (host-guest)
    QGroupBox* m_fragmentGroup = nullptr;
    QCheckBox* m_fragmentTintCheck = nullptr;
    QSlider* m_fragmentStrengthSlider = nullptr;
    QLabel* m_fragmentStrengthLabel = nullptr;
    QComboBox* m_fragmentCombo = nullptr;
    QPushButton* m_fragmentColorButton = nullptr;
    QPushButton* m_fragmentColorResetButton = nullptr;
    QSlider* m_fragmentScaleSlider = nullptr;
    QLabel* m_fragmentScaleLabel = nullptr;
    QGroupBox* m_fragmentSelectedGroup = nullptr;

    // Presets
    QListWidget* m_presetList = nullptr;

    MoleculeViewer* m_viewer = nullptr;
    Settings* m_settings = nullptr;
};
