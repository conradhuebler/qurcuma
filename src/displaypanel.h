// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// DisplayPanel — the detailed display options inside the Appearance dock. Style
// (mode, colours, sizes, labels, background), fragment and bead-type colours sit at
// the top; material, lighting and effects in a collapsed "Advanced" section. All
// controls drive MoleculeViewer's public setters live. Claude Generated 2026.
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
class QVBoxLayout;
class QScrollArea;
class CollapsibleSection;

class DisplayPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DisplayPanel(MoleculeViewer* viewer, Settings* settings = nullptr, QWidget* parent = nullptr);

    /// Re-read all control values from the viewer (called after shortcuts, looks
    /// or external changes so the panel stays in sync). Read-only: the viewer is
    /// the single source of truth, this never writes viewer state.
    void syncFromViewer();

    /// Expand one collapsible section by its stable key (today only "advanced")
    /// and scroll it into view. Claude Generated 2026.
    void expandSection(const QString& key);

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
    // Footer
    void onResetDefaults();

private:
    void setupUI();
    // Group builders.
    void createStyleGroup(QVBoxLayout* layout);     // mode, colours, sizes, labels, background
    void createMaterialGroup(QVBoxLayout* layout);
    void createEffectsGroup(QVBoxLayout* layout);   // fog, SSAO, bloom, HDR
    void createLightingGroup(QVBoxLayout* layout);  // corner lights
    void createBeadTypeGroup(QVBoxLayout* layout);  // per-bead-type colours (CG beads)
    void createFragmentGroup(QVBoxLayout* layout);  // per-fragment tint (host-guest)
    /// Rebuild the bead-type selector from the loaded structure.
    void refreshBeadTypes();
    /// Rebuild the fragment selector from the loaded structure.
    void refreshFragments();
    /// Load the selected fragment's own values into the per-fragment controls.
    void refreshSelectedFragment();

    // Style
    QComboBox* m_renderingModeCombo = nullptr;
    QComboBox* m_colorSchemeCombo = nullptr;
    QSlider* m_transparencySlider = nullptr;
    QLabel* m_transparencyLabel = nullptr;
    QDoubleSpinBox* m_shininessSpinBox = nullptr;
    QDoubleSpinBox* m_atomScaleSpinBox = nullptr;
    QDoubleSpinBox* m_bondThicknessSpinBox = nullptr;
    QPushButton* m_bgColorButton = nullptr;

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

    // Collapsible sections by stable key (expand-state persistence, expandSection).
    QHash<QString, CollapsibleSection*> m_sections;
    QScrollArea* m_scroll = nullptr;

    MoleculeViewer* m_viewer = nullptr;
    Settings* m_settings = nullptr;
};
