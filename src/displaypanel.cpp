// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// DisplayPanel — docked viewer display options. Ported from the former
// VisualizationSettingsDialog (wiring/presets/persistence preserved). Claude Generated 2026.
#include "displaypanel.h"

#include "widgets/collapsiblesection.h"

#include "ncianalysis.h"

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
    syncFromViewer();
    refreshPresetList();

    // Claude Generated 2026 - re-sync controls after a view preset is applied
    // (camera+display) without the dock being raised.
    if (m_viewer)
        connect(m_viewer, &MoleculeViewer::viewPresetApplied,
                this, [this]() { syncFromViewer(); });
}

namespace {
// Claude Generated 2026 - Paint a colour button so the button itself is the swatch.
// Used by both colour selectors (bead types, interaction classes).
void applySwatch(QPushButton* button, const QColor& color)
{
    if (!button)
        return;
    const QString text = color.isValid() ? color.name(QColor::HexRgb) : QString();
    button->setText(text);
    if (!color.isValid()) {
        button->setStyleSheet(QString());
        return;
    }
    // Readable label on both light and dark swatches.
    const bool dark = color.lightness() < 128;
    button->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                              .arg(color.name(QColor::HexRgb), dark ? "#ffffff" : "#000000"));
}

// Small colour square for a combo-box entry, so the whole palette is visible at a glance.
QIcon swatchIcon(const QColor& color)
{
    QPixmap pm(14, 14);
    pm.fill(color.isValid() ? color : QColor(Qt::transparent));
    return QIcon(pm);
}
} // namespace

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
        createFragmentGroup(l);
        createBeadTypeGroup(l);
        createMaterialGroup(l);
        createSizeGroup(l);
    }, true);
    addSection(tr("Effects"), [this](QVBoxLayout* l) { createAppearanceGroup(l); }, false);
    addSection(tr("Lighting"), [this](QVBoxLayout* l) { createLightingGroup(l); }, false);
    addSection(tr("Tools"), [this](QVBoxLayout* l) { createToolsGroup(l); createNciGroup(l); }, false);
    addSection(tr("Presets"), [this](QVBoxLayout* l) { createPresetsGroup(l); }, false);

    col->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    // Footer: live changes apply instantly; these manage defaults.
    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(4, 4, 4, 4);
    auto* resetBtn = new QPushButton(tr("Reset"), this);
    resetBtn->setToolTip(tr("Reset all display options to the built-in defaults"));
    connect(resetBtn, &QPushButton::clicked, this, &DisplayPanel::onResetDefaults);
    auto* loadBtn = new QPushButton(tr("Load Defaults"), this);
    loadBtn->setToolTip(tr("Apply the display options saved with \"Save as Default\""));
    connect(loadBtn, &QPushButton::clicked, this, &DisplayPanel::onLoadDefaults);
    auto* saveBtn = new QPushButton(tr("Save as Default"), this);
    saveBtn->setToolTip(tr("Remember the current display options for future launches"));
    connect(saveBtn, &QPushButton::clicked, this, &DisplayPanel::onSaveAsDefault);
    footer->addWidget(resetBtn);
    footer->addWidget(loadBtn);
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
    m_colorSchemeCombo->addItem(tr("By Type (CG beads)"), static_cast<int>(MoleculeViewer::ColorScheme::ByType));
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

    // Claude Generated 2026 - Per-atom overlay labels (element / bead type / index).
    auto* labelCombo = new QComboBox(this);
    labelCombo->addItem(tr("No labels"), int(MoleculeViewer::AtomLabel::None));
    labelCombo->addItem(tr("Element"), int(MoleculeViewer::AtomLabel::Element));
    labelCombo->addItem(tr("Type (bead)"), int(MoleculeViewer::AtomLabel::Type));
    labelCombo->addItem(tr("Index"), int(MoleculeViewer::AtomLabel::Index));
    labelCombo->setToolTip(tr("Draw a text label next to each atom. Element falls back "
                              "to the bead type for coarse-grained (VTF) atoms."));
    connect(labelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, labelCombo](int i) {
        if (m_viewer)
            m_viewer->setAtomLabelMode(static_cast<MoleculeViewer::AtomLabel>(labelCombo->itemData(i).toInt()));
    });
    f->addRow(tr("Labels:"), labelCombo);

    auto* labelSelOnly = new QCheckBox(tr("Label selected atoms only"), this);
    labelSelOnly->setToolTip(tr("Show labels only for selected atoms — clearer and faster "
                                "for large or coarse-grained systems."));
    connect(labelSelOnly, &QCheckBox::toggled, this, [this](bool on) {
        if (m_viewer) m_viewer->setLabelSelectionOnly(on);
    });
    f->addRow(QString(), labelSelOnly);

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

// Claude Generated 2026 - Fragment tinting for host-guest systems.
//
// A fragment is a connected component of the bond graph. The largest one keeps its
// plain colours (in a host-guest complex that is the host) and every other one is
// shifted toward a distinct hue, so a guest stands out without its elements
// becoming unrecognisable. The strength slider is the "wie deutlich" control; a
// fragment's hue can also be picked outright. The group hides itself when the
// structure has only one fragment - there is then nothing to distinguish.
void DisplayPanel::createFragmentGroup(QVBoxLayout* mainLayout)
{
    m_fragmentGroup = new QGroupBox(tr("Fragments (host-guest)"), this);
    auto* outer = new QVBoxLayout(m_fragmentGroup);

    m_fragmentTintCheck = new QCheckBox(tr("Tint fragments apart"), this);
    m_fragmentTintCheck->setToolTip(tr("Shift the colours of every fragment except the "
                                       "largest one, so separate molecules are told apart "
                                       "at a glance."));
    outer->addWidget(m_fragmentTintCheck);

    auto* pick = new QHBoxLayout;
    pick->setContentsMargins(0, 0, 0, 0);
    pick->addWidget(new QLabel(tr("Fragment:"), this));
    m_fragmentCombo = new QComboBox(this);
    m_fragmentCombo->setToolTip(tr("Fragments of the loaded structure, largest first, with "
                                   "their formula and atom count. Everything below applies "
                                   "to the fragment picked here."));
    pick->addWidget(m_fragmentCombo, 1);
    outer->addLayout(pick);

    // Everything below the combo edits exactly the fragment named in this box's
    // title. That naming is the whole point: a slider that silently applied to
    // "all guests" while a single fragment was selected above it read as ambiguous.
    m_fragmentSelectedGroup = new QGroupBox(this);
    QFormLayout* f = new QFormLayout(m_fragmentSelectedGroup);

    auto* colourRow = new QWidget(this);
    auto* colourLayout = new QHBoxLayout(colourRow);
    colourLayout->setContentsMargins(0, 0, 0, 0);
    m_fragmentColorButton = new QPushButton(this);
    m_fragmentColorButton->setMinimumWidth(80);
    m_fragmentColorButton->setToolTip(tr("Hue this fragment is shifted toward."));
    colourLayout->addWidget(m_fragmentColorButton, 1);
    // A picked hue cannot be un-picked through the colour dialog, so this fragment
    // needs its own way back to the automatic one - "Reset all" would also throw
    // away its tint strength and size.
    m_fragmentColorResetButton = new QPushButton(tr("Auto"), this);
    m_fragmentColorResetButton->setToolTip(tr("Back to the automatically derived hue for "
                                              "this fragment."));
    colourLayout->addWidget(m_fragmentColorResetButton);
    f->addRow(tr("Colour:"), colourRow);

    auto* strengthRow = new QWidget(this);
    auto* sh = new QHBoxLayout(strengthRow);
    sh->setContentsMargins(0, 0, 0, 0);
    m_fragmentStrengthSlider = new QSlider(Qt::Horizontal, this);
    m_fragmentStrengthSlider->setRange(0, 100);
    m_fragmentStrengthSlider->setValue(60);
    m_fragmentStrengthSlider->setToolTip(tr("How far this fragment's hues are rotated. Low keeps "
                                            "element colours almost intact, high makes it "
                                            "unmistakable."));
    m_fragmentStrengthLabel = new QLabel(QStringLiteral("60%"), this);
    m_fragmentStrengthLabel->setMinimumWidth(40);
    sh->addWidget(m_fragmentStrengthSlider, 1);
    sh->addWidget(m_fragmentStrengthLabel);
    f->addRow(tr("Tint:"), strengthRow);

    auto* scaleRow = new QWidget(this);
    auto* sc = new QHBoxLayout(scaleRow);
    sc->setContentsMargins(0, 0, 0, 0);
    m_fragmentScaleSlider = new QSlider(Qt::Horizontal, this);
    m_fragmentScaleSlider->setRange(20, 200);   // 0.2x .. 2.0x
    m_fragmentScaleSlider->setValue(100);
    m_fragmentScaleSlider->setToolTip(tr("Draw size of this fragment. Shrinking the host is how "
                                         "you look into its cavity; shrinking a guest keeps it "
                                         "from hiding the host."));
    m_fragmentScaleLabel = new QLabel(QStringLiteral("100%"), this);
    m_fragmentScaleLabel->setMinimumWidth(40);
    sc->addWidget(m_fragmentScaleSlider, 1);
    sc->addWidget(m_fragmentScaleLabel);
    f->addRow(tr("Size:"), scaleRow);

    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    auto* applyAllButton = new QPushButton(tr("Apply to all guests"), this);
    applyAllButton->setToolTip(tr("Give every fragment except the largest the tint strength "
                                  "and size set here."));
    auto* resetButton = new QPushButton(tr("Reset all"), this);
    resetButton->setToolTip(tr("Drop the custom hue, tint strength and size of every fragment."));
    buttons->addWidget(applyAllButton);
    buttons->addWidget(resetButton);
    buttons->addStretch();
    f->addRow(buttons);

    outer->addWidget(m_fragmentSelectedGroup);

    connect(m_fragmentTintCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_viewer)
            m_viewer->setFragmentTint(on, m_viewer->getFragmentTintStrength());
        refreshFragments();
    });

    connect(m_fragmentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int) { refreshSelectedFragment(); });

    connect(m_fragmentColorButton, &QPushButton::clicked, this, [this]() {
        if (!m_viewer || m_fragmentCombo->currentIndex() < 0)
            return;
        const int fragment = m_fragmentCombo->currentData().toInt();
        if (fragment <= 0)
            return;
        const QColor chosen = QColorDialog::getColor(m_viewer->getFragmentColor(fragment), this,
            tr("Hue for %1").arg(m_fragmentCombo->currentText()));
        if (!chosen.isValid())
            return;
        m_viewer->setFragmentColor(fragment, chosen);
        refreshFragments();
    });

    connect(m_fragmentColorResetButton, &QPushButton::clicked, this, [this]() {
        if (!m_viewer || m_fragmentCombo->currentIndex() < 0)
            return;
        // An invalid colour clears the override in the scene controller.
        m_viewer->setFragmentColor(m_fragmentCombo->currentData().toInt(), QColor());
        refreshFragments();
    });

    connect(m_fragmentStrengthSlider, &QSlider::valueChanged, this, [this](int v) {
        m_fragmentStrengthLabel->setText(QStringLiteral("%1%").arg(v));
        if (!m_viewer || m_fragmentCombo->currentIndex() < 0)
            return;
        m_viewer->setFragmentTintStrengthOverride(m_fragmentCombo->currentData().toInt(),
            v / 100.0f);
    });

    connect(m_fragmentScaleSlider, &QSlider::valueChanged, this, [this](int v) {
        m_fragmentScaleLabel->setText(QStringLiteral("%1%").arg(v));
        if (!m_viewer || m_fragmentCombo->currentIndex() < 0)
            return;
        m_viewer->setFragmentScaleOverride(m_fragmentCombo->currentData().toInt(), v / 100.0f);
    });

    connect(applyAllButton, &QPushButton::clicked, this, [this]() {
        if (!m_viewer)
            return;
        const float strength = m_fragmentStrengthSlider->value() / 100.0f;
        const float scale = m_fragmentScaleSlider->value() / 100.0f;
        const int count = m_viewer->getFragments().size();
        for (int fragment = 1; fragment < count; ++fragment) {
            m_viewer->setFragmentTintStrengthOverride(fragment, strength);
            m_viewer->setFragmentScaleOverride(fragment, scale);
        }
        refreshFragments();
    });

    connect(resetButton, &QPushButton::clicked, this, [this]() {
        if (!m_viewer)
            return;
        m_viewer->resetFragmentOverrides();
        refreshFragments();
    });

    mainLayout->addWidget(m_fragmentGroup);
    m_fragmentGroup->setVisible(false);

    if (m_viewer)
        connect(m_viewer, &MoleculeViewer::fragmentsChanged, this, &DisplayPanel::refreshFragments);
}

void DisplayPanel::refreshFragments()
{
    if (!m_viewer || !m_fragmentGroup)
        return;

    const QVector<QPair<QString, int>> fragments = m_viewer->getFragments();
    // One fragment means one molecule - nothing to tell apart.
    m_fragmentGroup->setVisible(fragments.size() > 1);
    if (fragments.size() < 2)
        return;

    const int previous = m_fragmentCombo->currentData().toInt();
    m_fragmentCombo->blockSignals(true);
    m_fragmentCombo->clear();
    for (int i = 0; i < fragments.size(); ++i) {
        const QString formula = fragments[i].first.isEmpty() ? tr("(no formula)") : fragments[i].first;
        const QString label = i == 0
            ? tr("1: %1  (%2 atoms, reference)").arg(formula).arg(fragments[i].second)
            : tr("%1: %2  (%3 atoms)").arg(i + 1).arg(formula).arg(fragments[i].second);
        m_fragmentCombo->addItem(swatchIcon(m_viewer->getFragmentColor(i)), label, i);
    }
    const int restore = m_fragmentCombo->findData(previous);
    // Default to the first tinted fragment: that is the one a user wants to adjust.
    m_fragmentCombo->setCurrentIndex(restore >= 0 ? restore : qMin(1, fragments.size() - 1));
    m_fragmentCombo->blockSignals(false);

    refreshSelectedFragment();
}

// Load the selected fragment's own values into the controls and say in the box
// title which fragment they belong to.
void DisplayPanel::refreshSelectedFragment()
{
    if (!m_viewer || !m_fragmentSelectedGroup || m_fragmentCombo->currentIndex() < 0)
        return;
    const int fragment = m_fragmentCombo->currentData().toInt();
    const bool isReference = fragment <= 0;

    m_fragmentSelectedGroup->setTitle(tr("Applies to: %1").arg(m_fragmentCombo->currentText()));

    applySwatch(m_fragmentColorButton, m_viewer->getFragmentColor(fragment));
    // The reference fragment is never tinted, so it has neither a hue nor a
    // strength - but it can still be resized, which is the "look inside" case.
    const bool tintable = !isReference && m_fragmentTintCheck->isChecked();
    m_fragmentColorButton->setEnabled(tintable);
    // Enabled only where there is something to undo, so the button also says
    // whether this fragment carries a picked hue at all.
    m_fragmentColorResetButton->setEnabled(tintable && m_viewer->hasFragmentColorOverride(fragment));
    m_fragmentStrengthSlider->setEnabled(!isReference && m_fragmentTintCheck->isChecked());

    const int strength = int(m_viewer->getFragmentTintStrengthFor(fragment) * 100);
    m_fragmentStrengthSlider->blockSignals(true);
    m_fragmentStrengthSlider->setValue(isReference ? 0 : strength);
    m_fragmentStrengthSlider->blockSignals(false);
    m_fragmentStrengthLabel->setText(QStringLiteral("%1%").arg(isReference ? 0 : strength));

    const int scale = int(m_viewer->getFragmentScaleFor(fragment) * 100);
    m_fragmentScaleSlider->blockSignals(true);
    m_fragmentScaleSlider->setValue(scale);
    m_fragmentScaleSlider->blockSignals(false);
    m_fragmentScaleLabel->setText(QStringLiteral("%1%").arg(scale));
}

// Claude Generated 2026 - Colours of coarse-grained bead types.
//
// VTF beads carry a type label instead of an element, and the "By Type" scheme
// derives a stable colour from that label. The selector is built from the loaded
// structure - it offers exactly the types present, with their bead counts - so it
// scales from the four types of a typical polymer file to whatever a file brings.
// The whole group hides itself for all-atom structures.
void DisplayPanel::createBeadTypeGroup(QVBoxLayout* mainLayout)
{
    m_beadTypeGroup = new QGroupBox(tr("Bead type colours"), this);
    QFormLayout* f = new QFormLayout(m_beadTypeGroup);

    m_beadInfoLabel = new QLabel(this);
    m_beadInfoLabel->setWordWrap(true);
    f->addRow(m_beadInfoLabel);

    auto* row = new QWidget(this);
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);

    m_beadTypeCombo = new QComboBox(this);
    m_beadTypeCombo->setToolTip(tr("Bead types found in the loaded structure, with their bead count."));
    h->addWidget(m_beadTypeCombo, 1);

    m_beadColorButton = new QPushButton(this);
    m_beadColorButton->setToolTip(tr("Pick the colour this bead type is drawn with."));
    m_beadColorButton->setMinimumWidth(80);
    h->addWidget(m_beadColorButton);

    auto* resetButton = new QPushButton(tr("Auto"), this);
    resetButton->setToolTip(tr("Drop all custom bead colours and go back to the automatically "
                               "derived ones."));
    h->addWidget(resetButton);
    f->addRow(tr("Type:"), row);

    m_beadSchemeHint = new QLabel(tr("These colours are drawn in the \"By Type\" colour scheme."), this);
    m_beadSchemeHint->setWordWrap(true);
    f->addRow(m_beadSchemeHint);

    connect(m_beadTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int i) {
            if (!m_viewer || i < 0)
                return;
            applySwatch(m_beadColorButton,
                m_viewer->getBeadTypeColor(m_beadTypeCombo->itemData(i).toString()));
        });

    connect(m_beadColorButton, &QPushButton::clicked, this, [this]() {
        if (!m_viewer || m_beadTypeCombo->currentIndex() < 0)
            return;
        const QString type = m_beadTypeCombo->currentData().toString();
        const QColor chosen = QColorDialog::getColor(m_viewer->getBeadTypeColor(type), this,
            tr("Colour for bead type \"%1\"").arg(type));
        if (!chosen.isValid())
            return;
        m_viewer->setBeadTypeColor(type, chosen);
        if (m_settings)
            m_settings->setBeadTypeColors(m_viewer->getBeadTypeColors());
        refreshBeadTypes();
    });

    connect(resetButton, &QPushButton::clicked, this, [this]() {
        if (!m_viewer)
            return;
        m_viewer->resetBeadTypeColors();
        if (m_settings)
            m_settings->setBeadTypeColors({});
        refreshBeadTypes();
    });

    mainLayout->addWidget(m_beadTypeGroup);
    m_beadTypeGroup->setVisible(false);   // shown once a structure brings bead types

    if (m_viewer) {
        connect(m_viewer, &MoleculeViewer::beadTypesChanged, this, &DisplayPanel::refreshBeadTypes);
        connect(m_viewer, &MoleculeViewer::colorSchemeChanged, this,
            [this]() { refreshBeadTypes(); });
    }
}

void DisplayPanel::refreshBeadTypes()
{
    if (!m_viewer || !m_beadTypeGroup)
        return;

    const QVector<QPair<QString, int>> types = m_viewer->getBeadTypes();
    m_beadTypeGroup->setVisible(!types.isEmpty());
    if (types.isEmpty())
        return;

    int beads = 0;
    for (const auto& t : types)
        beads += t.second;
    m_beadInfoLabel->setText(types.size() == 1
            ? tr("1 bead type, %1 beads").arg(beads)
            : tr("%1 bead types, %2 beads").arg(types.size()).arg(beads));

    const QString previous = m_beadTypeCombo->currentData().toString();
    m_beadTypeCombo->blockSignals(true);
    m_beadTypeCombo->clear();
    for (const auto& t : types) {
        const QColor c = m_viewer->getBeadTypeColor(t.first);
        m_beadTypeCombo->addItem(swatchIcon(c),
            tr("%1  (%2 beads)").arg(t.first).arg(t.second), t.first);
    }
    const int restore = m_beadTypeCombo->findData(previous);
    m_beadTypeCombo->setCurrentIndex(restore >= 0 ? restore : 0);
    m_beadTypeCombo->blockSignals(false);

    applySwatch(m_beadColorButton, m_viewer->getBeadTypeColor(m_beadTypeCombo->currentData().toString()));

    // A colour set here only shows up in the "By Type" scheme; say so rather than
    // letting the user wonder why nothing changed.
    m_beadSchemeHint->setVisible(
        m_viewer->getColorScheme() != MoleculeViewer::ColorScheme::ByType);
}

// Claude Generated 2026 - Non-covalent interaction overlay.
//
// Only the two hydrogen-bond numbers are exposed: they are the ones worth moving
// when looking at a structure. The halogen-bond and van-der-Waals fractions keep
// their literature defaults (see nci::detectGeometric) rather than adding a wall
// of spin boxes.
void DisplayPanel::createNciGroup(QVBoxLayout* mainLayout)
{
    QGroupBox* g = new QGroupBox(tr("Non-covalent interactions"), this);
    QFormLayout* f = new QFormLayout(g);

    m_nciSourceCombo = new QComboBox(this);
    m_nciSourceCombo->addItem(tr("Off"), 0);
    m_nciSourceCombo->addItem(tr("Geometry (distance/angle)"), 1);
    m_nciSourceCombo->addItem(tr("GFN-FF parameters"), 2);
    m_nciSourceCombo->addItem(tr("Population analysis (GFN2)"), 3);
    m_nciSourceCombo->setToolTip(tr(
        "Geometry evaluates distance and angle criteria on the displayed frame. "
        "GFN-FF reads the hydrogen- and halogen-bond terms of the force field, "
        "population analysis the charges of a GFN2 calculation."));
    connect(m_nciSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int i) {
            const int source = m_nciSourceCombo->itemData(i).toInt();
            if (m_viewer && source <= 1)
                m_viewer->setNciSource(source);
            emit nciSourceChanged(source);
        });
    f->addRow(tr("Overlay:"), m_nciSourceCombo);

    auto* kinds = new QWidget(this);
    auto* kindRow = new QHBoxLayout(kinds);
    kindRow->setContentsMargins(0, 0, 0, 0);
    m_nciHBondCheck = new QCheckBox(tr("H"), this);
    m_nciHBondCheck->setToolTip(tr("Hydrogen bonds D-H...A with D, A from N, O, F, S"));
    m_nciXBondCheck = new QCheckBox(tr("X"), this);
    m_nciXBondCheck->setToolTip(tr("Halogen bonds C-X...A with X = Cl, Br, I"));
    m_nciPiCheck = new QCheckBox(QString::fromUtf8("\xcf\x80"), this);
    m_nciPiCheck->setToolTip(tr("Pi stacking between planar five- and six-rings"));
    m_nciContactCheck = new QCheckBox(tr("vdW"), this);
    m_nciContactCheck->setToolTip(tr("Undirected close contacts below 0.9 times the sum of "
                                     "the van der Waals radii. Can produce many lines."));
    m_nciElectrostaticCheck = new QCheckBox(tr("q"), this);
    m_nciElectrostaticCheck->setToolTip(tr("GFN-FF source only: electrostatic atom pairs with "
                                           "their Coulomb pair energy, coloured by sign."));
    m_nciDispersionCheck = new QCheckBox(tr("disp"), this);
    m_nciDispersionCheck->setToolTip(tr("GFN-FF source only: dispersion atom pairs with their "
                                        "D4 pair energy."));
    for (QCheckBox* c : { m_nciHBondCheck, m_nciXBondCheck, m_nciPiCheck, m_nciContactCheck,
             m_nciElectrostaticCheck, m_nciDispersionCheck }) {
        kindRow->addWidget(c);
        connect(c, &QCheckBox::toggled, this, [this]() { applyNciOptions(); });
    }
    kindRow->addStretch();
    f->addRow(tr("Show:"), kinds);

    // The two pair terms exist only in the force-field parameter set.
    const auto updatePairKindState = [this]() {
        const bool gfnff = m_nciSourceCombo->currentData().toInt() == 2;
        m_nciElectrostaticCheck->setEnabled(gfnff);
        m_nciDispersionCheck->setEnabled(gfnff);
    };
    connect(m_nciSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [updatePairKindState](int) { updatePairKindState(); });
    updatePairKindState();

    auto* gate = new QWidget(this);
    auto* gateRow = new QHBoxLayout(gate);
    gateRow->setContentsMargins(0, 0, 0, 0);
    m_nciHbDistanceSpin = new QDoubleSpinBox(this);
    m_nciHbDistanceSpin->setRange(2.0, 3.5);
    m_nciHbDistanceSpin->setSingleStep(0.05);
    m_nciHbDistanceSpin->setDecimals(2);
    m_nciHbDistanceSpin->setSuffix(QString::fromUtf8(" \xc3\x85"));
    m_nciHbDistanceSpin->setToolTip(tr(
        "Maximum H...A distance. 2.50 A covers the strong and moderate bands and the "
        "top of the weak band (Jeffrey, An Introduction to Hydrogen Bonding, 1997)."));
    m_nciHbAngleSpin = new QSpinBox(this);
    m_nciHbAngleSpin->setRange(90, 180);
    m_nciHbAngleSpin->setSuffix(QString::fromUtf8(" \xc2\xb0"));
    m_nciHbAngleSpin->setToolTip(tr(
        "Minimum D-H...A angle. The IUPAC definition requires the angle to tend "
        "towards linearity (Arunan et al., Pure Appl. Chem. 2011, 83, 1637)."));
    gateRow->addWidget(m_nciHbDistanceSpin);
    gateRow->addWidget(m_nciHbAngleSpin);
    gateRow->addStretch();
    connect(m_nciHbDistanceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [this]() { applyNciOptions(); });
    connect(m_nciHbAngleSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [this]() { applyNciOptions(); });
    f->addRow(tr("H...A / angle:"), gate);

    // Colours per interaction class. Same selector shape as the bead types: pick a
    // class, pick its colour. The electrostatic term is listed twice because the
    // default palette splits it by sign (attractive vs repulsive).
    auto* colourRow = new QWidget(this);
    auto* colourLayout = new QHBoxLayout(colourRow);
    colourLayout->setContentsMargins(0, 0, 0, 0);

    m_nciKindCombo = new QComboBox(this);
    for (const auto& entry : nci::paletteEntries())
        m_nciKindCombo->addItem(entry.second, entry.first);
    m_nciKindCombo->setToolTip(tr("Interaction class whose overlay colour you want to change."));
    colourLayout->addWidget(m_nciKindCombo, 1);

    m_nciKindColorButton = new QPushButton(this);
    m_nciKindColorButton->setMinimumWidth(80);
    m_nciKindColorButton->setToolTip(tr("Colour of this interaction class in the 3D overlay "
                                        "and in the contact table."));
    colourLayout->addWidget(m_nciKindColorButton);

    auto* nciColourReset = new QPushButton(tr("Auto"), this);
    nciColourReset->setToolTip(tr("Drop all custom interaction colours."));
    colourLayout->addWidget(nciColourReset);
    f->addRow(tr("Colour:"), colourRow);

    connect(m_nciKindCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int i) {
            if (m_viewer && i >= 0)
                applySwatch(m_nciKindColorButton,
                    m_viewer->getNciKindColor(m_nciKindCombo->itemData(i).toInt()));
        });
    connect(m_nciKindColorButton, &QPushButton::clicked, this, [this]() {
        if (!m_viewer)
            return;
        const int key = m_nciKindCombo->currentData().toInt();
        const QColor chosen = QColorDialog::getColor(m_viewer->getNciKindColor(key), this,
            tr("Colour for %1").arg(m_nciKindCombo->currentText()));
        if (!chosen.isValid())
            return;
        m_viewer->setNciKindColor(key, chosen);
        if (m_settings)
            m_settings->setNciPalette(m_viewer->getNciPalette());
        refreshNciPalette();
    });
    connect(nciColourReset, &QPushButton::clicked, this, [this]() {
        if (!m_viewer)
            return;
        m_viewer->resetNciKindColors();
        if (m_settings)
            m_settings->setNciPalette({});
        refreshNciPalette();
    });

    m_nciLabelCheck = new QCheckBox(tr("Label contacts with the distance"), this);
    connect(m_nciLabelCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_viewer) m_viewer->setNciLabelsVisible(on);
    });
    f->addRow(QString(), m_nciLabelCheck);

    m_nciLiveMdCheck = new QCheckBox(tr("Live from GFN-FF during MD"), this);
    m_nciLiveMdCheck->setToolTip(tr(
        "Take the contact list from the running GFN-FF force field on every step "
        "instead of from the geometry. The force field then rebuilds its hydrogen- "
        "and halogen-bond lists every step, which costs simulation speed."));
    connect(m_nciLiveMdCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_viewer) m_viewer->setNciLiveMd(on);
        emit nciLiveMdChanged(on);
    });
    f->addRow(QString(), m_nciLiveMdCheck);

    mainLayout->addWidget(g);
}

void DisplayPanel::refreshNciPalette()
{
    if (!m_viewer || !m_nciKindCombo)
        return;
    for (int i = 0; i < m_nciKindCombo->count(); ++i) {
        const int key = m_nciKindCombo->itemData(i).toInt();
        m_nciKindCombo->setItemIcon(i, swatchIcon(m_viewer->getNciKindColor(key)));
    }
    applySwatch(m_nciKindColorButton,
        m_viewer->getNciKindColor(m_nciKindCombo->currentData().toInt()));
}

void DisplayPanel::applyNciOptions()
{
    if (!m_viewer)
        return;
    nci::Options o = m_viewer->getNciOptions();
    if (m_nciHBondCheck) o.hydrogenBonds = m_nciHBondCheck->isChecked();
    if (m_nciXBondCheck) o.halogenBonds = m_nciXBondCheck->isChecked();
    if (m_nciPiCheck) o.piStacking = m_nciPiCheck->isChecked();
    if (m_nciContactCheck) o.closeContacts = m_nciContactCheck->isChecked();
    if (m_nciElectrostaticCheck) o.electrostatics = m_nciElectrostaticCheck->isChecked();
    if (m_nciDispersionCheck) o.dispersion = m_nciDispersionCheck->isChecked();
    if (m_nciHbDistanceSpin) o.hbMaxDistance = float(m_nciHbDistanceSpin->value());
    if (m_nciHbAngleSpin) o.hbMinAngle = float(m_nciHbAngleSpin->value());
    m_viewer->setNciOptions(o);
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
// Sync all control values from the viewer (read-only; the viewer is the single
// source of truth for live display state). Claude Generated 2026.
// ---------------------------------------------------------------------------
void DisplayPanel::syncFromViewer()
{
    if (!m_viewer)
        return;

    const QWidget* all[] = { m_renderingModeCombo, m_colorSchemeCombo, m_transparencySlider,
        m_shininessSpinBox, m_atomScaleSpinBox, m_bondThicknessSpinBox, m_fogEnabledCheckBox,
        m_fogIntensitySlider, m_fogDistanceSlider, m_ssaoEnabledCheckBox, m_ssaoIntensitySlider,
        m_ssaoRadiusSpinBox, m_ssaoBiasSpinBox, m_bloomEnabledCheckBox, m_bloomThresholdSpinBox,
        m_bloomIntensitySlider, m_hdrEnabledCheckBox, m_exposureSpinBox, m_rotationModeCombo,
        m_instancingThresholdSpin, m_forceVectorsCheck, m_wallCheck, m_wallOpacitySlider, m_measureCheck, m_bondEditCombo,
        m_cornerLightButtons[0], m_cornerLightButtons[1], m_cornerLightButtons[2], m_cornerLightButtons[3],
        m_nciSourceCombo, m_nciHBondCheck, m_nciXBondCheck, m_nciPiCheck, m_nciContactCheck,
        m_nciHbDistanceSpin, m_nciHbAngleSpin, m_nciLabelCheck, m_nciLiveMdCheck,
        m_nciElectrostaticCheck, m_nciDispersionCheck, m_nciKindCombo, m_beadTypeCombo,
        m_fragmentTintCheck, m_fragmentStrengthSlider, m_fragmentCombo,
        m_fragmentScaleSlider };
    for (const QWidget* w : all)
        if (w) const_cast<QWidget*>(w)->blockSignals(true);

    auto setComboData = [](QComboBox* c, int data) { int i = c->findData(data); if (i >= 0) c->setCurrentIndex(i); };

    setComboData(m_renderingModeCombo, int(m_viewer->getRenderingMode()));
    setComboData(m_colorSchemeCombo, int(m_viewer->getColorScheme()));
    m_transparencySlider->setValue(int(m_viewer->getAtomTransparency() * 100));
    m_transparencyLabel->setText(QString("%1%").arg(int(m_viewer->getAtomTransparency() * 100)));
    m_shininessSpinBox->setValue(m_viewer->getAtomShininess());
    m_atomScaleSpinBox->setValue(m_viewer->getAtomScaleFactor());
    m_bondThicknessSpinBox->setValue(m_viewer->getBondThickness());
    m_fogEnabledCheckBox->setChecked(m_viewer->getFogEnabled());
    m_fogIntensitySlider->setValue(int(m_viewer->getFogIntensity() * 100.0f));
    m_fogIntensityLabel->setText(QString("%1%").arg(int(m_viewer->getFogIntensity() * 100.0f)));

    const bool ssaoOn = m_viewer->getSSAOEnabled();
    m_ssaoEnabledCheckBox->setChecked(ssaoOn);
    m_ssaoIntensitySlider->setValue(int(m_viewer->getSSAOIntensity() * 100.0f));
    m_ssaoIntensityLabel->setText(QString::number(m_viewer->getSSAOIntensity(), 'f', 2));
    m_ssaoRadiusSpinBox->setValue(m_viewer->getSSAORadius());
    m_ssaoBiasSpinBox->setValue(m_viewer->getSSAOBias());
    m_ssaoIntensitySlider->setEnabled(ssaoOn);
    m_ssaoRadiusSpinBox->setEnabled(ssaoOn);
    m_ssaoBiasSpinBox->setEnabled(ssaoOn);
    const bool bloomOn = m_viewer->getBloomEnabled();
    m_bloomEnabledCheckBox->setChecked(bloomOn);
    m_bloomThresholdSpinBox->setValue(m_viewer->getBloomThreshold());
    m_bloomIntensitySlider->setValue(int(m_viewer->getBloomIntensity() * 100.0f));
    m_bloomIntensityLabel->setText(QString::number(m_viewer->getBloomIntensity(), 'f', 2));
    m_bloomThresholdSpinBox->setEnabled(bloomOn);
    m_bloomIntensitySlider->setEnabled(bloomOn);
    const bool hdrOn = m_viewer->getHDREnabled();
    m_hdrEnabledCheckBox->setChecked(hdrOn);
    m_exposureSpinBox->setValue(m_viewer->getExposure());
    m_exposureSpinBox->setEnabled(hdrOn);

    setComboData(m_rotationModeCombo, m_viewer->getRotationMode());
    m_instancingThresholdSpin->setValue(m_viewer->getInstancingThreshold());
    m_wallCheck->setChecked(m_viewer->getWallVisibleOverride());
    const qreal wallOpacity = m_viewer->getWallOpacity();
    m_wallOpacitySlider->setValue(int(wallOpacity * 100));
    m_wallOpacityLabel->setText(QString("%1%").arg(int(wallOpacity * 100)));

    const nci::Options o = m_viewer->getNciOptions();
    m_nciHBondCheck->setChecked(o.hydrogenBonds);
    m_nciXBondCheck->setChecked(o.halogenBonds);
    m_nciPiCheck->setChecked(o.piStacking);
    m_nciContactCheck->setChecked(o.closeContacts);
    m_nciElectrostaticCheck->setChecked(o.electrostatics);
    m_nciDispersionCheck->setChecked(o.dispersion);
    m_nciHbDistanceSpin->setValue(o.hbMaxDistance);
    m_nciHbAngleSpin->setValue(int(o.hbMinAngle));
    m_nciLabelCheck->setChecked(m_viewer->getNciLabelsVisible());
    m_nciLiveMdCheck->setChecked(m_viewer->getNciLiveMd());
    m_fragmentTintCheck->setChecked(m_viewer->getFragmentTint());
    setComboData(m_nciSourceCombo, m_viewer->getNciSource());
    const bool gfnff = m_viewer->getNciSource() == 2;
    m_nciElectrostaticCheck->setEnabled(gfnff);
    m_nciDispersionCheck->setEnabled(gfnff);

    refreshBeadTypes();
    refreshNciPalette();
    refreshFragments();

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
// Claude Generated 2026 - Reset/save/load work on the full DisplaySettings struct
// (viewer round-trip), so no field can be forgotten in a hand-maintained list.
void DisplayPanel::onResetDefaults()
{
    if (!m_viewer)
        return;
    m_viewer->applyDisplaySettings(DisplaySettings{});
    syncFromViewer();
}

void DisplayPanel::onSaveAsDefault()
{
    if (!m_settings || !m_viewer)
        return;
    // Read-modify-write: centerOnLoad persists on toggle and stays untouched here.
    Settings::VisualizationSettings c = m_settings->getVisualizationSettings();
    static_cast<DisplaySettings&>(c) = m_viewer->currentDisplaySettings();
    c.instancingThreshold = m_viewer->getInstancingThreshold();
    m_settings->setVisualizationSettings(c);
}

void DisplayPanel::onLoadDefaults()
{
    if (!m_settings || !m_viewer)
        return;
    m_viewer->applyDisplaySettings(m_settings->getVisualizationSettings());
    syncFromViewer();
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
    m_viewer->applyDisplaySettings(presets[index].settings);
    syncFromViewer();
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
    static_cast<DisplaySettings&>(c) = m_viewer->currentDisplaySettings();
    c.instancingThreshold = m_viewer->getInstancingThreshold();
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
