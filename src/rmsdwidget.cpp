// rmsdwidget.cpp
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - RMSD / align / reorder workspace (SimulationDock "RMSD /
// Align" tab). A QTableWidget of structures: one is the reference (radio button,
// drawn as the primary molecule), the others are aligned to it and drawn as tinted
// overlays. Per structure the table shows the plain + permutation RMSD and offers a
// colour tint, a size and a visibility toggle, plus removal. Backed by curcuma's
// RMSDDriver; decoupled from the viewer via signals (MainWindow drives it).
#include "rmsdwidget.h"

#include "moleculebridge.h"
#include "mol2parser.h"
#include "pdbparser.h"
#include "vtfparser.h"
#include "xyzparser.h"

#include <src/capabilities/rmsd.h>
#include <src/capabilities/rmsd/rmsd_functions.h>

#include "external/json.hpp"
using json = nlohmann::json;

#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <vector>

namespace {
// Column order of the structures table.
enum Column {
    ColRef = 0,    // reference radio button
    ColShow,       // visibility checkbox
    ColName,       // structure name (reference marked)
    ColRmsd,       // plain RMSD (before reorder)
    ColRmsdPerm,   // permutation RMSD (after reorder)
    ColColor,      // colour-tint swatch (overlays only)
    ColSize,       // size spinbox (overlays only)
    ColRemove,     // remove button
    ColCount
};
}

RMSDWidget::RMSDWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    updateButtons();
}

void RMSDWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // --- Alignment options (how structures are aligned) ---
    auto* optBox = new QGroupBox(tr("Alignment options"), this);
    auto* optLayout = new QFormLayout(optBox);

    m_methodCombo = new QComboBox(this);
    // label -> RMSDDriver method string (see curcuma rmsd.cpp method_map). Inertia is the
    // default permutation method (template-free, fast).
    m_methodCombo->addItem(tr("Inertia (recommended, template-free)"), QStringLiteral("inertia"));
    m_methodCombo->addItem(tr("Subspace"), QStringLiteral("subspace"));
    m_methodCombo->addItem(tr("Template"), QStringLiteral("template"));
    m_methodCombo->addItem(tr("Distance template"), QStringLiteral("dtemplates"));
    m_methodCombo->addItem(tr("Incremental (legacy)"), QStringLiteral("incr"));
    m_methodCombo->addItem(tr("MolAlign (external)"), QStringLiteral("molalign"));
    m_methodCombo->addItem(tr("Predefined order"), QStringLiteral("predefined"));
    optLayout->addRow(tr("Method (for reorder):"), m_methodCombo);

    m_protonsCheck = new QCheckBox(tr("Include hydrogens"), this);
    m_protonsCheck->setChecked(true);
    optLayout->addRow(QString(), m_protonsCheck);

    m_elementEdit = new QLineEdit(QStringLiteral("7"), this);
    m_elementEdit->setToolTip(
        tr("Element(s) for the template, e.g. \"7\" or \"7,8\" (template methods only)."));
    optLayout->addRow(tr("Template element(s):"), m_elementEdit);

    m_threadsSpin = new QSpinBox(this);
    m_threadsSpin->setRange(1, 64);
    m_threadsSpin->setValue(1);
    optLayout->addRow(tr("Threads:"), m_threadsSpin);
    mainLayout->addWidget(optBox);

    // --- Structures table ---
    m_table = new QTableWidget(0, ColCount, this);
    m_table->setHorizontalHeaderLabels({ tr("Ref"), tr("Show"), tr("Structure"),
        tr("RMSD [Å]"), tr("perm. [Å]"), tr("Color"), tr("Size"), QString() });
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setMinimumHeight(220);
    auto* hh = m_table->horizontalHeader();
    hh->setSectionResizeMode(ColName, QHeaderView::Stretch);
    for (int c : { int(ColRef), int(ColShow), int(ColRmsd), int(ColRmsdPerm),
             int(ColColor), int(ColSize), int(ColRemove) })
        hh->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    mainLayout->addWidget(m_table, 1);

    m_refGroup = new QButtonGroup(this);
    m_refGroup->setExclusive(true);
    connect(m_refGroup, &QButtonGroup::idClicked, this, [this](int id) {
        const int i = indexOfId(id);
        if (i >= 0)
            setReferenceByIndex(i);
    });

    // --- Workspace actions (single row under the table) ---
    // Reorder is off by default (permutation search is expensive); the user enables it
    // here right next to "Re-align all".
    auto* actionRow = new QHBoxLayout();
    m_addButton = new QPushButton(QIcon::fromTheme(QStringLiteral("document-open")),
        tr("Add structure…"), this);
    m_addButton->setToolTip(tr("Load a structure file and align it to the reference."));
    m_useReferenceButton = new QPushButton(QIcon::fromTheme(QStringLiteral("snap-orthogonal")),
        tr("Use current view as reference"), this);
    m_useReferenceButton->setToolTip(
        tr("Take the structure shown in the viewer as the alignment reference."));
    m_reorderCheck = new QCheckBox(tr("Reorder atoms"), this);
    m_reorderCheck->setChecked(false);
    m_reorderCheck->setToolTip(
        tr("Find the optimal atom mapping with the chosen method before computing the RMSD\n"
           "(permutation RMSD). Off (default): rigid alignment in the given atom order only —\n"
           "the permutation search is expensive."));
    m_realignButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")),
        tr("Re-align all"), this);
    m_realignButton->setToolTip(
        tr("Re-run the alignment for every structure with the current options."));
    actionRow->addWidget(m_addButton);
    actionRow->addWidget(m_useReferenceButton);
    actionRow->addStretch();
    actionRow->addWidget(m_reorderCheck);
    actionRow->addWidget(m_realignButton);
    mainLayout->addLayout(actionRow);

    // --- Reorder mapping for the selected structure ---
    m_reorderLabel = new QLabel(tr("Reorder mapping (selected structure):"), this);
    mainLayout->addWidget(m_reorderLabel);
    m_reorderText = new QPlainTextEdit(this);
    m_reorderText->setReadOnly(true);
    m_reorderText->setMaximumHeight(120);
    mainLayout->addWidget(m_reorderText);

    connect(m_useReferenceButton, &QPushButton::clicked, this, &RMSDWidget::onUseCurrentAsReference);
    connect(m_addButton, &QPushButton::clicked, this, &RMSDWidget::onAddStructure);
    connect(m_realignButton, &QPushButton::clicked, this, &RMSDWidget::onRealignAll);
    connect(m_methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &RMSDWidget::onMethodChanged);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &RMSDWidget::onSelectionChanged);

    onMethodChanged(m_methodCombo->currentIndex());
}

QString RMSDWidget::currentMethod() const
{
    return m_methodCombo->currentData().toString();
}

void RMSDWidget::onMethodChanged(int /*index*/)
{
    // Template element only matters for template-based methods.
    const QString m = currentMethod();
    const bool templateBased = (m == QLatin1String("subspace")
        || m == QLatin1String("template")
        || m == QLatin1String("dtemplates"));
    m_elementEdit->setEnabled(templateBased);
}

// ---- helpers ----

int RMSDWidget::referenceIndex() const
{
    for (int i = 0; i < m_structures.size(); ++i)
        if (m_structures[i].isReference)
            return i;
    return -1;
}

int RMSDWidget::indexOfId(int id) const
{
    for (int i = 0; i < m_structures.size(); ++i)
        if (m_structures[i].id == id)
            return i;
    return -1;
}

bool RMSDWidget::isDuplicateName(const QString& name) const
{
    for (const Structure& s : m_structures)
        if (s.name.compare(name, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

int RMSDWidget::overlayIndexOf(int structureIndex) const
{
    // Overlay index = position among the non-reference structures (the order in which
    // pushWorkspace() emits them, which the viewer's overlay list mirrors).
    const int refIdx = referenceIndex();
    int oi = 0;
    for (int i = 0; i < structureIndex; ++i)
        if (i != refIdx)
            ++oi;
    return oi;
}

QColor RMSDWidget::nextDefaultTint()
{
    static const QColor palette[] = {
        QColor(60, 200, 90),   // green
        QColor(240, 150, 40),  // orange
        QColor(220, 80, 200),  // magenta
        QColor(70, 200, 220),  // cyan
        QColor(230, 210, 60),  // yellow
        QColor(150, 110, 230), // purple
        QColor(60, 200, 170),  // teal
        QColor(240, 120, 140), // pink
    };
    const int n = int(sizeof(palette) / sizeof(palette[0]));
    return palette[(m_nextTint++) % n];
}

void RMSDWidget::setSwatchColor(QPushButton* b, const QColor& c)
{
    b->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid #555;").arg(c.name()));
}

QWidget* RMSDWidget::centerCell(QWidget* inner)
{
    auto* w = new QWidget(m_table);
    auto* l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setAlignment(Qt::AlignCenter);
    l->addWidget(inner);
    return w;
}

// ---- table ----

void RMSDWidget::rebuildTable()
{
    m_table->setRowCount(0);  // table owns the cell widgets; this deletes the old radios
                              // (their destructor removes them from m_refGroup)
    m_table->setRowCount(m_structures.size());

    for (int row = 0; row < m_structures.size(); ++row) {
        const Structure& s = m_structures[row];
        const int id = s.id;

        // Reference radio
        auto* refRadio = new QRadioButton;
        refRadio->setToolTip(tr("Use as reference (align all others to it)"));
        m_refGroup->addButton(refRadio, id);
        refRadio->setChecked(s.isReference);
        m_table->setCellWidget(row, ColRef, centerCell(refRadio));

        // Visibility checkbox (works for the reference too -> hides the primary)
        auto* show = new QCheckBox;
        show->setChecked(s.visible);
        show->setToolTip(tr("Show / hide this structure"));
        connect(show, &QCheckBox::toggled, this, [this, id](bool on) { onVisibilityToggled(id, on); });
        m_table->setCellWidget(row, ColShow, centerCell(show));

        // Name
        m_table->setItem(row, ColName, new QTableWidgetItem(
            s.isReference ? tr("%1  (reference)").arg(s.name) : s.name));

        // RMSD columns: plain for any aligned structure; perm. only when reordering ran.
        const bool aligned = !s.isReference && s.hasResult;
        auto* r1 = new QTableWidgetItem(
            aligned ? QString::number(s.rmsdPlain, 'f', 3) : QStringLiteral("–"));
        r1->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, ColRmsd, r1);
        auto* r2 = new QTableWidgetItem(
            (aligned && s.reordered) ? QString::number(s.rmsdPerm, 'f', 3) : QStringLiteral("–"));
        r2->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, ColRmsdPerm, r2);

        // Colour-tint swatch (overlays only)
        auto* swatch = new QPushButton;
        swatch->setFixedSize(34, 18);
        if (s.isReference) {
            swatch->setEnabled(false);
            swatch->setToolTip(tr("The reference uses the global colour scheme."));
        } else {
            swatch->setToolTip(tr("Colour tint for this overlay"));
            setSwatchColor(swatch, s.tint);
            connect(swatch, &QPushButton::clicked, this, [this, id] { onColorClicked(id); });
        }
        m_table->setCellWidget(row, ColColor, centerCell(swatch));

        // Size spinbox (overlays only)
        auto* size = new QDoubleSpinBox;
        size->setRange(0.2, 1.5);
        size->setSingleStep(0.05);
        size->setDecimals(2);
        size->setValue(s.sizeScale);
        size->setToolTip(tr("Size relative to the reference (1.0 = identical)"));
        size->setEnabled(!s.isReference);
        if (!s.isReference)
            connect(size, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, id](double v) { onSizeChanged(id, v); });
        m_table->setCellWidget(row, ColSize, size);

        // Remove
        auto* del = new QPushButton(QIcon::fromTheme(QStringLiteral("list-remove")),
            QString());
        del->setFixedWidth(26);
        del->setToolTip(tr("Remove this structure"));
        connect(del, &QPushButton::clicked, this, [this, id] {
            const int i = indexOfId(id);
            if (i >= 0)
                removeStructure(i);
        });
        m_table->setCellWidget(row, ColRemove, centerCell(del));
    }

    onSelectionChanged();
}

void RMSDWidget::updateButtons()
{
    m_realignButton->setEnabled(referenceIndex() >= 0 && m_structures.size() >= 2);
}

// ---- alignment ----

bool RMSDWidget::alignToReference(Structure& s)
{
    const int refIdx = referenceIndex();
    if (refIdx < 0)
        return false;
    if (m_structures[refIdx].id == s.id) {
        s.aligned = s.original;  // the reference itself
        s.hasResult = false;
        return true;
    }

    json controller;
    controller["method"] = currentMethod().toStdString();
    controller["protons"] = m_protonsCheck->isChecked();
    // Reorder ON => force the chosen method's permutation (curcuma only reorders when the
    // atom order differs OR force_reorder is set, so picking a method alone would not
    // reorder conformers in matching order). OFF => disable reordering entirely.
    const bool reorder = m_reorderCheck->isChecked();
    controller["force_reorder"] = reorder;
    controller["no_reorder"] = !reorder;
    controller["threads"] = m_threadsSpin->value();
    s.reordered = reorder;
    if (m_elementEdit->isEnabled()) {
        const QString el = m_elementEdit->text().trimmed();
        if (!el.isEmpty())
            controller["element"] = el.toStdString();
    }

    curcuma::Molecule ref = atomsToMolecule(m_structures[refIdx].original);
    curcuma::Molecule tgt = atomsToMolecule(s.original);

    // Plain RMSD = best-fit (Kabsch) in the original atom order. Computed locally so the
    // column is always available and independent of whether curcuma populated its internal
    // RMSDRaw() (it only does so on the reorder path, for matching atom multisets). Honours
    // "Include hydrogens" so it stays consistent with the permutation RMSD, which depletes
    // protons when disabled. Returns 0 on an atom-count mismatch (no meaningful same-order
    // RMSD). Claude Generated.
    const bool includeH = m_protonsCheck->isChecked();
    auto plainRmsd = [&ref, &tgt, includeH]() -> double {
        auto prep = [includeH](const curcuma::Molecule& m) {
            curcuma::Molecule out;
            for (std::size_t i = 0; i < m.AtomCount(); ++i)
                if (includeH || m.Atom(i).first != 1)
                    out.addPair(m.Atom(i));
            out.Center();
            return out;
        };
        curcuma::Molecule a = prep(ref), b = prep(tgt);
        if (a.AtomCount() == 0 || a.AtomCount() != b.AtomCount())
            return 0.0;
        const Eigen::Matrix3d R = RMSDFunctions::BestFitRotation(a.getGeometry(), b.getGeometry());
        const Geometry aligned = RMSDFunctions::applyRotation(b.getGeometry(), R);
        return RMSDFunctions::getRMSD(a.getGeometry(), aligned);
    };

    bool ok = true;
    try {
        RMSDDriver driver(controller, true);
        driver.setReference(ref);
        driver.setTarget(tgt);
        driver.start();
        s.rules = driver.ReorderRules();
        // Overlay geometry = the target whose deviation equals the reported RMSD. curcuma's
        // TargetForRMSD() returns the reordered + aligned target when reordering ran, else the
        // plain best-fit. Using TargetAligned() directly here drew the un-reordered (rmsd_raw)
        // structure while the table showed the permutation RMSD — the original overlay bug.
        s.aligned = moleculeToAtoms(driver.TargetForRMSD());
        // RMSD columns: plain (best-fit in the original order, local Kabsch above) is always
        // shown; perm. is curcuma's reordered best-fit, only meaningful when reordering ran.
        s.rmsdPlain = plainRmsd();
        s.rmsdPerm = reorder ? driver.RMSD() : s.rmsdPlain;
        s.hasResult = true;
    } catch (const std::exception& e) {
        ok = false;
        qWarning("RMSDDriver failed: %s", e.what());
    } catch (...) {
        ok = false;
    }
    if (!ok || s.aligned.isEmpty()) {
        s.aligned = s.original;
        s.hasResult = false;
        return false;
    }
    return true;
}

void RMSDWidget::realignAll()
{
    if (referenceIndex() < 0)
        return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    for (Structure& s : m_structures) {
        if (s.isReference)
            s.aligned = s.original;
        else
            alignToReference(s);
    }
    QApplication::restoreOverrideCursor();
}

// ---- workspace mutations ----

bool RMSDWidget::addStructure(const QVector<MoleculeViewer::Atom>& atoms,
    const QVector<MoleculeViewer::Bond>& bonds, const QString& name)
{
    if (atoms.isEmpty())
        return false;

    if (isDuplicateName(name)) {
        QMessageBox::information(this, tr("RMSD"),
            tr("A structure named \"%1\" is already in the workspace.").arg(name));
        return false;
    }

    Structure s;
    s.id = m_nextId++;
    s.name = name;
    s.original = atoms;
    s.bonds = bonds;
    s.aligned = atoms;
    s.visible = true;

    if (referenceIndex() < 0) {
        // First structure becomes the reference.
        s.isReference = true;
        m_structures.append(s);
        rebuildTable();
        updateButtons();
        pushWorkspace(/*referenceChanged=*/true);
        return true;
    }

    s.isReference = false;
    s.tint = nextDefaultTint();
    s.sizeScale = 0.8f;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = alignToReference(s);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, tr("RMSD"),
            tr("Alignment failed. Try a different method or check the structures."));
        return false;
    }
    m_structures.append(s);
    rebuildTable();
    updateButtons();
    pushWorkspace(/*referenceChanged=*/false);
    emit structureAligned(s.name, s.rmsdPerm);
    return true;
}

bool RMSDWidget::addStructureFromFile(const QString& path)
{
    QVector<MoleculeViewer::Atom> atoms;
    QVector<MoleculeViewer::Bond> bonds;
    if (!loadStructureFile(path, atoms, bonds)) {
        QMessageBox::warning(this, tr("Add Structure"),
            tr("Could not load structure:\n%1").arg(path));
        return false;
    }
    return addStructure(atoms, bonds, QFileInfo(path).fileName());
}

void RMSDWidget::setReferenceStructure(const QVector<MoleculeViewer::Atom>& atoms,
    const QVector<MoleculeViewer::Bond>& bonds, const QString& name)
{
    if (atoms.isEmpty())
        return;
    const int refIdx = referenceIndex();
    if (refIdx >= 0) {
        // Refresh the existing reference geometry from the current viewer.
        Structure& r = m_structures[refIdx];
        r.original = atoms;
        r.bonds = bonds;
        r.name = name;
        r.aligned = atoms;
    } else {
        Structure s;
        s.id = m_nextId++;
        s.name = name;
        s.original = atoms;
        s.bonds = bonds;
        s.aligned = atoms;
        s.isReference = true;
        s.visible = true;
        m_structures.append(s);
    }
    realignAll();
    rebuildTable();
    updateButtons();
    pushWorkspace(/*referenceChanged=*/true);
}

void RMSDWidget::setReferenceByIndex(int index)
{
    if (index < 0 || index >= m_structures.size() || m_structures[index].isReference)
        return;
    for (int i = 0; i < m_structures.size(); ++i)
        m_structures[i].isReference = (i == index);
    m_structures[index].aligned = m_structures[index].original;
    realignAll();
    rebuildTable();
    updateButtons();
    pushWorkspace(/*referenceChanged=*/true);
}

void RMSDWidget::removeStructure(int index)
{
    if (index < 0 || index >= m_structures.size())
        return;
    const bool wasReference = m_structures[index].isReference;
    m_structures.removeAt(index);

    if (m_structures.isEmpty()) {
        rebuildTable();
        updateButtons();
        emit overlayWorkspaceChanged({}, {}, true, {}, /*resetView=*/false);  // clear overlays
        return;
    }
    if (wasReference) {
        // Promote the first remaining structure to reference and re-align the rest.
        m_structures[0].isReference = true;
        m_structures[0].aligned = m_structures[0].original;
        realignAll();
        rebuildTable();
        updateButtons();
        pushWorkspace(/*referenceChanged=*/true);
    } else {
        rebuildTable();
        updateButtons();
        pushWorkspace(/*referenceChanged=*/false);
    }
}

void RMSDWidget::clearWorkspace()
{
    if (m_structures.isEmpty())
        return;
    m_structures.clear();
    rebuildTable();
    updateButtons();
    emit overlayWorkspaceChanged({}, {}, true, {}, /*resetView=*/false);  // clear overlays, keep primary
}

void RMSDWidget::pushWorkspace(bool referenceChanged)
{
    const int refIdx = referenceIndex();
    if (refIdx < 0) {
        emit overlayWorkspaceChanged({}, {}, true, {}, referenceChanged);
        return;
    }
    const Structure& ref = m_structures[refIdx];
    QVector<MoleculeViewer::OverlaySpec> overlays;
    for (int i = 0; i < m_structures.size(); ++i) {
        if (i == refIdx)
            continue;
        const Structure& s = m_structures[i];
        MoleculeViewer::OverlaySpec spec;
        spec.atoms = s.aligned;
        spec.tint = s.tint;
        spec.sizeScale = s.sizeScale;
        spec.visible = s.visible;
        overlays.append(spec);
    }
    emit overlayWorkspaceChanged(ref.original, ref.bonds, ref.visible, overlays, referenceChanged);
}

// ---- per-row edits ----

void RMSDWidget::onColorClicked(int id)
{
    const int i = indexOfId(id);
    if (i < 0)
        return;
    const QColor c = QColorDialog::getColor(m_structures[i].tint, this, tr("Overlay colour tint"));
    if (!c.isValid())
        return;
    m_structures[i].tint = c;
    if (QWidget* cell = m_table->cellWidget(i, ColColor)) {
        if (auto* btn = cell->findChild<QPushButton*>())
            setSwatchColor(btn, c);
    }
    emit overlayTintChanged(overlayIndexOf(i), c);
}

void RMSDWidget::onSizeChanged(int id, double value)
{
    const int i = indexOfId(id);
    if (i < 0)
        return;
    m_structures[i].sizeScale = float(value);
    emit overlaySizeChanged(overlayIndexOf(i), float(value));
}

void RMSDWidget::onVisibilityToggled(int id, bool on)
{
    const int i = indexOfId(id);
    if (i < 0)
        return;
    m_structures[i].visible = on;
    if (m_structures[i].isReference)
        emit referenceVisibilityChanged(on);
    else
        emit overlayVisibilityChanged(overlayIndexOf(i), on);
}

void RMSDWidget::onSelectionChanged()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_structures.size()) {
        m_reorderText->clear();
        return;
    }
    const Structure& s = m_structures[row];
    if (s.isReference) {
        m_reorderText->setPlainText(tr("(reference structure)"));
        return;
    }
    if (!s.hasResult) {
        m_reorderText->setPlainText(tr("(not aligned)"));
        return;
    }
    if (!s.reordered) {
        m_reorderText->setPlainText(
            tr("RMSD %1 Å  (no reordering — enable \"Reorder atoms\" for the permutation RMSD)")
                .arg(s.rmsdPlain, 0, 'f', 4));
        return;
    }
    QString mapStr;
    for (std::size_t i = 0; i < s.rules.size(); ++i)
        mapStr += QStringLiteral("%1 → %2\n").arg(i).arg(s.rules[i]);
    if (s.rules.empty())
        mapStr = tr("(no reorder rules returned)");
    m_reorderText->setPlainText(
        tr("plain RMSD %1 Å · permutation RMSD %2 Å\n\n%3")
            .arg(s.rmsdPlain, 0, 'f', 4)
            .arg(s.rmsdPerm, 0, 'f', 4)
            .arg(mapStr));
}

// ---- button slots ----

void RMSDWidget::onUseCurrentAsReference()
{
    // MainWindow owns the viewer; it re-seeds via setReferenceStructure().
    emit seedReferenceRequested();
}

void RMSDWidget::onAddStructure()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Add Structure"),
        QString(), tr("Molecular structures (*.xyz *.pdb *.mol2 *.vtf)"));
    if (path.isEmpty())
        return;
    addStructureFromFile(path);
}

void RMSDWidget::onRealignAll()
{
    if (referenceIndex() < 0)
        return;
    realignAll();
    rebuildTable();
    pushWorkspace(/*referenceChanged=*/false);  // overlay geometry changed; primary unchanged
}

// ---- file loading ----

bool RMSDWidget::loadStructureFile(const QString& path,
    QVector<MoleculeViewer::Atom>& atoms, QVector<MoleculeViewer::Bond>& bonds)
{
    atoms.clear();
    bonds.clear();
    const QString suffix = QFileInfo(path).suffix().toLower();

    if (suffix == QLatin1String("xyz")) {
        XYZParser parser;
        XYZParser::XYZFrame frame;
        if (!parser.parseFile(path, frame))
            return false;
        XYZParser::convertToMoleculeViewer(frame, atoms, bonds);
    } else if (suffix == QLatin1String("pdb")) {
        PDBParser parser;
        PDBParser::PDBFrame frame;
        if (!parser.parseFile(path, frame))
            return false;
        PDBParser::convertToMoleculeViewer(frame, atoms, bonds, parser.getBonds());
    } else if (suffix == QLatin1String("mol2")) {
        MOL2Parser parser;
        MOL2Parser::MOL2Molecule mol;
        if (!parser.parseFile(path, mol))
            return false;
        MOL2Parser::convertToMoleculeViewer(mol, atoms, bonds);
    } else if (suffix == QLatin1String("vtf")) {
        VTFParser parser;
        VTFParser::VTFFrame frame;
        if (!parser.parseFile(path, frame))
            return false;
        VTFParser::convertToMoleculeViewer(frame, atoms, bonds);
    } else {
        return false;
    }
    return !atoms.isEmpty();
}
