// rmsddialog.cpp
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Implementation of the RMSD / align / reorder dialog.
// Wraps curcuma's RMSDDriver: aligns a target structure onto the currently
// displayed reference, optionally reordering atoms (permutation) so chemically
// equivalent atoms are matched, then reports RMSD + reorder mapping and emits
// the aligned target for 3D overlay.
#include "rmsddialog.h"

#include "../moleculebridge.h"
#include "../mol2parser.h"
#include "../pdbparser.h"
#include "../vtfparser.h"
#include "../xyzparser.h"

#include <src/capabilities/rmsd.h>

#include "external/json.hpp"
using json = nlohmann::json;

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <vector>

RMSDDialog::RMSDDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("RMSD / Align Structures"));
    setupUI();
    updateReferenceLabel();
    updateTargetLabel();
}

void RMSDDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // --- Structures ---
    auto* structBox = new QGroupBox(tr("Structures"), this);
    auto* structLayout = new QVBoxLayout(structBox);
    m_referenceLabel = new QLabel(this);
    auto* targetRow = new QHBoxLayout();
    m_targetLabel = new QLabel(this);
    m_loadTargetButton = new QPushButton(tr("Load target file…"), this);
    targetRow->addWidget(m_targetLabel, 1);
    targetRow->addWidget(m_loadTargetButton);
    structLayout->addWidget(m_referenceLabel);
    structLayout->addLayout(targetRow);
    mainLayout->addWidget(structBox);

    // --- Options ---
    auto* optBox = new QGroupBox(tr("Alignment options"), this);
    auto* optLayout = new QFormLayout(optBox);

    m_methodCombo = new QComboBox(this);
    // label -> RMSDDriver method string (see curcuma rmsd.cpp method_map)
    m_methodCombo->addItem(tr("Subspace (recommended)"), QStringLiteral("subspace"));
    m_methodCombo->addItem(tr("Inertia (template-free)"), QStringLiteral("inertia"));
    m_methodCombo->addItem(tr("Template"), QStringLiteral("template"));
    m_methodCombo->addItem(tr("Distance template"), QStringLiteral("dtemplate"));
    m_methodCombo->addItem(tr("Incremental (legacy)"), QStringLiteral("incr"));
    m_methodCombo->addItem(tr("MolAlign (external)"), QStringLiteral("molalign"));
    m_methodCombo->addItem(tr("Predefined order"), QStringLiteral("predefined"));
    optLayout->addRow(tr("Method:"), m_methodCombo);

    m_protonsCheck = new QCheckBox(tr("Include hydrogens"), this);
    m_protonsCheck->setChecked(true);
    optLayout->addRow(QString(), m_protonsCheck);

    m_forceReorderCheck = new QCheckBox(tr("Force reorder"), this);
    optLayout->addRow(QString(), m_forceReorderCheck);

    m_noReorderCheck = new QCheckBox(tr("Disable reorder"), this);
    optLayout->addRow(QString(), m_noReorderCheck);

    m_elementEdit = new QLineEdit(QStringLiteral("7"), this);
    m_elementEdit->setToolTip(tr("Element(s) used to build the template, e.g. \"7\" or \"7,8\" (template methods only)."));
    optLayout->addRow(tr("Template element(s):"), m_elementEdit);

    m_threadsSpin = new QSpinBox(this);
    m_threadsSpin->setRange(1, 64);
    m_threadsSpin->setValue(1);
    optLayout->addRow(tr("Threads:"), m_threadsSpin);

    mainLayout->addWidget(optBox);

    m_alignButton = new QPushButton(tr("Align"), this);
    mainLayout->addWidget(m_alignButton);

    // --- Results ---
    auto* resultBox = new QGroupBox(tr("Result"), this);
    auto* resultLayout = new QVBoxLayout(resultBox);
    m_rmsdLabel = new QLabel(tr("RMSD: –"), this);
    m_reorderText = new QPlainTextEdit(this);
    m_reorderText->setReadOnly(true);
    m_reorderText->setMaximumHeight(160);
    m_saveButton = new QPushButton(tr("Save aligned target…"), this);
    m_saveButton->setEnabled(false);
    resultLayout->addWidget(m_rmsdLabel);
    resultLayout->addWidget(new QLabel(tr("Reorder mapping (target index → reference index):"), this));
    resultLayout->addWidget(m_reorderText);
    resultLayout->addWidget(m_saveButton);
    mainLayout->addWidget(resultBox);

    connect(m_loadTargetButton, &QPushButton::clicked, this, &RMSDDialog::onLoadTarget);
    connect(m_alignButton, &QPushButton::clicked, this, &RMSDDialog::onAlign);
    connect(m_saveButton, &QPushButton::clicked, this, &RMSDDialog::onSaveAligned);
    connect(m_methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &RMSDDialog::onMethodChanged);

    onMethodChanged(m_methodCombo->currentIndex());
}

QString RMSDDialog::currentMethod() const
{
    return m_methodCombo->currentData().toString();
}

void RMSDDialog::onMethodChanged(int /*index*/)
{
    // Template element only matters for template-based methods.
    const QString m = currentMethod();
    const bool templateBased = (m == QLatin1String("subspace")
        || m == QLatin1String("template")
        || m == QLatin1String("dtemplate"));
    m_elementEdit->setEnabled(templateBased);
}

void RMSDDialog::setReferenceStructure(const QVector<MoleculeViewer::Atom>& atoms,
    const QVector<MoleculeViewer::Bond>& bonds, const QString& name)
{
    m_refAtoms = atoms;
    m_refBonds = bonds;
    m_refName = name;
    updateReferenceLabel();
}

void RMSDDialog::setTargetFile(const QString& path)
{
    QVector<MoleculeViewer::Atom> atoms;
    QVector<MoleculeViewer::Bond> bonds;
    if (!loadStructureFile(path, atoms, bonds)) {
        QMessageBox::warning(this, tr("Load Target"),
            tr("Could not load target structure:\n%1").arg(path));
        return;
    }
    m_targetAtoms = atoms;
    m_targetBonds = bonds;
    m_targetName = QFileInfo(path).fileName();
    updateTargetLabel();
}

void RMSDDialog::updateReferenceLabel()
{
    const QString name = m_refName.isEmpty() ? tr("(none)") : m_refName;
    m_referenceLabel->setText(tr("Reference: %1  [%2 atoms]").arg(name).arg(m_refAtoms.size()));
}

void RMSDDialog::updateTargetLabel()
{
    const QString name = m_targetName.isEmpty() ? tr("(none — load a target file)") : m_targetName;
    m_targetLabel->setText(tr("Target: %1  [%2 atoms]").arg(name).arg(m_targetAtoms.size()));
}

void RMSDDialog::onLoadTarget()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Load Target Structure"),
        QString(), tr("Molecular structures (*.xyz *.pdb *.mol2 *.vtf)"));
    if (path.isEmpty())
        return;
    setTargetFile(path);
}

bool RMSDDialog::loadStructureFile(const QString& path,
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

void RMSDDialog::onAlign()
{
    if (m_refAtoms.isEmpty()) {
        QMessageBox::warning(this, tr("RMSD"),
            tr("No reference structure. Load a molecule in the viewer first."));
        return;
    }
    if (m_targetAtoms.isEmpty()) {
        QMessageBox::warning(this, tr("RMSD"), tr("No target structure. Load a target file."));
        return;
    }

    // Build the RMSDDriver controller json from the UI (rmsd-level keys, no wrapper).
    json controller;
    controller["method"] = currentMethod().toStdString();
    controller["protons"] = m_protonsCheck->isChecked();
    controller["force_reorder"] = m_forceReorderCheck->isChecked();
    controller["no_reorder"] = m_noReorderCheck->isChecked();
    controller["threads"] = m_threadsSpin->value();
    if (m_elementEdit->isEnabled()) {
        const QString el = m_elementEdit->text().trimmed();
        if (!el.isEmpty())
            controller["element"] = el.toStdString();
    }

    curcuma::Molecule ref = atomsToMolecule(m_refAtoms);
    curcuma::Molecule tgt = atomsToMolecule(m_targetAtoms);

    double rmsd = 0.0;
    double rmsdRaw = 0.0;
    std::vector<int> rules;
    bool ok = true;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    try {
        RMSDDriver driver(controller, true);
        driver.setReference(ref);
        driver.setTarget(tgt);
        driver.start();
        rmsd = driver.RMSD();
        rmsdRaw = driver.RMSDRaw();
        rules = driver.ReorderRules();
        m_alignedAtoms = moleculeToAtoms(driver.TargetAligned());
    } catch (const std::exception& e) {
        ok = false;
        qWarning("RMSDDriver failed: %s", e.what());
    } catch (...) {
        ok = false;
    }
    QApplication::restoreOverrideCursor();

    if (!ok || m_alignedAtoms.isEmpty()) {
        QMessageBox::warning(this, tr("RMSD"),
            tr("Alignment failed. Try a different method or check the structures."));
        return;
    }

    m_haveResult = true;
    m_saveButton->setEnabled(true);

    m_rmsdLabel->setText(tr("RMSD: %1 Å   (raw, before reorder: %2 Å)")
                             .arg(rmsd, 0, 'f', 4)
                             .arg(rmsdRaw, 0, 'f', 4));

    bool reordered = false;
    QString mapStr;
    for (std::size_t i = 0; i < rules.size(); ++i) {
        if (static_cast<int>(i) != rules[i])
            reordered = true;
        mapStr += QStringLiteral("%1 → %2\n").arg(i).arg(rules[i]);
    }
    if (rules.empty())
        mapStr = tr("(no reorder rules returned)");
    m_reorderText->setPlainText(
        tr("reordered: %1\n\n%2").arg(reordered ? tr("yes") : tr("no")).arg(mapStr));

    // Hand the aligned target back to MainWindow for 3D overlay.
    emit overlayRequested(m_refAtoms, m_refBonds, m_alignedAtoms);
}

void RMSDDialog::onSaveAligned()
{
    if (!m_haveResult || m_alignedAtoms.isEmpty())
        return;

    QString path = QFileDialog::getSaveFileName(this, tr("Save Aligned Target"),
        QString(), tr("XYZ Files (*.xyz)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QLatin1String(".xyz"), Qt::CaseInsensitive))
        path += QStringLiteral(".xyz");

    XYZParser::XYZFrame frame;
    if (!XYZParser::convertFromMoleculeViewer(m_alignedAtoms,
            QStringLiteral("aligned target (qurcuma RMSD)"), frame)
        || !XYZParser::writeFile(path, frame)) {
        QMessageBox::warning(this, tr("Save"), tr("Failed to save aligned structure."));
        return;
    }
}
