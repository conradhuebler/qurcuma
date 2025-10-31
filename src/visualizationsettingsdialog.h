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
#include "view.h"

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
    explicit VisualizationSettingsDialog(MoleculeViewer* viewer, QWidget *parent = nullptr);
    ~VisualizationSettingsDialog() override;

private slots:
    void onRenderingModeChanged(int index);
    void onColorSchemeChanged(int index);
    void onAtomTransparencyChanged(int value);
    void onAtomShininessChanged(double value);
    void onAtomScaleChanged(double value);
    void onBondThicknessChanged(double value);
    void onResetDefaults();
    void onApply();

private:
    void setupUI();
    void loadCurrentSettings();
    void createRenderingGroup(QVBoxLayout* mainLayout);
    void createMaterialGroup(QVBoxLayout* mainLayout);
    void createSizeGroup(QVBoxLayout* mainLayout);

    // Widgets
    QComboBox* m_renderingModeCombo;
    QComboBox* m_colorSchemeCombo;
    QSlider* m_transparencySlider;
    QLabel* m_transparencyLabel;
    QDoubleSpinBox* m_shininessSpinBox;
    QDoubleSpinBox* m_atomScaleSpinBox;
    QDoubleSpinBox* m_bondThicknessSpinBox;

    QPushButton* m_resetButton;
    QPushButton* m_applyButton;
    QPushButton* m_closeButton;

    MoleculeViewer* m_viewer;
};

#endif // VISUALIZATIONSETTINGSDIALOG_H
