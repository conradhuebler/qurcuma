// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// SimulationDock implementation.
//
// Claude Generated 2026 - Dock system restructuring.

#include "simulationdock.h"

#include "modifiabletextedit.h"
#include "rmsdwidget.h"
#include "simulationcontrolwidget.h"
#include "snapshotswidget.h"
#include "view.h"
#include "widgets/collapsiblesection.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QVBoxLayout>

SimulationDock::SimulationDock(QWidget* parent)
    : QDockWidget(DockConfig::SimulationDockTitle, parent)
{
    setObjectName(DockConfig::SimulationDockObjectName);
    setupUI();
}

void SimulationDock::setupUI()
{
    m_tabs = new QTabWidget(this);
    m_tabs->setTabPosition(QTabWidget::North);
    m_tabs->setDocumentMode(true);

    m_simulationControlWidget = new SimulationControlWidget(this);
    // Claude Generated 2026 - The Simulation tab = controls + a collapsible "Show in viewer"
    // section that setViewOptions() fills (expand state persisted).
    auto* simPage = new QWidget;
    auto* simLayout = new QVBoxLayout(simPage);
    simLayout->setContentsMargins(0, 0, 0, 0);
    simLayout->setSpacing(2);
    simLayout->addWidget(m_simulationControlWidget, 1);
    m_viewOptionsSection = new CollapsibleSection(tr("Show in viewer"), simPage);
    m_viewOptionsSection->setVisible(false);  // until setViewOptions()
    const QString expandKey = QStringLiteral("ui/simulationDock/viewOptionsExpanded");
    m_viewOptionsSection->setExpanded(QSettings().value(expandKey, false).toBool());
    connect(m_viewOptionsSection, &CollapsibleSection::expandedChanged, this,
        [expandKey](bool on) { QSettings().setValue(expandKey, on); });
    simLayout->addWidget(m_viewOptionsSection);
    m_tabs->addTab(simPage, tr("Simulation"));

    m_snapshotsWidget = new SnapshotsWidget(this);
    m_tabs->addTab(m_snapshotsWidget, tr("Snapshots"));
    // Re-emit the snapshot widget's signals as dock signals (see header note).
    connect(m_snapshotsWidget, &SnapshotsWidget::takeSnapshotRequested,
            this, &SimulationDock::takeSnapshotRequested);
    connect(m_snapshotsWidget, &SnapshotsWidget::restoreSnapshotRequested,
            this, &SimulationDock::restoreSnapshotRequested);
    connect(m_snapshotsWidget, &SnapshotsWidget::deleteSnapshotRequested,
            this, &SimulationDock::deleteSnapshotRequested);

    m_rmsdWidget = new RMSDWidget(this);
    m_tabs->addTab(m_rmsdWidget,
        QIcon::fromTheme(QStringLiteral("view-object-histogram-linear")),
        tr("RMSD / Align"));

    QWidget* inputPage = new QWidget;
    QVBoxLayout* inputLayout = new QVBoxLayout(inputPage);
    inputLayout->setContentsMargins(4, 4, 4, 4);
    QHBoxLayout* inputFileLayout = new QHBoxLayout;
    inputFileLayout->addWidget(new QLabel(tr("Input file:")));
    m_inputFileEdit = new QLineEdit(tr("input"));
    m_inputFileEdit->setToolTip(tr("Base name for input file"));
    m_inputFileEditExtension = new QLineEdit;
    m_inputFileEditExtension->setMaximumWidth(60);
    inputFileLayout->addWidget(m_inputFileEdit);
    inputFileLayout->addWidget(m_inputFileEditExtension);
    inputLayout->addLayout(inputFileLayout);
    m_inputView = new ModifiableTextEdit;
    m_inputView->setPlaceholderText(tr("Input data"));
    inputLayout->addWidget(m_inputView);
    m_tabs->addTab(inputPage, tr("Input"));

    setWidget(m_tabs);
}

QTabWidget* SimulationDock::tabs() const { return m_tabs; }
SimulationControlWidget* SimulationDock::simulationControlWidget() const { return m_simulationControlWidget; }
SnapshotsWidget* SimulationDock::snapshotsWidget() const { return m_snapshotsWidget; }
RMSDWidget* SimulationDock::rmsdWidget() const { return m_rmsdWidget; }
ModifiableTextEdit* SimulationDock::inputView() const { return m_inputView; }
QLineEdit* SimulationDock::inputFileEdit() const { return m_inputFileEdit; }
QLineEdit* SimulationDock::inputFileEditExtension() const { return m_inputFileEditExtension; }

void SimulationDock::setViewOptions(QWidget* options)
{
    if (!m_viewOptionsSection || !options)
        return;
    auto* layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(options);
    m_viewOptionsSection->setContentLayout(layout);
    m_viewOptionsSection->setVisible(true);
}

// ---------------------------------------------------------------------------
// SimulationViewOptions (Claude Generated 2026 - UX stage 4)
// ---------------------------------------------------------------------------
SimulationViewOptions::SimulationViewOptions(MoleculeViewer* viewer, QWidget* parent)
    : QWidget(parent)
    , m_viewer(viewer)
{
    auto* f = new QFormLayout(this);
    f->setContentsMargins(4, 2, 4, 2);

    m_dynamicBondsCheck = new QCheckBox(tr("Dynamic bonds (reactions)"), this);
    m_dynamicBondsCheck->setToolTip(tr("Re-detect bonds from the geometry every simulation frame so "
        "bonds break and form as the structure reacts. Turn off to keep the initial topology fixed."));
    connect(m_dynamicBondsCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_viewer) m_viewer->setDynamicBonds(on);
    });
    f->addRow(QString(), m_dynamicBondsCheck);

    m_forceVectorsCheck = new QCheckBox(tr("Force vectors while grabbing"), this);
    m_forceVectorsCheck->setToolTip(tr("Draw the force applied to the grabbed atom and its shells."));
    connect(m_forceVectorsCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_viewer) m_viewer->setForceVectorsVisible(on);
    });
    f->addRow(QString(), m_forceVectorsCheck);

    // The wall geometry comes from Confinement Walls above; this only shows or hides it.
    m_wallCheck = new QCheckBox(tr("Confinement walls"), this);
    m_wallCheck->setToolTip(tr("Show/hide the confinement-wall wireframe. The wall geometry "
        "and activation come from Confinement Walls above; this only toggles the drawing."));
    connect(m_wallCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_viewer) m_viewer->setWallVisibleOverride(on);
    });
    f->addRow(QString(), m_wallCheck);

    auto* opacityRow = new QHBoxLayout;
    m_wallOpacitySlider = new QSlider(Qt::Horizontal, this);
    m_wallOpacitySlider->setRange(0, 100);
    m_wallOpacitySlider->setToolTip(tr("Transparency of the confinement-wall wireframe"));
    m_wallOpacityLabel = new QLabel(this);
    m_wallOpacityLabel->setMinimumWidth(40);
    opacityRow->addWidget(m_wallOpacitySlider);
    opacityRow->addWidget(m_wallOpacityLabel);
    connect(m_wallOpacitySlider, &QSlider::valueChanged, this, [this](int v) {
        m_wallOpacityLabel->setText(QStringLiteral("%1%").arg(v));
        if (m_viewer) m_viewer->setWallOpacity(v / 100.0);
    });
    f->addRow(tr("Wall opacity:"), opacityRow);

    m_potGradientCheck = new QCheckBox(tr("Wall potential shells"), this);
    m_potGradientCheck->setToolTip(tr("Concentric iso-potential wireframe shells: blue/teal inside "
        "the boundary (approach zone), yellow/red outside (force zone). Spacing scales with "
        "1/beta for LogFermi walls."));
    connect(m_potGradientCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_viewer) m_viewer->setWallPotentialViz(on);
    });
    f->addRow(QString(), m_potGradientCheck);

    auto* arrowRow = new QHBoxLayout;
    m_potArrowCheck = new QCheckBox(tr("Wall force field"), this);
    m_potArrowCheck->setToolTip(tr("Force arrows on a grid around the wall. Length = force "
        "magnitude, colour = distance level; LogFermi walls also push inside."));
    m_potArrowResSpin = new QSpinBox(this);
    m_potArrowResSpin->setRange(2, 8);
    m_potArrowResSpin->setValue(4);
    m_potArrowResSpin->setToolTip(tr("Sample points per axis (box face) or per latitude ring (sphere)."));
    arrowRow->addWidget(m_potArrowCheck);
    arrowRow->addWidget(new QLabel(tr("Res:"), this));
    arrowRow->addWidget(m_potArrowResSpin);
    arrowRow->addStretch();
    auto applyArrows = [this]() {
        if (m_viewer)
            m_viewer->setWallVectorField(m_potArrowCheck->isChecked(), m_potArrowResSpin->value());
    };
    connect(m_potArrowCheck, &QCheckBox::toggled, this, [applyArrows](bool) { applyArrows(); });
    connect(m_potArrowResSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [applyArrows](int) { applyArrows(); });
    f->addRow(QString(), arrowRow);

    syncFromViewer();
}

void SimulationViewOptions::syncFromViewer()
{
    if (!m_viewer)
        return;
    const QSignalBlocker b1(m_dynamicBondsCheck), b2(m_forceVectorsCheck), b3(m_wallCheck),
        b4(m_wallOpacitySlider);
    m_dynamicBondsCheck->setChecked(m_viewer->dynamicBonds());
    m_forceVectorsCheck->setChecked(m_viewer->getForceVectorsVisible());
    m_wallCheck->setChecked(m_viewer->getWallVisibleOverride());
    const int opacity = int(m_viewer->getWallOpacity() * 100.0 + 0.5);
    m_wallOpacitySlider->setValue(opacity);
    m_wallOpacityLabel->setText(QStringLiteral("%1%").arg(opacity));
    // Wall potential shells and force field have no persisted state; they start off.
}

void SimulationViewOptions::showEvent(QShowEvent* event)
{
    syncFromViewer();
    QWidget::showEvent(event);
}

void SimulationDock::setCurrentTab(int index)
{
    if (m_tabs && index >= 0 && index < m_tabs->count())
        m_tabs->setCurrentIndex(index);
}
