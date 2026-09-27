// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// CellDock implementation.
//
// Claude Generated 2026.

#include "celldock.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <cmath>

namespace {

/// Cell volume from the six parameters:
/// V = abc sqrt(1 - cos²α - cos²β - cos²γ + 2 cosα cosβ cosγ).
double cellVolume(const MoleculeFileLoader::CifInfo& info)
{
    const double d2r = M_PI / 180.0;
    const double ca = std::cos(info.alpha * d2r);
    const double cb = std::cos(info.beta * d2r);
    const double cg = std::cos(info.gamma * d2r);
    const double root = 1.0 - ca * ca - cb * cb - cg * cg + 2.0 * ca * cb * cg;
    return root > 0.0 ? info.a * info.b * info.c * std::sqrt(root) : 0.0;
}

QLabel* valueLabel(QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return label;
}

}  // namespace

CellDock::CellDock(QWidget* parent)
    : QDockWidget(DockConfig::CellDockTitle, parent)
{
    setObjectName(DockConfig::CellDockObjectName);
    setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);

    m_pages = new QStackedWidget(this);

    // --- page 0: nothing loaded -------------------------------------------
    auto* empty = new QLabel(tr("No CIF loaded.\n\nOpen a .cif file to see its unit cell, "
                                "choose a disorder conformation and repeat the cell."), m_pages);
    empty->setAlignment(Qt::AlignCenter);
    empty->setWordWrap(true);
    empty->setEnabled(false);
    m_pages->addWidget(empty);

    // --- page 1: a cif ----------------------------------------------------
    auto* page = new QWidget(m_pages);
    auto* column = new QVBoxLayout(page);

    m_fileLabel = new QLabel(page);
    m_fileLabel->setWordWrap(true);
    QFont bold = m_fileLabel->font();
    bold.setBold(true);
    m_fileLabel->setFont(bold);
    column->addWidget(m_fileLabel);

    // What is shown. Applied at once: both are cheap to build.
    auto* showBox = new QGroupBox(tr("Show"), page);
    auto* showColumn = new QVBoxLayout(showBox);
    m_showUnit = new QRadioButton(tr("Asymmetric unit (as in the file)"), showBox);
    m_showCellContent = new QRadioButton(tr("Unit cell (all symmetry images)"), showBox);
    auto* showGroup = new QButtonGroup(this);
    showGroup->addButton(m_showUnit);
    showGroup->addButton(m_showCellContent);
    showColumn->addWidget(m_showUnit);
    showColumn->addWidget(m_showCellContent);
    m_completeMolecules = new QCheckBox(tr("Complete molecules"), showBox);
    m_completeMolecules->setToolTip(tr("Put molecules cut by the cell faces back together, each "
                                       "with its centre inside the cell (as Mercury does)"));
    m_completeMolecules->setContentsMargins(20, 0, 0, 0);
    showColumn->addWidget(m_completeMolecules);
    connect(m_completeMolecules, &QCheckBox::toggled, this,
        [this](bool) { emit optionsRequested(currentOptions()); });
    // One of the two radios toggles on for every switch; answering that one
    // alone avoids reading the file twice.
    connect(m_showCellContent, &QRadioButton::toggled, this, [this](bool) {
        updateEnabled();
        emit optionsRequested(currentOptions());
    });
    column->addWidget(showBox);

    // Disorder: SHELX PART groups, e.g. the two conformations of a disordered
    // part. Only present when the file has them.
    m_disorderBox = new QGroupBox(tr("Disorder"), page);
    auto* disorderColumn = new QVBoxLayout(m_disorderBox);
    m_disorderCombo = new QComboBox(m_disorderBox);
    m_disorderCombo->setToolTip(tr("Which disorder group (SHELX PART) to build. The ordered "
                                   "atoms are always there; 'all' puts every alternative on "
                                   "top of each other, as the file lists them."));
    connect(m_disorderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int) { emit optionsRequested(currentOptions()); });
    disorderColumn->addWidget(m_disorderCombo);
    column->addWidget(m_disorderBox);

    auto* cellBox = new QGroupBox(tr("Unit cell"), page);
    auto* grid = new QGridLayout(cellBox);
    m_a = valueLabel(cellBox);
    m_b = valueLabel(cellBox);
    m_c = valueLabel(cellBox);
    m_alpha = valueLabel(cellBox);
    m_beta = valueLabel(cellBox);
    m_gamma = valueLabel(cellBox);
    m_volume = valueLabel(cellBox);
    // Columns: length label, value, gap, angle label, value, rest. Values sit
    // next to their labels; spare width goes to the gap and the end.
    const auto row = [grid, cellBox](int r, const QString& length, QLabel* lengthValue,
                                     const QString& angle, QLabel* angleValue) {
        grid->addWidget(new QLabel(length, cellBox), r, 0);
        grid->addWidget(lengthValue, r, 1);
        grid->addWidget(new QLabel(angle, cellBox), r, 3);
        grid->addWidget(angleValue, r, 4);
    };
    row(0, QStringLiteral("a"), m_a, QStringLiteral("α"), m_alpha);
    row(1, QStringLiteral("b"), m_b, QStringLiteral("β"), m_beta);
    row(2, QStringLiteral("c"), m_c, QStringLiteral("γ"), m_gamma);
    grid->addWidget(new QLabel(tr("Volume"), cellBox), 3, 0);
    grid->addWidget(m_volume, 3, 1, 1, 4);
    grid->setColumnMinimumWidth(2, 16);
    grid->setColumnStretch(2, 1);
    grid->setColumnStretch(5, 2);
    m_showCell = new QCheckBox(tr("Show cell in the viewer"), cellBox);
    m_showCell->setChecked(true);
    m_showCell->setToolTip(tr("Draw the unit cell: a red, b green, c blue from the origin; "
                              "with repeats also the outline of the whole block"));
    connect(m_showCell, &QCheckBox::toggled, this, &CellDock::cellShownChanged);
    grid->addWidget(m_showCell, 4, 0, 1, 6);
    column->addWidget(cellBox);

    auto* contentBox = new QGroupBox(tr("Contents"), page);
    auto* form = new QFormLayout(contentBox);
    m_spaceGroup = valueLabel(contentBox);
    m_formula = valueLabel(contentBox);
    m_formula->setWordWrap(true);
    m_sites = valueLabel(contentBox);
    m_operations = valueLabel(contentBox);
    m_unitAtoms = valueLabel(contentBox);
    m_cellAtoms = valueLabel(contentBox);
    form->addRow(tr("Space group"), m_spaceGroup);
    form->addRow(tr("Formula (Z)"), m_formula);
    form->addRow(tr("Sites in the file"), m_sites);
    form->addRow(tr("Symmetry operations"), m_operations);
    form->addRow(tr("Atoms, asymmetric unit"), m_unitAtoms);
    form->addRow(tr("Atoms, unit cell"), m_cellAtoms);
    column->addWidget(contentBox);

    m_repeatBox = new QGroupBox(tr("Repeats of the unit cell"), page);
    auto* repeatColumn = new QVBoxLayout(m_repeatBox);
    auto* spins = new QHBoxLayout;
    const auto makeSpin = [this, spins](const QString& axis) {
        spins->addWidget(new QLabel(axis, m_repeatBox));
        auto* spin = new QSpinBox(m_repeatBox);
        spin->setRange(1, 20);
        spin->setToolTip(tr("Copies of the unit cell along %1").arg(axis));
        spins->addWidget(spin);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &CellDock::updatePreview);
        return spin;
    };
    m_na = makeSpin(QStringLiteral("a"));
    m_nb = makeSpin(QStringLiteral("b"));
    m_nc = makeSpin(QStringLiteral("c"));
    spins->addStretch(1);
    repeatColumn->addLayout(spins);

    auto* applyRow = new QHBoxLayout;
    m_preview = new QLabel(m_repeatBox);
    applyRow->addWidget(m_preview, 1);
    m_apply = new QPushButton(tr("Apply"), m_repeatBox);
    m_apply->setToolTip(tr("Read the file again with these repeats"));
    connect(m_apply, &QPushButton::clicked, this,
        [this] { emit optionsRequested(currentOptions()); });
    applyRow->addWidget(m_apply);
    repeatColumn->addLayout(applyRow);
    column->addWidget(m_repeatBox);

    // Thermal ellipsoids from the displacement parameters, drawn in place of
    // the atom spheres; only offered when the file has any.
    m_ellipsoidBox = new QGroupBox(tr("Thermal ellipsoids"), page);
    auto* ellipsoidForm = new QFormLayout(m_ellipsoidBox);
    m_showEllipsoids = new QCheckBox(tr("Show displacement ellipsoids"), m_ellipsoidBox);
    m_showEllipsoids->setToolTip(tr("Draw each atom as its displacement ellipsoid (ORTEP style): "
                                    "anisotropic atoms from U_ij, isotropic ones (usually H) as "
                                    "spheres from U_iso."));
    m_probability = new QSpinBox(m_ellipsoidBox);
    m_probability->setRange(10, 99);
    m_probability->setValue(50);
    m_probability->setSuffix(QStringLiteral(" %"));
    m_probability->setToolTip(tr("Probability that the atom lies inside its ellipsoid; "
                                 "50 % is the usual choice for publication figures"));
    m_adpCounts = valueLabel(m_ellipsoidBox);
    ellipsoidForm->addRow(m_showEllipsoids);
    ellipsoidForm->addRow(tr("Probability"), m_probability);
    ellipsoidForm->addRow(tr("Parameters"), m_adpCounts);
    const auto emitEllipsoids = [this] {
        emit ellipsoidsChanged(m_showEllipsoids->isChecked(), m_probability->value());
    };
    connect(m_showEllipsoids, &QCheckBox::toggled, this, emitEllipsoids);
    connect(m_probability, QOverload<int>::of(&QSpinBox::valueChanged), this, emitEllipsoids);
    column->addWidget(m_ellipsoidBox);

    m_notes = new QLabel(page);
    m_notes->setWordWrap(true);
    m_notes->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_notes->setEnabled(false);
    column->addWidget(m_notes);
    column->addStretch(1);

    m_pages->addWidget(page);
    setWidget(m_pages);
    clearCif();
}

void CellDock::setCif(const QString& fileName, const MoleculeFileLoader::CifInfo& info)
{
    m_hasCif = true;
    m_info = info;

    m_fileLabel->setText(fileName);

    {
        const QSignalBlocker blockUnit(m_showUnit);
        const QSignalBlocker blockCell(m_showCellContent);
        const QSignalBlocker blockComplete(m_completeMolecules);
        (info.unitCell ? m_showCellContent : m_showUnit)->setChecked(true);
        m_completeMolecules->setChecked(info.completeMolecules);
    }
    m_spaceGroup->setText(info.spaceGroup.isEmpty() ? tr("not given")
            : info.spaceGroupNumber > 0 ? QStringLiteral("%1 (%2)").arg(info.spaceGroup).arg(info.spaceGroupNumber)
                                        : info.spaceGroup);
    m_formula->setText(info.formulaSum.isEmpty() ? tr("not given")
            : info.formulaUnitsZ > 0 ? QStringLiteral("%1 (Z = %2)").arg(info.formulaSum).arg(info.formulaUnitsZ)
                                     : info.formulaSum);

    // One entry per group, the major one marked, then "all". The data of an
    // entry is the group number, 0 for all.
    {
        const QSignalBlocker block(m_disorderCombo);
        m_disorderCombo->clear();
        double best = -1.0;
        int major = 0;
        for (const MoleculeFileLoader::CifDisorderGroup& group : info.disorderGroups) {
            if (group.meanOccupancy > best) {
                best = group.meanOccupancy;
                major = group.group;
            }
        }
        for (const MoleculeFileLoader::CifDisorderGroup& group : info.disorderGroups) {
            QString text = tr("Part %1 · %2 sites · occupancy %3")
                               .arg(group.group).arg(group.sites)
                               .arg(group.meanOccupancy, 0, 'f', 2);
            if (group.group == major)
                text += tr(" (major)");
            m_disorderCombo->addItem(text, group.group);
        }
        m_disorderCombo->addItem(tr("All parts (overlapping)"),
            MoleculeFileLoader::CifOptions::kAllGroups);
        const int index = m_disorderCombo->findData(info.shownGroup);
        m_disorderCombo->setCurrentIndex(index >= 0 ? index : m_disorderCombo->count() - 1);
    }
    m_disorderBox->setVisible(!info.disorderGroups.isEmpty());

    const auto length = [](double v) { return QStringLiteral("%1 Å").arg(v, 0, 'f', 4); };
    const auto angle = [](double v) { return QStringLiteral("%1°").arg(v, 0, 'f', 2); };
    if (info.hasCell) {
        m_a->setText(length(info.a));
        m_b->setText(length(info.b));
        m_c->setText(length(info.c));
        m_alpha->setText(angle(info.alpha));
        m_beta->setText(angle(info.beta));
        m_gamma->setText(angle(info.gamma));
        m_volume->setText(QStringLiteral("%1 Å³").arg(cellVolume(info), 0, 'f', 2));
    } else {
        for (QLabel* label : { m_a, m_b, m_c, m_alpha, m_beta, m_gamma })
            label->setText(QStringLiteral("–"));
        m_volume->setText(tr("no cell in the file"));
    }
    m_sites->setText(QString::number(info.asymmetricAtoms));
    m_operations->setText(info.symmetryOperations == 1
            ? tr("1 (P1)") : QString::number(info.symmetryOperations));
    m_unitAtoms->setText(QString::number(info.unitAtoms));
    m_cellAtoms->setText(QString::number(info.cellAtoms));

    {
        const QSignalBlocker blockA(m_na);
        const QSignalBlocker blockB(m_nb);
        const QSignalBlocker blockC(m_nc);
        m_na->setValue(info.na);
        m_nb->setValue(info.nb);
        m_nc->setValue(info.nc);
    }
    updateEnabled();
    updatePreview();

    const int adpSites = info.anisotropicSites + info.isotropicSites;
    m_ellipsoidBox->setVisible(adpSites > 0);
    m_adpCounts->setText(tr("%1 anisotropic, %2 isotropic").arg(info.anisotropicSites)
                             .arg(info.isotropicSites));

    m_notes->setText(info.notes.join(QLatin1Char('\n')));
    m_notes->setVisible(!info.notes.isEmpty());
    m_pages->setCurrentIndex(1);
}

void CellDock::setCellShown(bool shown)
{
    const QSignalBlocker block(m_showCell);
    m_showCell->setChecked(shown);
}

void CellDock::setEllipsoidSettings(bool shown, int percent)
{
    const QSignalBlocker blockBox(m_showEllipsoids);
    const QSignalBlocker blockSpin(m_probability);
    m_showEllipsoids->setChecked(shown);
    m_probability->setValue(percent);
}

void CellDock::clearCif()
{
    m_hasCif = false;
    m_pages->setCurrentIndex(0);
}

MoleculeFileLoader::CifOptions CellDock::currentOptions() const
{
    MoleculeFileLoader::CifOptions options;
    options.unitCell = m_showCellContent->isChecked();
    options.completeMolecules = options.unitCell && m_completeMolecules->isChecked();
    if (options.unitCell) {
        options.na = m_na->value();
        options.nb = m_nb->value();
        options.nc = m_nc->value();
    }
    options.disorderGroup = m_info.disorderGroups.isEmpty()
        ? MoleculeFileLoader::CifOptions::kAllGroups
        : m_disorderCombo->currentData().toInt();
    return options;
}

void CellDock::updateEnabled()
{
    // Repeats replicate the unit cell; the asymmetric unit has none to repeat,
    // and a file without a cell has neither.
    m_showCellContent->setEnabled(m_info.hasCell);
    m_completeMolecules->setEnabled(m_info.hasCell && m_showCellContent->isChecked());
    m_repeatBox->setEnabled(m_info.hasCell && m_showCellContent->isChecked());
    m_showCell->setEnabled(m_info.hasCell);
}

void CellDock::updatePreview()
{
    if (!m_showCellContent->isChecked()) {
        m_preview->setText(tr("%1 atoms shown").arg(m_info.unitAtoms));
        m_apply->setEnabled(false);
        return;
    }
    const int applied = m_info.cellAtoms * m_info.na * m_info.nb * m_info.nc;
    const int total = m_info.cellAtoms * m_na->value() * m_nb->value() * m_nc->value();
    const bool changed = m_na->value() != m_info.na || m_nb->value() != m_info.nb
        || m_nc->value() != m_info.nc;
    m_preview->setText(changed ? tr("→ %1 atoms (shown: %2)").arg(total).arg(applied)
                               : tr("%1 atoms shown").arg(total));
    m_apply->setEnabled(changed);
}
