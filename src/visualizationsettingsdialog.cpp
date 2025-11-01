// Claude Generated - Visualization Settings Dialog Implementation
#include "visualizationsettingsdialog.h"
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QShowEvent>

VisualizationSettingsDialog::VisualizationSettingsDialog(MoleculeViewer* viewer, Settings* settings, QWidget *parent)
    : QDialog(parent)
    , m_viewer(viewer)
    , m_settings(settings)
{
    setWindowTitle(tr("Visualization Settings"));
    setMinimumWidth(400);
    setupUI();
    loadCurrentSettings();
}

VisualizationSettingsDialog::~VisualizationSettingsDialog()
{
}

void VisualizationSettingsDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Create grouped sections
    createRenderingGroup(mainLayout);
    createMaterialGroup(mainLayout);
    createSizeGroup(mainLayout);
    createAppearanceGroup(mainLayout);  // Claude Generated - Fog controls

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_resetButton = new QPushButton(tr("Reset to Defaults"), this);
    m_applyButton = new QPushButton(tr("Apply"), this);
    m_closeButton = new QPushButton(tr("Close"), this);

    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_applyButton);
    buttonLayout->addWidget(m_closeButton);

    mainLayout->addLayout(buttonLayout);

    // Connect buttons
    connect(m_resetButton, &QPushButton::clicked, this, &VisualizationSettingsDialog::onResetDefaults);
    connect(m_applyButton, &QPushButton::clicked, this, &VisualizationSettingsDialog::onApply);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

void VisualizationSettingsDialog::createRenderingGroup(QVBoxLayout* mainLayout)
{
    QGroupBox* renderingGroup = new QGroupBox(tr("Rendering"), this);
    QFormLayout* formLayout = new QFormLayout(renderingGroup);

    // Rendering Mode
    m_renderingModeCombo = new QComboBox(this);
    m_renderingModeCombo->addItem(tr("Ball and Stick"), static_cast<int>(MoleculeViewer::RenderingMode::BallAndStick));
    m_renderingModeCombo->addItem(tr("Space Filling"), static_cast<int>(MoleculeViewer::RenderingMode::SpaceFilling));
    m_renderingModeCombo->addItem(tr("Wireframe"), static_cast<int>(MoleculeViewer::RenderingMode::Wireframe));
    m_renderingModeCombo->addItem(tr("Sticks Only"), static_cast<int>(MoleculeViewer::RenderingMode::SticksOnly));
    connect(m_renderingModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VisualizationSettingsDialog::onRenderingModeChanged);
    formLayout->addRow(tr("Mode:"), m_renderingModeCombo);

    // Color Scheme
    m_colorSchemeCombo = new QComboBox(this);
    m_colorSchemeCombo->addItem(tr("CPK (Element Colors)"), static_cast<int>(MoleculeViewer::ColorScheme::CPK));
    m_colorSchemeCombo->addItem(tr("Monochrome"), static_cast<int>(MoleculeViewer::ColorScheme::Monochrome));
    m_colorSchemeCombo->addItem(tr("By Charge"), static_cast<int>(MoleculeViewer::ColorScheme::ByCharge));
    m_colorSchemeCombo->addItem(tr("Custom"), static_cast<int>(MoleculeViewer::ColorScheme::Custom));
    connect(m_colorSchemeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VisualizationSettingsDialog::onColorSchemeChanged);
    formLayout->addRow(tr("Colors:"), m_colorSchemeCombo);

    mainLayout->addWidget(renderingGroup);
}

void VisualizationSettingsDialog::createMaterialGroup(QVBoxLayout* mainLayout)
{
    QGroupBox* materialGroup = new QGroupBox(tr("Material Properties"), this);
    QFormLayout* formLayout = new QFormLayout(materialGroup);

    // Transparency slider (0-100%)
    QHBoxLayout* transparencyLayout = new QHBoxLayout();
    m_transparencySlider = new QSlider(Qt::Horizontal, this);
    m_transparencySlider->setRange(0, 100);
    m_transparencySlider->setValue(100);
    m_transparencyLabel = new QLabel("100%", this);
    m_transparencyLabel->setMinimumWidth(50);
    transparencyLayout->addWidget(m_transparencySlider);
    transparencyLayout->addWidget(m_transparencyLabel);
    connect(m_transparencySlider, &QSlider::valueChanged,
            this, &VisualizationSettingsDialog::onAtomTransparencyChanged);
    formLayout->addRow(tr("Transparency:"), transparencyLayout);

    // Shininess spinner (0-200)
    m_shininessSpinBox = new QDoubleSpinBox(this);
    m_shininessSpinBox->setRange(0.0, 200.0);
    m_shininessSpinBox->setValue(80.0);
    m_shininessSpinBox->setSingleStep(5.0);
    m_shininessSpinBox->setSuffix("");
    connect(m_shininessSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &VisualizationSettingsDialog::onAtomShininessChanged);
    formLayout->addRow(tr("Shininess:"), m_shininessSpinBox);

    mainLayout->addWidget(materialGroup);
}

void VisualizationSettingsDialog::createSizeGroup(QVBoxLayout* mainLayout)
{
    QGroupBox* sizeGroup = new QGroupBox(tr("Size Adjustments"), this);
    QFormLayout* formLayout = new QFormLayout(sizeGroup);

    // Atom Scale Factor (0.1 - 3.0x)
    m_atomScaleSpinBox = new QDoubleSpinBox(this);
    m_atomScaleSpinBox->setRange(0.1, 3.0);
    m_atomScaleSpinBox->setValue(1.0);
    m_atomScaleSpinBox->setSingleStep(0.1);
    m_atomScaleSpinBox->setSuffix("x");
    connect(m_atomScaleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &VisualizationSettingsDialog::onAtomScaleChanged);
    formLayout->addRow(tr("Atom Size:"), m_atomScaleSpinBox);

    // Bond Thickness (0.05 - 0.5)
    m_bondThicknessSpinBox = new QDoubleSpinBox(this);
    m_bondThicknessSpinBox->setRange(0.05, 0.5);
    m_bondThicknessSpinBox->setValue(0.15);
    m_bondThicknessSpinBox->setSingleStep(0.05);
    m_bondThicknessSpinBox->setDecimals(2);
    connect(m_bondThicknessSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &VisualizationSettingsDialog::onBondThicknessChanged);
    formLayout->addRow(tr("Bond Thickness:"), m_bondThicknessSpinBox);

    mainLayout->addWidget(sizeGroup);
}

// Claude Generated - Appearance Group for Fog Effects
void VisualizationSettingsDialog::createAppearanceGroup(QVBoxLayout* mainLayout)
{
    QGroupBox* appearanceGroup = new QGroupBox(tr("Appearance"), this);
    QFormLayout* formLayout = new QFormLayout(appearanceGroup);

    // Fog enabled checkbox
    m_fogEnabledCheckBox = new QCheckBox(this);
    m_fogEnabledCheckBox->setChecked(false);
    connect(m_fogEnabledCheckBox, &QCheckBox::toggled,
            this, &VisualizationSettingsDialog::onFogEnabledChanged);
    formLayout->addRow(tr("Enable Fog:"), m_fogEnabledCheckBox);

    // Fog intensity slider (0-100)
    QHBoxLayout* fogIntensityLayout = new QHBoxLayout();
    m_fogIntensitySlider = new QSlider(Qt::Horizontal, this);
    m_fogIntensitySlider->setRange(0, 100);
    m_fogIntensitySlider->setValue(50);
    m_fogIntensitySlider->setEnabled(false);  // Disabled until fog is enabled
    m_fogIntensityLabel = new QLabel("50%", this);
    m_fogIntensityLabel->setMinimumWidth(50);
    fogIntensityLayout->addWidget(m_fogIntensitySlider);
    fogIntensityLayout->addWidget(m_fogIntensityLabel);
    connect(m_fogIntensitySlider, &QSlider::valueChanged,
            this, &VisualizationSettingsDialog::onFogIntensityChanged);
    formLayout->addRow(tr("Fog Intensity:"), fogIntensityLayout);

    mainLayout->addWidget(appearanceGroup);
}

void VisualizationSettingsDialog::loadCurrentSettings()
{
    if (!m_viewer) return;

    // Claude Generated - Load from saved settings if available
    if (m_settings) {
        Settings::VisualizationSettings saved = m_settings->getVisualizationSettings();

        // Load rendering mode
        int modeIndex = m_renderingModeCombo->findData(saved.renderingMode);
        if (modeIndex >= 0) {
            m_renderingModeCombo->setCurrentIndex(modeIndex);
        }

        // Load color scheme
        int schemeIndex = m_colorSchemeCombo->findData(saved.colorScheme);
        if (schemeIndex >= 0) {
            m_colorSchemeCombo->setCurrentIndex(schemeIndex);
        }

        // Load transparency (0.0-1.0 -> 0-100)
        int transparencyPercent = static_cast<int>(saved.atomTransparency * 100);
        m_transparencySlider->setValue(transparencyPercent);
        m_transparencyLabel->setText(QString("%1%").arg(transparencyPercent));

        // Load other settings
        m_shininessSpinBox->setValue(saved.atomShininess);
        m_atomScaleSpinBox->setValue(saved.atomScaleFactor);
        m_bondThicknessSpinBox->setValue(saved.bondThickness);

        // Claude Generated - Load fog settings
        m_fogEnabledCheckBox->setChecked(saved.fogEnabled);
        m_fogIntensitySlider->setValue(static_cast<int>(saved.fogIntensity * 100.0f));
        m_fogIntensityLabel->setText(QString("%1%").arg(static_cast<int>(saved.fogIntensity * 100.0f)));
        m_fogIntensitySlider->setEnabled(saved.fogEnabled);
    } else {
        // Fallback: Load from viewer (when no Settings available)
        int modeIndex = m_renderingModeCombo->findData(static_cast<int>(m_viewer->getRenderingMode()));
        if (modeIndex >= 0) {
            m_renderingModeCombo->setCurrentIndex(modeIndex);
        }

        int schemeIndex = m_colorSchemeCombo->findData(static_cast<int>(m_viewer->getColorScheme()));
        if (schemeIndex >= 0) {
            m_colorSchemeCombo->setCurrentIndex(schemeIndex);
        }

        int transparencyPercent = static_cast<int>(m_viewer->getAtomTransparency() * 100);
        m_transparencySlider->setValue(transparencyPercent);
        m_transparencyLabel->setText(QString("%1%").arg(transparencyPercent));

        m_shininessSpinBox->setValue(m_viewer->getAtomShininess());
        m_atomScaleSpinBox->setValue(m_viewer->getAtomScaleFactor());
        m_bondThicknessSpinBox->setValue(m_viewer->getBondThickness());

        // Load fog settings from viewer
        m_fogEnabledCheckBox->setChecked(m_viewer->getFogEnabled());
        m_fogIntensitySlider->setValue(static_cast<int>(m_viewer->getFogIntensity() * 100.0f));
        m_fogIntensityLabel->setText(QString("%1%").arg(static_cast<int>(m_viewer->getFogIntensity() * 100.0f)));
        m_fogIntensitySlider->setEnabled(m_viewer->getFogEnabled());
    }
}

void VisualizationSettingsDialog::onRenderingModeChanged(int index)
{
    if (!m_viewer) return;
    int modeValue = m_renderingModeCombo->itemData(index).toInt();
    m_viewer->setRenderingMode(static_cast<MoleculeViewer::RenderingMode>(modeValue));
}

void VisualizationSettingsDialog::onColorSchemeChanged(int index)
{
    if (!m_viewer) return;
    int schemeValue = m_colorSchemeCombo->itemData(index).toInt();
    m_viewer->setColorScheme(static_cast<MoleculeViewer::ColorScheme>(schemeValue));
}

void VisualizationSettingsDialog::onAtomTransparencyChanged(int value)
{
    if (!m_viewer) return;
    float alpha = value / 100.0f;
    m_transparencyLabel->setText(QString("%1%").arg(value));
    m_viewer->setAtomTransparency(alpha);
}

void VisualizationSettingsDialog::onAtomShininessChanged(double value)
{
    if (!m_viewer) return;
    m_viewer->setAtomShininess(static_cast<float>(value));
}

void VisualizationSettingsDialog::onAtomScaleChanged(double value)
{
    if (!m_viewer) return;
    m_viewer->setAtomScaleFactor(static_cast<float>(value));
}

void VisualizationSettingsDialog::onBondThicknessChanged(double value)
{
    if (!m_viewer) return;
    m_viewer->setBondThickness(static_cast<float>(value));
}

void VisualizationSettingsDialog::onResetDefaults()
{
    // Reset to default values
    m_renderingModeCombo->setCurrentIndex(0); // Ball and Stick
    m_colorSchemeCombo->setCurrentIndex(0);   // CPK
    m_transparencySlider->setValue(100);      // Fully opaque
    m_shininessSpinBox->setValue(80.0);       // Default shininess
    m_atomScaleSpinBox->setValue(1.0);        // 1x scale
    m_bondThicknessSpinBox->setValue(0.15);   // Default thickness

    // Claude Generated - Reset fog settings
    m_fogEnabledCheckBox->setChecked(false);  // Fog disabled by default
    m_fogIntensitySlider->setValue(50);       // 50% intensity
}

void VisualizationSettingsDialog::onApply()
{
    // Claude Generated - Save settings to persistent storage
    if (m_settings && m_viewer) {
        Settings::VisualizationSettings current;
        current.renderingMode = m_renderingModeCombo->currentData().toInt();
        current.colorScheme = m_colorSchemeCombo->currentData().toInt();
        current.atomTransparency = m_viewer->getAtomTransparency();
        current.atomShininess = m_viewer->getAtomShininess();
        current.atomScaleFactor = m_viewer->getAtomScaleFactor();
        current.bondThickness = m_viewer->getBondThickness();
        current.fogEnabled = m_viewer->getFogEnabled();
        current.fogIntensity = m_viewer->getFogIntensity();

        m_settings->setVisualizationSettings(current);
    }

    // Settings are applied in real-time, but this can trigger a refresh
    if (m_viewer) {
        m_viewer->update();
    }
}

void VisualizationSettingsDialog::onFogEnabledChanged(bool enabled)
{
    if (!m_viewer) return;

    m_viewer->setFogEnabled(enabled);
    m_fogIntensitySlider->setEnabled(enabled);

    if (enabled) {
        m_viewer->setFogIntensity(m_fogIntensitySlider->value() / 100.0f);
    }
}

void VisualizationSettingsDialog::onFogIntensityChanged(int value)
{
    if (!m_viewer) return;

    float intensity = value / 100.0f;
    m_fogIntensityLabel->setText(QString("%1%").arg(value));
    m_viewer->setFogIntensity(intensity);
}

// Claude Generated - Load saved settings on dialog show
void VisualizationSettingsDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    loadCurrentSettings();
}
