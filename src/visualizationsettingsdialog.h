// Claude Generated - Visualization Settings Dialog
#ifndef VISUALIZATIONSETTINGSDIALOG_H
#define VISUALIZATIONSETTINGSDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QCheckBox>  // Claude Generated
#include <QTabWidget>  // Claude Generated - Presets tab
#include <QListWidget>  // Claude Generated - Preset list
#include "view.h"
#include "settings.h"

/**
 * @brief Dialog for adjusting molecular visualization settings
 *
 * Provides controls for:
 * - Rendering modes (Ball-and-Stick, Wireframe, Space-Filling, Sticks-Only)
 * - Color schemes (CPK, Monochrome, By-Charge, Custom)
 * - Material properties (transparency, shininess)
 * - Size adjustments (atom scale, bond thickness)
 */
class VisualizationSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // Claude Generated - Constructor with Settings integration
    explicit VisualizationSettingsDialog(MoleculeViewer* viewer, Settings* settings = nullptr, QWidget *parent = nullptr);
    ~VisualizationSettingsDialog() override;

protected:
    // Claude Generated - Load saved settings on dialog show
    void showEvent(QShowEvent* event) override;

private slots:
    void onRenderingModeChanged(int index);
    void onColorSchemeChanged(int index);
    void onAtomTransparencyChanged(int value);
    void onAtomShininessChanged(double value);
    void onAtomScaleChanged(double value);
    void onBondThicknessChanged(double value);
    void onFogEnabledChanged(bool enabled);  // Claude Generated
    void onFogIntensityChanged(int value);   // Claude Generated
    // Claude Generated - Phase 5A: SSAO post-processing controls
    void onSSAOEnabledChanged(bool enabled);
    void onSSAOIntensityChanged(int value);
    void onSSAORadiusChanged(double value);
    void onSSAOBiasChanged(double value);
    void onResetDefaults();
    void onApply();

    // Claude Generated - Preset management slots
    void onLoadPreset(int index);
    void onSavePreset();
    void onDeletePreset();
    void loadQuickPreset(const QString& presetName);

public:
    // Claude Generated - Make public for synchronization with shortcuts
    void loadCurrentSettings();

private:
    void setupUI();
    void setupTabs();  // Claude Generated - Setup tabbed interface
    void refreshPresetList();  // Claude Generated
    void createRenderingGroup(QVBoxLayout* mainLayout);
    void createMaterialGroup(QVBoxLayout* mainLayout);
    void createSizeGroup(QVBoxLayout* mainLayout);
    void createAppearanceGroup(QVBoxLayout* mainLayout);  // Claude Generated - Fog effects
    void createPresetsTab(QTabWidget* tabs);  // Claude Generated

    // Widgets
    QComboBox* m_renderingModeCombo;
    QComboBox* m_colorSchemeCombo;
    QSlider* m_transparencySlider;
    QLabel* m_transparencyLabel;
    QDoubleSpinBox* m_shininessSpinBox;
    QDoubleSpinBox* m_atomScaleSpinBox;
    QDoubleSpinBox* m_bondThicknessSpinBox;

    // Claude Generated - Fog effect controls
    QCheckBox* m_fogEnabledCheckBox;
    QSlider* m_fogIntensitySlider;
    QLabel* m_fogIntensityLabel;

    // Claude Generated - Phase 5A: SSAO post-processing controls
    QCheckBox* m_ssaoEnabledCheckBox;
    QSlider* m_ssaoIntensitySlider;
    QLabel* m_ssaoIntensityLabel;
    QDoubleSpinBox* m_ssaoRadiusSpinBox;
    QDoubleSpinBox* m_ssaoBiasSpinBox;

    // Claude Generated - Preset management widgets
    QTabWidget* m_tabWidget;
    QListWidget* m_presetList;
    QPushButton* m_loadPresetButton;
    QPushButton* m_savePresetButton;
    QPushButton* m_deletePresetButton;
    QPushButton* m_publicationPresetButton;
    QPushButton* m_analysisPresetButton;
    QPushButton* m_presentationPresetButton;

    QPushButton* m_resetButton;
    QPushButton* m_applyButton;
    QPushButton* m_closeButton;

    MoleculeViewer* m_viewer;
    Settings* m_settings;  // Claude Generated - For persistence
};

#endif // VISUALIZATIONSETTINGSDIALOG_H
