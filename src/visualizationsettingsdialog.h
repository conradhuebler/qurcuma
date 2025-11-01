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
    void onResetDefaults();
    void onApply();

private:
    void setupUI();
    void loadCurrentSettings();
    void createRenderingGroup(QVBoxLayout* mainLayout);
    void createMaterialGroup(QVBoxLayout* mainLayout);
    void createSizeGroup(QVBoxLayout* mainLayout);
    void createAppearanceGroup(QVBoxLayout* mainLayout);  // Claude Generated - Fog effects

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

    QPushButton* m_resetButton;
    QPushButton* m_applyButton;
    QPushButton* m_closeButton;

    MoleculeViewer* m_viewer;
    Settings* m_settings;  // Claude Generated - For persistence
};

#endif // VISUALIZATIONSETTINGSDIALOG_H
