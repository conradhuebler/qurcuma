// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// DisplayPanel — docked viewer display options. Ported from the former
// VisualizationSettingsDialog (wiring/presets/persistence preserved). Claude Generated 2026.
#include "displaypanel.h"

#include "widgets/collapsiblesection.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <functional>

DisplayPanel::DisplayPanel(MoleculeViewer* viewer, Settings* settings, QWidget* parent)
    : QWidget(parent)
    , m_viewer(viewer)
    , m_settings(settings)
{
    setupUI();
    if (m_settings)
        m_settings->initializeDefaultPresets();
    loadCurrentSettings();
    refreshPresetList();

    // Claude Generated 2026 - re-sync controls after a view preset is applied
    // (camera+display) without the dock being raised.
    if (m_viewer)
        connect(m_viewer, &MoleculeViewer::viewPresetApplied,
                this, [this]() { loadCurrentSettings(); });
}

void DisplayPanel::setupUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget;
    auto* col = new QVBoxLayout(content);
    col->setContentsMargins(4, 4, 4, 4);
    col->setSpacing(4);

    auto addSection = [&](const QString& title, std::function<void(QVBoxLayout*)> build,
                          bool expanded) {
        auto* sec = new CollapsibleSection(title, content);
        auto* lay = new QVBoxLayout;
        lay->setSpacing(4);
        build(lay);
        sec->setContentLayout(lay);
        sec->setExpanded(expanded);
        col->addWidget(sec);
        return sec;
    };

    addSection(tr("Style"), [this](QVBoxLayout* l) {
        createRenderingGroup(l);
        createMaterialGroup(l);
        createSizeGroup(l);
    }, true);
    addSection(tr("Effects"), [this](QVBoxLayout* l) { createAppearanceGroup(l); }, false);
    addSection(tr("Lighting"), [this](QVBoxLayout* l) { createLightingGroup(l); }, false);
    addSection(tr("Tools"), [this](QVBoxLayout* l) { createToolsGroup(l); }, false);
    addSection(tr("Presets"), [this](QVBoxLayout* l) { createPresetsGroup(l); }, false);

    col->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    // Footer: live changes apply instantly; these manage defaults.
    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(4, 4, 4, 4);
    auto* resetBtn = new QPushButton(tr("Reset"), this);
    resetBtn->setToolTip(tr("Reset display options to defaults"));
    connect(resetBtn, &QPushButton::clicked, this, &DisplayPanel::onResetDefaults);
    auto* saveBtn = new QPushButton(tr("Save as Default"), this);
    saveBtn->setToolTip(tr("Remember the current display options for future launches"));
    connect(saveBtn, &QPushButton::clicked, this, &DisplayPanel::onSaveAsDefault);
    footer->addWidget(resetBtn);
    footer->addStretch();
    footer->addWidget(saveBtn);
    root->addLayout(footer);
}

// ---------------------------------------------------------------------------
// Section builders (Rendering/Material/Size/Appearance reused from the dialog)
// ---------------------------------------------------------------------------
void DisplayPanel::createRenderingGroup(QVBoxLayout* mainLayout)
{
    QGroupBox* g = new QGroupBox(tr("Rendering"), this);
    QFormLayout* f = new QFormLayout(g);

    m_renderingModeCombo = new QComboBox(this);
    m_renderingModeCombo->addItem(tr("Ball and Stick"), static_cast<int>(MoleculeViewer::RenderingMode::BallAndStick));
    m_renderingModeCombo->addItem(tr("Space Filling"), static_cast<int>(MoleculeViewer::RenderingMode::SpaceFilling));
    m_renderingModeCombo->addItem(tr("Wireframe"), static_cast<int>(MoleculeViewer::RenderingMode::Wireframe));
    m_renderingModeCombo->addItem(tr("Sticks Only"), static_cast<int>(MoleculeViewer::RenderingMode::SticksOnly));
    connect(m_renderingModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &DisplayPanel::onRenderingModeChanged);
    f->addRow(tr("Mode:"), m_renderingModeCombo);

    m_colorSchemeCombo = new QComboBox(this);
    m_colorSchemeCombo->addItem(tr("CPK (Element Colors)"), static_cast<int>(MoleculeViewer::ColorScheme::CPK));
    m_colorSchemeCombo->addItem(tr("Monochrome"), static_cast<int>(MoleculeViewer::ColorScheme::Monochrome));
    m_colorSchemeCombo->addItem(tr("By Charge"), static_cast<int>(MoleculeViewer::ColorScheme::ByCharge));
    m_colorSchemeCombo->addItem(tr("Custom"), static_cast<int>(MoleculeViewer::ColorScheme::Custom));
    connect(m_colorSchemeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &DisplayPanel::onColorSchemeChanged);
    f->addRow(tr("Colors:"), m_colorSchemeCombo);

    mainLayout->addWidget(g);
}

void DisplayPanel::createMaterialGroup(QVBoxLayout* mainLayout)
{
    QGroupBox* g = new QGroupBox(tr("Material"), this);
    QFormLayout* f = new QFormLayout(g);

    QHBoxLayout* tl = new QHBoxLayout;
    m_transparencySlider = new QSlider(Qt::Horizontal, this);
    m_transparencySlider->setRange(0, 100);
    m_transparencySlider->setValue(100);
    m_transparencyLabel = new QLabel("100%", this);
    m_transparencyLabel->setMinimumWidth(40);
    tl->addWidget(m_transparencySlider);
    tl->addWidget(m_transparencyLabel);
    connect(m_transparencySlider, &QSlider::valueChanged, this, &DisplayPanel::onAtomTransparencyChanged);
    f->addRow(tr("Transparency:"), tl);

    m_shininessSpinBox = new QDoubleSpinBox(this);
    m_shininessSpinBox->setRange(0.0, 200.0);
    m_shininessSpinBox->setValue(80.0);
    m_shininessSpinBox->setSingleStep(5.0);
    connect(m_shininessSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &DisplayPanel::onAtomShininessChanged);
    f->addRow(tr("Shininess:"), m_shininessSpinBox);

    mainLayout->addWidget(g);
}

void DisplayPanel::createSizeGroup(QVBoxLayout* mainLayout)
{
    QGroupBox* g = new QGroupBox(tr("Size"), this);
    QFormLayout* f = new QFormLayout(g);

    m_atomScaleSpinBox = new QDoubleSpinBox(this);
    m_atomScaleSpinBox->setRange(0.1, 3.0);
    m_atomScaleSpinBox->setValue(1.0);
    m_atomScaleSpinBox->setSingleStep(0.1);
    m_atomScaleSpinBox->setSuffix("x");
    connect(m_atomScaleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &DisplayPanel::onAtomScaleChanged);
    f->addRow(tr("Atom Size:"), m_atomScaleSpinBox);

    m_bondThicknessSpinBox = new QDoubleSpinBox(this);
    m_bondThicknessSpinBox->setRange(0.05, 0.5);
    m_bondThicknessSpinBox->setValue(0.15);
    m_bondThicknessSpinBox->setSingleStep(0.05);
    m_bondThicknessSpinBox->setDecimals(2);
    connect(m_bondThicknessSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &DisplayPanel::onBondThicknessChanged);
    f->addRow(tr("Bond Thickness:"), m_bondThicknessSpinBox);

    mainLayout->addWidget(g);
}

void DisplayPanel::createAppearanceGroup(QVBoxLayout* mainLayout)
{
    QGroupBox* g = new QGroupBox(tr("Post-processing"), this);
    QFormLayout* f = new QFormLayout(g);

    // Fog
    m_fogEnabledCheckBox = new QCheckBox(this);
    connect(m_fogEnabledCheckBox, &QCheckBox::toggled, this, &DisplayPanel::onFogEnabledChanged);
    f->addRow(tr("Enable Fog:"), m_fogEnabledCheckBox);

    QHBoxLayout* fil = new QHBoxLayout;
    m_fogIntensitySlider = new QSlider(Qt::Horizontal, this);
    m_fogIntensitySlider->setRange(0, 100);
    m_fogIntensitySlider->setValue(70);
    m_fogIntensityLabel = new QLabel("70%", this);
    m_fogIntensityLabel->setMinimumWidth(40);
    fil->addWidget(m_fogIntensitySlider);
    fil->addWidget(m_fogIntensityLabel);
    connect(m_fogIntensitySlider, &QSlider::valueChanged, this, &DisplayPanel::onFogIntensityChanged);
    f->addRow(tr("Fog Strength:"), fil);

    m_fogDistanceSlider = new QSlider(Qt::Horizontal, this);
    m_fogDistanceSlider->setRange(0, 100);
    m_fogDistanceSlider->setValue(20);
    m_fogDistanceSlider->setToolTip(tr("How far before atoms start to fade"));
    connect(m_fogDistanceSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_viewer) m_viewer->setFogDistance(v / 100.0f);
        if (m_fogEnabledCheckBox && !m_fogEnabledCheckBox->isChecked())
            m_fogEnabledCheckBox->setChecked(true);
    });
    f->addRow(tr("Fog Distance:"), m_fogDistanceSlider);

    f->addRow(new QLabel(""));

    // SSAO
    m_ssaoEnabledCheckBox = new QCheckBox(this);
    m_ssaoEnabledCheckBox->setChecked(true);
    connect(m_ssaoEnabledCheckBox, &QCheckBox::toggled, this, &DisplayPanel::onSSAOEnabledChanged);
    f->addRow(tr("Enable SSAO:"), m_ssaoEnabledCheckBox);

    QHBoxLayout* sil = new QHBoxLayout;
    m_ssaoIntensitySlider = new QSlider(Qt::Horizontal, this);
    m_ssaoIntensitySlider->setRange(0, 200);
    m_ssaoIntensitySlider->setValue(100);
    m_ssaoIntensityLabel = new QLabel("1.0", this);
    m_ssaoIntensityLabel->setMinimumWidth(40);
    sil->addWidget(m_ssaoIntensitySlider);
    sil->addWidget(m_ssaoIntensityLabel);
    connect(m_ssaoIntensitySlider, &QSlider::valueChanged, this, &DisplayPanel::onSSAOIntensityChanged);
    f->addRow(tr("SSAO Intensity:"), sil);

    m_ssaoRadiusSpinBox = new QDoubleSpinBox(this);
    m_ssaoRadiusSpinBox->setRange(0.01, 0.2);
    m_ssaoRadiusSpinBox->setValue(0.05);
    m_ssaoRadiusSpinBox->setSingleStep(0.01);
    m_ssaoRadiusSpinBox->setDecimals(3);
    connect(m_ssaoRadiusSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &DisplayPanel::onSSAORadiusChanged);
    f->addRow(tr("SSAO Radius:"), m_ssaoRadiusSpinBox);

    m_ssaoBiasSpinBox = new QDoubleSpinBox(this);
    m_ssaoBiasSpinBox->setRange(0.0, 0.1);
    m_ssaoBiasSpinBox->setValue(0.025);
    m_ssaoBiasSpinBox->setSingleStep(0.005);
    m_ssaoBiasSpinBox->setDecimals(4);
    connect(m_ssaoBiasSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &DisplayPanel::onSSAOBiasChanged);
    f->addRow(tr("SSAO Bias:"), m_ssaoBiasSpinBox);

    f->addRow(new QLabel(""));

    // Bloom
    m_bloomEnabledCheckBox = new QCheckBox(this);
    m_bloomEnabledCheckBox->setChecked(true);
    connect(m_bloomEnabledCheckBox, &QCheckBox::toggled, this, &DisplayPanel::onBloomEnabledChanged);
    f->addRow(tr("Enable Bloom:"), m_bloomEnabledCheckBox);

    m_bloomThresholdSpinBox = new QDoubleSpinBox(this);
    m_bloomThresholdSpinBox->setRange(0.5, 1.5);
    m_bloomThresholdSpinBox->setValue(0.8);
    m_bloomThresholdSpinBox->setSingleStep(0.1);
    m_bloomThresholdSpinBox->setDecimals(2);
    connect(m_bloomThresholdSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &DisplayPanel::onBloomThresholdChanged);
    f->addRow(tr("Bloom Threshold:"), m_bloomThresholdSpinBox);

    QHBoxLayout* bil = new QHBoxLayout;
    m_bloomIntensitySlider = new QSlider(Qt::Horizontal, this);
    m_bloomIntensitySlider->setRange(0, 200);
    m_bloomIntensitySlider->setValue(100);
    m_bloomIntensityLabel = new QLabel("1.0", this);
    m_bloomIntensityLabel->setMinimumWidth(40);
    bil->addWidget(m_bloomIntensitySlider);
    bil->addWidget(m_bloomIntensityLabel);
    connect(m_bloomIntensitySlider, &QSlider::valueChanged, this, &DisplayPanel::onBloomIntensityChanged);
    f->addRow(tr("Bloom Intensity:"), bil);

    f->addRow(new QLabel(""));

    // HDR
    m_hdrEnabledCheckBox = new QCheckBox(this);
    m_hdrEnabledCheckBox->setChecked(true);
    connect(m_hdrEnabledCheckBox, &QCheckBox::toggled, this, &DisplayPanel::onHDREnabledChanged);
    f->addRow(tr("Enable HDR:"), m_hdrEnabledCheckBox);

    m_exposureSpinBox = new QDoubleSpinBox(this);
    m_exposureSpinBox->setRange(0.5, 3.0);
    m_exposureSpinBox->setValue(1.0);
    m_exposureSpinBox->setSingleStep(0.1);
    m_exposureSpinBox->setDecimals(2);
    connect(m_exposureSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, &DisplayPanel::onExposureChanged);
    f->addRow(tr("Exposure:"), m_exposureSpinBox);

    mainLayout->addWidget(g);
}

void DisplayPanel::createLightingGroup(QVBoxLayout* mainLayout)
{
    QGroupBox* g = new QGroupBox(tr("Lighting"), this);
    QVBoxLayout* v = new QVBoxLayout(g);

    QHBoxLayout* lightsRow = new QHBoxLayout;
    lightsRow->addWidget(new QLabel(tr("Corner lights:"), this));
    QWidget* grid = new QWidget(this);
    QGridLayout* gl = new QGridLayout(grid);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setSpacing(1);
    struct Spec { const char* label; const char* tip; int r; int c; };
    const Spec specs[4] = {
        { "◤", "Top-front-left light", 0, 0 }, { "◥", "Top-front-right light", 0, 1 },
        { "◣", "Top-back-left light", 1, 0 }, { "◢", "Top-back-right light", 1, 1 }
    };
    for (int i = 0; i < 4; ++i) {
        m_cornerLightButtons[i] = new QToolButton(this);
        m_cornerLightButtons[i]->setText(tr(specs[i].label));
        m_cornerLightButtons[i]->setToolTip(tr(specs[i].tip));
        m_cornerLightButtons[i]->setCheckable(true);
        m_cornerLightButtons[i]->setFixedSize(24, 24);
        connect(m_cornerLightButtons[i], &QToolButton::toggled, this, [this, i](bool on) {
            if (m_viewer) m_viewer->setCornerLightEnabled(i, on);
        });
        gl->addWidget(m_cornerLightButtons[i], specs[i].r, specs[i].c);
    }
    lightsRow->addWidget(grid);
    lightsRow->addStretch();
    v->addLayout(lightsRow);

    m_bgColorButton = new QPushButton(tr("Background Color…"), this);
    connect(m_bgColorButton, &QPushButton::clicked, this, [this]() {
        if (!m_viewer) return;
        QColor c = QColorDialog::getColor(m_viewer->getBackgroundColor(), this, tr("Background Color"));
        if (c.isValid())
            m_viewer->setBackgroundColor(c);
    });
    v->addWidget(m_bgColorButton);

    mainLayout->addWidget(g);
}

void DisplayPanel::createToolsGroup(QVBoxLayout* mainLayout)
{
    QGroupBox* g = new QGroupBox(tr("Tools"), this);
    QFormLayout* f = new QFormLayout(g);

    m_measureCheck = new QCheckBox(tr("on — click atoms (2=dist, 3=angle, 4=dihedral)"), this);
    m_measureCheck->setToolTip(tr("Type is auto-detected from the number of picked atoms. "
                                  "Click a marked atom again to deselect; Esc clears."));
    connect(m_measureCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_viewer) m_viewer->setMeasurementMode(on ? 1 : 0);
    });
    // Stay in sync with the viewer-bar measurement toggle.
    if (m_viewer)
        connect(m_viewer, &MoleculeViewer::measurementModeChanged, m_measureCheck, [this](int mode) {
            const bool on = (mode != 0);
            if (m_measureCheck->isChecked() != on) {
                m_measureCheck->blockSignals(true);
                m_measureCheck->setChecked(on);
                m_measureCheck->blockSignals(false);
            }
        });
    f->addRow(tr("Measure:"), m_measureCheck);

    m_bondEditCombo = new QComboBox(this);
    m_bondEditCombo->addItem(tr("No Bond Edit"), 0);
    m_bondEditCombo->addItem(tr("Add Bond"), 1);
    m_bondEditCombo->addItem(tr("Delete Bond"), 2);
    m_bondEditCombo->addItem(tr("Cycle Order"), 3);
    connect(m_bondEditCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
        if (m_viewer) m_viewer->setBondEditMode(m_bondEditCombo->itemData(i).toInt());
    });
    f->addRow(tr("Bond Edit:"), m_bondEditCombo);

    m_forceVectorsCheck = new QCheckBox(tr("Show force vectors while grabbing"), this);
    connect(m_forceVectorsCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_viewer) m_viewer->setForceVectorsVisible(on);
    });
    f->addRow(QString(), m_forceVectorsCheck);

    // Claude Generated 2026 - Dynamic bonds: re-detect the bond graph each live MD/Opt frame so
    // bond breaking/formation in reactions is drawn. Default on (matches MoleculeViewer).
    auto* dynamicBondsCheck = new QCheckBox(tr("Dynamic bonds (live MD/Opt reactions)"), this);
    dynamicBondsCheck->setToolTip(tr("Re-detect bonds from the geometry every simulation frame so "
        "bonds break and form as the structure reacts. Turn off to keep the initial topology fixed."));
    dynamicBondsCheck->setChecked(m_viewer ? m_viewer->dynamicBonds() : true);
    connect(dynamicBondsCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_viewer) m_viewer->setDynamicBonds(on);
    });
    f->addRow(QString(), dynamicBondsCheck);

    // Claude Generated 2026 - Auto-center on load: shift COM to origin when a file is opened.
    auto* centerOnLoadCheck = new QCheckBox(tr("Center molecule at origin on load"), this);
    centerOnLoadCheck->setToolTip(tr("When opening a file, translate all frames so the "
        "mass-weighted centre-of-mass is at the coordinate origin."));
    const bool currentCenterOnLoad = m_settings
        ? m_settings->getVisualizationSettings().centerOnLoad : true;
    centerOnLoadCheck->setChecked(currentCenterOnLoad);
    connect(centerOnLoadCheck, &QCheckBox::toggled, this, [this](bool on) {
        emit centerOnLoadChanged(on);
    });
    f->addRow(QString(), centerOnLoadCheck);

    // Claude Generated 2026 - Confinement-wall wireframe toggle. The wall geometry
    // itself is driven by the Simulation config (auto-show when walls are enabled);
    // this checkbox is an independent show/hide override for the wireframe.
    m_wallCheck = new QCheckBox(tr("Show confinement walls"), this);
    m_wallCheck->setToolTip(tr("Show/hide the harmonic confinement-wall wireframe. "
        "The wall geometry and activation come from the Simulation dock; this only "
        "toggles whether the box/sphere is drawn."));
    connect(m_wallCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_viewer) m_viewer->setWallVisibleOverride(on);
    });
    f->addRow(QString(), m_wallCheck);

    // Claude Generated 2026 - Variable wall-wireframe transparency. The RGB
    // (grey/red on violations) comes from the instance colour; this slider sets
    // the material alpha via MoleculeViewer::setWallOpacity.
    QHBoxLayout* wol = new QHBoxLayout;
    m_wallOpacitySlider = new QSlider(Qt::Horizontal, this);
    m_wallOpacitySlider->setRange(0, 100);
    m_wallOpacitySlider->setValue(60);
    m_wallOpacitySlider->setToolTip(tr("Transparency of the confinement-wall wireframe"));
    m_wallOpacityLabel = new QLabel("60%", this);
    m_wallOpacityLabel->setMinimumWidth(40);
    wol->addWidget(m_wallOpacitySlider);
    wol->addWidget(m_wallOpacityLabel);
    connect(m_wallOpacitySlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_wallOpacityLabel)
            m_wallOpacityLabel->setText(QString("%1%").arg(v));
        if (m_viewer)
            m_viewer->setWallOpacity(v / 100.0);
    });
    f->addRow(tr("Wall opacity:"), wol);

    // Iso-potential gradient shell overlay. 3 inside shells (blue->teal) +
    // 3 outside shells (yellow->red) at force-contour distances from the boundary.
    m_potGradientCheck = new QCheckBox(tr("Show potential gradient"), this);
    m_potGradientCheck->setToolTip(tr("Overlay concentric iso-potential wireframe shells:\n"
        "Blue/teal (inside boundary, approach zone), yellow/red (outside, force zone).\n"
        "Shell spacing scales with 1/beta for LogFermi walls."));
    m_potGradientCheck->setChecked(false);
    connect(m_potGradientCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_viewer) m_viewer->setWallPotentialViz(on);
        emit potGradientChanged(on);
    });
    f->addRow(QString(), m_potGradientCheck);

    // Wall force vector field: arrows sampled on a grid around the boundary.
    m_potArrowCheck = new QCheckBox(tr("Show force vectors"), this);
    m_potArrowCheck->setToolTip(tr("Draw force arrows at grid points around the wall boundary.\n"
        "Length = force magnitude; colour = distance level.\n"
        "LogFermi: also shows arrows inside (bell-shaped force profile)."));
    m_potArrowCheck->setChecked(false);
    QHBoxLayout* arrowResLay = new QHBoxLayout;
    m_potArrowResSpin = new QSpinBox(this);
    m_potArrowResSpin->setRange(2, 8);
    m_potArrowResSpin->setValue(4);
    m_potArrowResSpin->setToolTip(tr("Sample points per axis (box face) or per latitude ring (sphere)."));
    arrowResLay->addWidget(m_potArrowCheck);
    arrowResLay->addWidget(new QLabel(tr("Res:"), this));
    arrowResLay->addWidget(m_potArrowResSpin);
    arrowResLay->addStretch();
    auto emitArrows = [this]() {
        const bool on  = m_potArrowCheck   && m_potArrowCheck->isChecked();
        const int  res = m_potArrowResSpin ? m_potArrowResSpin->value() : 4;
        if (m_viewer) m_viewer->setWallVectorField(on, res);
        emit potVectorFieldChanged(on, res);
    };
    connect(m_potArrowCheck,   &QCheckBox::toggled,
            this, [emitArrows](bool) { emitArrows(); });
    connect(m_potArrowResSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [emitArrows](int)  { emitArrows(); });
    f->addRow(QString(), arrowResLay);

    f->addRow(new QLabel(""));

    m_rotationModeCombo = new QComboBox(this);
    m_rotationModeCombo->addItem(tr("Rotate molecule (camera fixed)"), static_cast<int>(MoleculeViewer::RotationMode::Model));
    m_rotationModeCombo->addItem(tr("Rotate camera (orbit)"), static_cast<int>(MoleculeViewer::RotationMode::CameraOrbit));
    connect(m_rotationModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &DisplayPanel::onRotationModeChanged);
    f->addRow(tr("Rotation:"), m_rotationModeCombo);

    m_instancingThresholdSpin = new QSpinBox(this);
    m_instancingThresholdSpin->setRange(1, 100000);
    m_instancingThresholdSpin->setSingleStep(100);
    m_instancingThresholdSpin->setValue(500);
    m_instancingThresholdSpin->setSuffix(tr(" atoms"));
    m_instancingThresholdSpin->setToolTip(tr("Informational on the Quick3D renderer (always instanced)."));
    connect(m_instancingThresholdSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &DisplayPanel::onInstancingThresholdChanged);
    f->addRow(tr("Instancing threshold:"), m_instancingThresholdSpin);

    mainLayout->addWidget(g);
}

void DisplayPanel::createPresetsGroup(QVBoxLayout* mainLayout)
{
    QGroupBox* quick = new QGroupBox(tr("Quick Presets"), this);
    QHBoxLayout* ql = new QHBoxLayout(quick);
    for (const char* name : { "Publication", "Analysis", "Presentation" }) {
        QString n = QString::fromLatin1(name);
        QPushButton* b = new QPushButton(tr(name), this);
        connect(b, &QPushButton::clicked, this, [this, n]() { loadQuickPreset(n); });
        ql->addWidget(b);
    }
    mainLayout->addWidget(quick);

    QGroupBox* custom = new QGroupBox(tr("Custom Presets"), this);
    QVBoxLayout* cl = new QVBoxLayout(custom);
    m_presetList = new QListWidget(this);
    m_presetList->setMaximumHeight(110);
    cl->addWidget(m_presetList);
    QHBoxLayout* bl = new QHBoxLayout;
    QPushButton* loadB = new QPushButton(tr("Load"), this);
    QPushButton* saveB = new QPushButton(tr("Save As…"), this);
    QPushButton* delB = new QPushButton(tr("Delete"), this);
    bl->addWidget(loadB);
    bl->addWidget(saveB);
    bl->addWidget(delB);
    cl->addLayout(bl);
    connect(loadB, &QPushButton::clicked, this, [this]() { onLoadPreset(m_presetList->currentRow()); });
    connect(saveB, &QPushButton::clicked, this, &DisplayPanel::onSavePreset);
    connect(delB, &QPushButton::clicked, this, &DisplayPanel::onDeletePreset);
    mainLayout->addWidget(custom);
}

// ---------------------------------------------------------------------------
// Load current values into controls (ported from the dialog, minus working-dir)
// ---------------------------------------------------------------------------
void DisplayPanel::loadCurrentSettings()
{
    if (!m_viewer)
        return;

    const QWidget* all[] = { m_renderingModeCombo, m_colorSchemeCombo, m_transparencySlider,
        m_shininessSpinBox, m_atomScaleSpinBox, m_bondThicknessSpinBox, m_fogEnabledCheckBox,
        m_fogIntensitySlider, m_fogDistanceSlider, m_ssaoEnabledCheckBox, m_ssaoIntensitySlider,
        m_ssaoRadiusSpinBox, m_ssaoBiasSpinBox, m_bloomEnabledCheckBox, m_bloomThresholdSpinBox,
        m_bloomIntensitySlider, m_hdrEnabledCheckBox, m_exposureSpinBox, m_rotationModeCombo,
        m_instancingThresholdSpin, m_forceVectorsCheck, m_wallCheck, m_wallOpacitySlider, m_measureCheck, m_bondEditCombo,
        m_cornerLightButtons[0], m_cornerLightButtons[1], m_cornerLightButtons[2], m_cornerLightButtons[3] };
    for (const QWidget* w : all)
        if (w) const_cast<QWidget*>(w)->blockSignals(true);

    auto setComboData = [](QComboBox* c, int data) { int i = c->findData(data); if (i >= 0) c->setCurrentIndex(i); };

    if (m_settings) {
        Settings::VisualizationSettings s = m_settings->getVisualizationSettings();
        setComboData(m_renderingModeCombo, s.renderingMode);
        setComboData(m_colorSchemeCombo, s.colorScheme);
        m_transparencySlider->setValue(int(s.atomTransparency * 100));
        m_transparencyLabel->setText(QString("%1%").arg(int(s.atomTransparency * 100)));
        m_shininessSpinBox->setValue(s.atomShininess);
        m_atomScaleSpinBox->setValue(s.atomScaleFactor);
        m_bondThicknessSpinBox->setValue(s.bondThickness);
        m_fogEnabledCheckBox->setChecked(s.fogEnabled);
        m_fogIntensitySlider->setValue(int(s.fogIntensity * 100.0f));
        m_fogIntensityLabel->setText(QString("%1%").arg(int(s.fogIntensity * 100.0f)));
        m_ssaoEnabledCheckBox->setChecked(s.ssaoEnabled);
        m_ssaoIntensitySlider->setValue(int(s.ssaoIntensity * 100.0f));
        m_ssaoIntensityLabel->setText(QString::number(s.ssaoIntensity, 'f', 2));
        m_ssaoRadiusSpinBox->setValue(s.ssaoRadius);
        m_ssaoBiasSpinBox->setValue(s.ssaoBias);
        m_bloomEnabledCheckBox->setChecked(s.bloomEnabled);
        m_bloomThresholdSpinBox->setValue(s.bloomThreshold);
        m_bloomIntensitySlider->setValue(int(s.bloomIntensity * 100.0f));
        m_bloomIntensityLabel->setText(QString::number(s.bloomIntensity, 'f', 2));
        m_hdrEnabledCheckBox->setChecked(s.hdrEnabled);
        m_exposureSpinBox->setValue(s.exposure);
        setComboData(m_rotationModeCombo, s.rotationMode);
        m_instancingThresholdSpin->setValue(s.instancingThreshold);
        m_wallCheck->setChecked(s.wallVisible);
        m_viewer->setWallVisibleOverride(s.wallVisible);  // apply persisted override
        m_wallOpacitySlider->setValue(int(s.wallOpacity * 100));
        m_wallOpacityLabel->setText(QString("%1%").arg(int(s.wallOpacity * 100)));
        m_viewer->setWallOpacity(s.wallOpacity);
    } else {
        setComboData(m_renderingModeCombo, int(m_viewer->getRenderingMode()));
        setComboData(m_colorSchemeCombo, int(m_viewer->getColorScheme()));
        m_transparencySlider->setValue(int(m_viewer->getAtomTransparency() * 100));
        m_shininessSpinBox->setValue(m_viewer->getAtomShininess());
        m_atomScaleSpinBox->setValue(m_viewer->getAtomScaleFactor());
        m_bondThicknessSpinBox->setValue(m_viewer->getBondThickness());
        m_fogEnabledCheckBox->setChecked(m_viewer->getFogEnabled());
        m_fogIntensitySlider->setValue(int(m_viewer->getFogIntensity() * 100.0f));
        setComboData(m_rotationModeCombo, m_viewer->getRotationMode());
        m_instancingThresholdSpin->setValue(m_viewer->getInstancingThreshold());
        m_wallCheck->setChecked(m_viewer->getWallVisibleOverride());
        const qreal curOpacity = m_viewer->getWallOpacity();
        m_wallOpacitySlider->setValue(int(curOpacity * 100));
        m_wallOpacityLabel->setText(QString("%1%").arg(int(curOpacity * 100)));
    }
    m_fogIntensitySlider->setEnabled(m_fogEnabledCheckBox->isChecked());
    m_fogDistanceSlider->setValue(int(m_viewer->getFogDistance() * 100.0f));
    m_forceVectorsCheck->setChecked(m_viewer->getForceVectorsVisible());
    m_measureCheck->setChecked(m_viewer->getMeasurementMode() != 0);
    for (int i = 0; i < 4; ++i)
        m_cornerLightButtons[i]->setChecked(m_viewer->isCornerLightEnabled(i));

    for (const QWidget* w : all)
        if (w) const_cast<QWidget*>(w)->blockSignals(false);
}

// ---------------------------------------------------------------------------
// Live-apply slots (ported verbatim)
// ---------------------------------------------------------------------------
void DisplayPanel::onRenderingModeChanged(int index)
{
    if (m_viewer) m_viewer->setRenderingMode(static_cast<MoleculeViewer::RenderingMode>(m_renderingModeCombo->itemData(index).toInt()));
}
void DisplayPanel::onColorSchemeChanged(int index)
{
    if (m_viewer) m_viewer->setColorScheme(static_cast<MoleculeViewer::ColorScheme>(m_colorSchemeCombo->itemData(index).toInt()));
}
void DisplayPanel::onAtomTransparencyChanged(int value)
{
    m_transparencyLabel->setText(QString("%1%").arg(value));
    if (m_viewer) m_viewer->setAtomTransparency(value / 100.0f);
}
void DisplayPanel::onAtomShininessChanged(double value) { if (m_viewer) m_viewer->setAtomShininess(float(value)); }
void DisplayPanel::onAtomScaleChanged(double value) { if (m_viewer) m_viewer->setAtomScaleFactor(float(value)); }
void DisplayPanel::onBondThicknessChanged(double value) { if (m_viewer) m_viewer->setBondThickness(float(value)); }

void DisplayPanel::onFogEnabledChanged(bool enabled)
{
    if (!m_viewer) return;
    m_viewer->setFogEnabled(enabled);
    m_fogIntensitySlider->setEnabled(enabled);
    if (enabled) m_viewer->setFogIntensity(m_fogIntensitySlider->value() / 100.0f);
}
void DisplayPanel::onFogIntensityChanged(int value)
{
    m_fogIntensityLabel->setText(QString("%1%").arg(value));
    if (m_viewer) m_viewer->setFogIntensity(value / 100.0f);
    if (!m_fogEnabledCheckBox->isChecked()) m_fogEnabledCheckBox->setChecked(true);
}
void DisplayPanel::onSSAOEnabledChanged(bool enabled)
{
    if (!m_viewer) return;
    m_viewer->setSSAOEnabled(enabled);
    m_ssaoIntensitySlider->setEnabled(enabled);
    m_ssaoRadiusSpinBox->setEnabled(enabled);
    m_ssaoBiasSpinBox->setEnabled(enabled);
}
void DisplayPanel::onSSAOIntensityChanged(int value)
{
    m_ssaoIntensityLabel->setText(QString::number(value / 100.0f, 'f', 2));
    if (m_viewer) m_viewer->setSSAOIntensity(value / 100.0f);
}
void DisplayPanel::onSSAORadiusChanged(double value) { if (m_viewer) m_viewer->setSSAORadius(float(value)); }
void DisplayPanel::onSSAOBiasChanged(double value) { if (m_viewer) m_viewer->setSSAOBias(float(value)); }
void DisplayPanel::onBloomEnabledChanged(bool enabled)
{
    if (!m_viewer) return;
    m_viewer->setBloomEnabled(enabled);
    m_bloomThresholdSpinBox->setEnabled(enabled);
    m_bloomIntensitySlider->setEnabled(enabled);
}
void DisplayPanel::onBloomThresholdChanged(double value) { if (m_viewer) m_viewer->setBloomThreshold(float(value)); }
void DisplayPanel::onBloomIntensityChanged(int value)
{
    m_bloomIntensityLabel->setText(QString::number(value / 100.0f, 'f', 2));
    if (m_viewer) m_viewer->setBloomIntensity(value / 100.0f);
}
void DisplayPanel::onHDREnabledChanged(bool enabled)
{
    if (!m_viewer) return;
    m_viewer->setHDREnabled(enabled);
    m_exposureSpinBox->setEnabled(enabled);
}
void DisplayPanel::onExposureChanged(double value) { if (m_viewer) m_viewer->setExposure(float(value)); }
void DisplayPanel::onRotationModeChanged(int index)
{
    if (m_viewer) m_viewer->setRotationMode(m_rotationModeCombo->itemData(index).toInt());
}
void DisplayPanel::onInstancingThresholdChanged(int value) { if (m_viewer) m_viewer->setInstancingThreshold(value); }

// ---------------------------------------------------------------------------
// Footer + presets
// ---------------------------------------------------------------------------
void DisplayPanel::onResetDefaults()
{
    m_renderingModeCombo->setCurrentIndex(0);
    m_colorSchemeCombo->setCurrentIndex(0);
    m_transparencySlider->setValue(100);
    m_shininessSpinBox->setValue(80.0);
    m_atomScaleSpinBox->setValue(1.0);
    m_bondThicknessSpinBox->setValue(0.15);
    m_fogEnabledCheckBox->setChecked(false);
    m_fogIntensitySlider->setValue(70);
    if (m_rotationModeCombo) m_rotationModeCombo->setCurrentIndex(0);
    if (m_instancingThresholdSpin) m_instancingThresholdSpin->setValue(500);
}

void DisplayPanel::onSaveAsDefault()
{
    if (!m_settings || !m_viewer)
        return;
    Settings::VisualizationSettings c;
    c.renderingMode = m_renderingModeCombo->currentData().toInt();
    c.colorScheme = m_colorSchemeCombo->currentData().toInt();
    c.atomTransparency = m_viewer->getAtomTransparency();
    c.atomShininess = m_viewer->getAtomShininess();
    c.atomScaleFactor = m_viewer->getAtomScaleFactor();
    c.bondThickness = m_viewer->getBondThickness();
    c.fogEnabled = m_viewer->getFogEnabled();
    c.fogIntensity = m_viewer->getFogIntensity();
    c.ssaoEnabled = m_viewer->getSSAOEnabled();
    c.ssaoIntensity = m_viewer->getSSAOIntensity();
    c.ssaoRadius = m_viewer->getSSAORadius();
    c.ssaoBias = m_viewer->getSSAOBias();
    c.bloomEnabled = m_viewer->getBloomEnabled();
    c.bloomThreshold = m_viewer->getBloomThreshold();
    c.bloomIntensity = m_viewer->getBloomIntensity();
    c.hdrEnabled = m_viewer->getHDREnabled();
    c.exposure = m_viewer->getExposure();
    c.rotationMode = m_viewer->getRotationMode();
    c.instancingThreshold = m_viewer->getInstancingThreshold();
    c.wallVisible = m_wallCheck->isChecked();
    c.wallOpacity = (m_wallOpacitySlider ? m_wallOpacitySlider->value() / 100.0 : 0.6);
    m_settings->setVisualizationSettings(c);
}

void DisplayPanel::refreshPresetList()
{
    if (!m_settings || !m_presetList)
        return;
    m_presetList->clear();
    for (const auto& preset : m_settings->getVisualizationPresets())
        m_presetList->addItem(preset.name);
}

void DisplayPanel::onLoadPreset(int index)
{
    if (!m_settings || index < 0 || !m_viewer)
        return;
    auto presets = m_settings->getVisualizationPresets();
    if (index >= presets.size())
        return;
    const auto& s = presets[index].settings;
    m_viewer->setRenderingMode(static_cast<MoleculeViewer::RenderingMode>(s.renderingMode));
    m_viewer->setColorScheme(static_cast<MoleculeViewer::ColorScheme>(s.colorScheme));
    m_viewer->setAtomTransparency(s.atomTransparency);
    m_viewer->setAtomShininess(s.atomShininess);
    m_viewer->setAtomScaleFactor(s.atomScaleFactor);
    m_viewer->setBondThickness(s.bondThickness);
    m_viewer->setFogEnabled(s.fogEnabled);
    m_viewer->setFogIntensity(s.fogIntensity);
    loadCurrentSettings();
}

void DisplayPanel::onSavePreset()
{
    if (!m_settings || !m_viewer)
        return;
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Save Preset"), tr("Preset name:"),
        QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty())
        return;
    Settings::VisualizationSettings c;
    c.renderingMode = m_renderingModeCombo->currentData().toInt();
    c.colorScheme = m_colorSchemeCombo->currentData().toInt();
    c.atomTransparency = m_viewer->getAtomTransparency();
    c.atomShininess = m_viewer->getAtomShininess();
    c.atomScaleFactor = m_viewer->getAtomScaleFactor();
    c.bondThickness = m_viewer->getBondThickness();
    c.fogEnabled = m_viewer->getFogEnabled();
    c.fogIntensity = m_viewer->getFogIntensity();
    m_settings->savePreset(name, c);
    refreshPresetList();
}

void DisplayPanel::onDeletePreset()
{
    if (!m_settings || !m_presetList || m_presetList->currentRow() < 0)
        return;
    QString name = m_presetList->currentItem()->text();
    if (name == "Publication" || name == "Analysis" || name == "Presentation") {
        QMessageBox::warning(this, tr("Cannot Delete"), tr("Built-in presets cannot be deleted."));
        return;
    }
    m_settings->deletePreset(name);
    refreshPresetList();
}

void DisplayPanel::loadQuickPreset(const QString& presetName)
{
    if (!m_settings || !m_viewer)
        return;
    auto presets = m_settings->getVisualizationPresets();
    for (int i = 0; i < presets.size(); ++i)
        if (presets[i].name == presetName) {
            onLoadPreset(i);
            return;
        }
}
