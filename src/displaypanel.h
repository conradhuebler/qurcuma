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

    /// Re-read all control values from the viewer/settings (called after shortcuts
    /// or external changes so the panel stays in sync).
    void loadCurrentSettings();

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
    QComboBox* m_measureCombo = nullptr;
    QComboBox* m_bondEditCombo = nullptr;
    QCheckBox* m_forceVectorsCheck = nullptr;
    QComboBox* m_rotationModeCombo = nullptr;
    QSpinBox* m_instancingThresholdSpin = nullptr;

    // Presets
    QListWidget* m_presetList = nullptr;

    MoleculeViewer* m_viewer = nullptr;
    Settings* m_settings = nullptr;
};
