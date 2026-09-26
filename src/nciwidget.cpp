// nciwidget.cpp - Contact table of the non-covalent interaction analysis
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "nciwidget.h"

#include "ncianalysis.h"
#include "settings.h"
#include "widgets/colorswatch.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSignalBlocker>
#include <QSpinBox>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
// Column order of the contact table.
enum Column {
    ColKind = 0,
    ColAtoms,
    ColDistance,
    ColAngle,
    ColScore,
    ColEnergy,
    ColNote,
    ColumnCount
};

/// Human-readable note per contact: the GFN-FF hydrogen-bond case for a
/// force-field row, the stacking geometry for pi contacts.
QString noteFor(const nci::Contact& c)
{
    if (c.kind == nci::Kind::PiStacking)
        return c.motif == 2 ? QObject::tr("T-shaped")
                            : QObject::tr("parallel, offset %1 A").arg(c.offset, 0, 'f', 2);
    if (c.motif > 0)
        return QObject::tr("GFN-FF case %1").arg(c.motif);
    return QString();
}
} // namespace

NciWidget::NciWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void NciWidget::setupUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    auto* controls = new QHBoxLayout;
    controls->setSpacing(6);

    controls->addWidget(new QLabel(tr("Source:"), this));
    m_sourceCombo = new QComboBox(this);
    m_sourceCombo->addItem(tr("Off"), 0);
    m_sourceCombo->addItem(tr("Geometry (distance/angle)"), 1);
    m_sourceCombo->addItem(tr("GFN-FF parameters"), 2);
    m_sourceCombo->addItem(tr("Population analysis (GFN2)"), 3);
    connect(m_sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
        const int source = m_sourceCombo->itemData(i).toInt();
        m_analyseButton->setEnabled(source >= 2);
        emit sourceChanged(source);
    });
    controls->addWidget(m_sourceCombo);

    m_analyseButton = new QPushButton(tr("Analyse current frame"), this);
    m_analyseButton->setEnabled(false);
    m_analyseButton->setToolTip(tr("Run the selected method on the frame currently shown. "
                                   "The geometric source needs no calculation and updates by itself."));
    connect(m_analyseButton, &QPushButton::clicked, this, [this]() {
        emit analysisRequested(m_sourceCombo->currentData().toInt());
    });
    controls->addWidget(m_analyseButton);

    controls->addStretch();

    m_copyButton = new QPushButton(tr("Copy table"), this);
    m_copyButton->setToolTip(tr("Copy the contact list to the clipboard as tab-separated text."));
    connect(m_copyButton, &QPushButton::clicked, this, &NciWidget::copyTable);
    controls->addWidget(m_copyButton);

    root->addLayout(controls);

    m_summaryLabel = new QLabel(tr("No contacts."), this);
    m_summaryLabel->setWordWrap(true);
    root->addWidget(m_summaryLabel);

    m_table = new QTableWidget(0, ColumnCount, this);
    m_table->setHorizontalHeaderLabels({ tr("Type"), tr("Atoms"), tr("d / A"), tr("Angle / deg"),
        tr("Score"), tr("E / kJ mol-1"), tr("Note") });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSortingEnabled(false);

    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        const int row = m_table->currentRow();
        const QVector<int> atoms = atomsOfRow(row);
        if (!atoms.isEmpty())
            emit contactSelected(atoms);
    });
    connect(m_table, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem* item) {
        const QVector<int> atoms = atomsOfRow(item ? item->row() : -1);
        if (!atoms.isEmpty())
            emit contactFocused(atoms);
    });

    root->addWidget(m_table, 1);
}

void NciWidget::setKindPalette(const nci::Palette& palette)
{
    m_palette = palette;
    // Restyle the rows already on screen instead of asking for a new analysis.
    for (int row = 0; row < m_table->rowCount() && row < m_result.contacts.size(); ++row) {
        if (QTableWidgetItem* item = m_table->item(row, ColKind)) {
            const nci::Contact& c = m_result.contacts[row];
            item->setForeground(nci::kindColor(c.kind, c.energy, m_palette));
        }
    }
}

void NciWidget::setSource(int source)
{
    if (!m_sourceCombo)
        return;
    const int index = m_sourceCombo->findData(source);
    if (index < 0 || index == m_sourceCombo->currentIndex())
        return;
    m_sourceCombo->blockSignals(true);
    m_sourceCombo->setCurrentIndex(index);
    m_sourceCombo->blockSignals(false);
    m_analyseButton->setEnabled(source >= 2);
}

void NciWidget::setBusy(bool busy)
{
    if (m_analyseButton)
        m_analyseButton->setEnabled(!busy && m_sourceCombo->currentData().toInt() >= 2);
}

void NciWidget::setStatus(const QString& text)
{
    if (m_summaryLabel)
        m_summaryLabel->setText(text);
}

void NciWidget::setResult(const nci::Result& result, const QVector<MoleculeViewer::Atom>& atoms)
{
    m_result = result;

    m_summaryLabel->setText(result.summary.isEmpty() ? tr("No contacts.") : result.summary);

    m_table->setRowCount(result.contacts.size());
    for (int row = 0; row < result.contacts.size(); ++row) {
        const nci::Contact& c = result.contacts[row];

        auto* kindItem = new QTableWidgetItem(nci::kindName(c.kind));
        // The row colour matches the overlay line so table and 3D view read as one.
        kindItem->setForeground(nci::kindColor(c.kind, c.energy, m_palette));
        m_table->setItem(row, ColKind, kindItem);

        m_table->setItem(row, ColAtoms, new QTableWidgetItem(nci::describe(c, atoms)));
        m_table->setItem(row, ColDistance,
            new QTableWidgetItem(QString::number(c.distance, 'f', 3)));
        m_table->setItem(row, ColAngle,
            new QTableWidgetItem(c.angle > 0.0f ? QString::number(c.angle, 'f', 1) : QString()));
        m_table->setItem(row, ColScore, new QTableWidgetItem(QString::number(c.score, 'f', 2)));
        // Only electrostatic and dispersion rows carry a real pair energy; hydrogen
        // and halogen bonds are reported as a system total in the summary line,
        // because GFN-FF does not keep their energy per contact.
        m_table->setItem(row, ColEnergy,
            new QTableWidgetItem(c.hasEnergy ? QString::number(c.energy, 'f', 2) : QString()));
        m_table->setItem(row, ColNote, new QTableWidgetItem(noteFor(c)));
    }
    m_table->resizeColumnsToContents();
}

QVector<int> NciWidget::atomsOfRow(int row) const
{
    if (row < 0 || row >= m_result.contacts.size())
        return {};
    return nci::contactAtoms(m_result.contacts[row]);
}

void NciWidget::copyTable()
{
    QStringList lines;
    QStringList header;
    for (int col = 0; col < m_table->columnCount(); ++col)
        header << m_table->horizontalHeaderItem(col)->text();
    lines << header.join(QLatin1Char('\t'));

    for (int row = 0; row < m_table->rowCount(); ++row) {
        QStringList cells;
        for (int col = 0; col < m_table->columnCount(); ++col) {
            const QTableWidgetItem* item = m_table->item(row, col);
            cells << (item ? item->text() : QString());
        }
        lines << cells.join(QLatin1Char('\t'));
    }
    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
}

// ---------------------------------------------------------------------------
// NciOptionsWidget (Claude Generated 2026 - UX stage 4)
// ---------------------------------------------------------------------------
NciOptionsWidget::NciOptionsWidget(MoleculeViewer* viewer, Settings* settings, QWidget* parent)
    : QWidget(parent)
    , m_viewer(viewer)
    , m_settings(settings)
{
    auto* f = new QFormLayout(this);
    f->setContentsMargins(4, 2, 4, 2);

    auto* kinds = new QWidget(this);
    auto* kindRow = new QHBoxLayout(kinds);
    kindRow->setContentsMargins(0, 0, 0, 0);
    m_hbondCheck = new QCheckBox(tr("H"), this);
    m_hbondCheck->setToolTip(tr("Hydrogen bonds D-H...A with D, A from N, O, F, S"));
    m_xbondCheck = new QCheckBox(tr("X"), this);
    m_xbondCheck->setToolTip(tr("Halogen bonds C-X...A with X = Cl, Br, I"));
    m_piCheck = new QCheckBox(QString::fromUtf8("\xcf\x80"), this);
    m_piCheck->setToolTip(tr("Pi stacking between planar five- and six-rings"));
    m_contactCheck = new QCheckBox(tr("vdW"), this);
    m_contactCheck->setToolTip(tr("Undirected close contacts below 0.9 times the sum of "
                                  "the van der Waals radii. Can produce many lines."));
    m_electrostaticCheck = new QCheckBox(tr("q"), this);
    m_electrostaticCheck->setToolTip(tr("GFN-FF source only: electrostatic atom pairs with "
                                        "their Coulomb pair energy, coloured by sign."));
    m_dispersionCheck = new QCheckBox(tr("disp"), this);
    m_dispersionCheck->setToolTip(tr("GFN-FF source only: dispersion atom pairs with their "
                                     "D4 pair energy."));
    for (QCheckBox* c : { m_hbondCheck, m_xbondCheck, m_piCheck, m_contactCheck,
             m_electrostaticCheck, m_dispersionCheck }) {
        kindRow->addWidget(c);
        connect(c, &QCheckBox::toggled, this, [this]() { applyOptions(); });
    }
    kindRow->addStretch();
    f->addRow(tr("Show:"), kinds);

    auto* gate = new QWidget(this);
    auto* gateRow = new QHBoxLayout(gate);
    gateRow->setContentsMargins(0, 0, 0, 0);
    m_hbDistanceSpin = new QDoubleSpinBox(this);
    m_hbDistanceSpin->setRange(2.0, 3.5);
    m_hbDistanceSpin->setSingleStep(0.05);
    m_hbDistanceSpin->setDecimals(2);
    m_hbDistanceSpin->setSuffix(QString::fromUtf8(" \xc3\x85"));
    m_hbDistanceSpin->setToolTip(tr(
        "Maximum H...A distance. 2.50 A covers the strong and moderate bands and the "
        "top of the weak band (Jeffrey, An Introduction to Hydrogen Bonding, 1997)."));
    m_hbAngleSpin = new QSpinBox(this);
    m_hbAngleSpin->setRange(90, 180);
    m_hbAngleSpin->setSuffix(QString::fromUtf8(" \xc2\xb0"));
    m_hbAngleSpin->setToolTip(tr(
        "Minimum D-H...A angle. The IUPAC definition requires the angle to tend "
        "towards linearity (Arunan et al., Pure Appl. Chem. 2011, 83, 1637)."));
    gateRow->addWidget(m_hbDistanceSpin);
    gateRow->addWidget(m_hbAngleSpin);
    gateRow->addStretch();
    connect(m_hbDistanceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [this]() { applyOptions(); });
    connect(m_hbAngleSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [this]() { applyOptions(); });
    f->addRow(tr("H...A / angle:"), gate);

    // Colours per interaction class: pick a class, pick its colour. The electrostatic
    // term is listed twice because the default palette splits it by sign.
    auto* colourRow = new QWidget(this);
    auto* colourLayout = new QHBoxLayout(colourRow);
    colourLayout->setContentsMargins(0, 0, 0, 0);
    m_kindCombo = new QComboBox(this);
    for (const auto& entry : nci::paletteEntries())
        m_kindCombo->addItem(entry.second, entry.first);
    m_kindCombo->setToolTip(tr("Interaction class whose overlay colour you want to change."));
    colourLayout->addWidget(m_kindCombo, 1);
    m_kindColorButton = new QPushButton(this);
    m_kindColorButton->setMinimumWidth(80);
    m_kindColorButton->setToolTip(tr("Colour of this interaction class in the 3D overlay "
                                     "and in the contact table."));
    colourLayout->addWidget(m_kindColorButton);
    auto* colourReset = new QPushButton(tr("Auto"), this);
    colourReset->setToolTip(tr("Drop all custom interaction colours."));
    colourLayout->addWidget(colourReset);
    f->addRow(tr("Colour:"), colourRow);
    connect(m_kindCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
        if (m_viewer && i >= 0)
            swatch::apply(m_kindColorButton, m_viewer->getNciKindColor(m_kindCombo->itemData(i).toInt()));
    });
    connect(m_kindColorButton, &QPushButton::clicked, this, [this]() {
        if (!m_viewer)
            return;
        const int key = m_kindCombo->currentData().toInt();
        const QColor chosen = QColorDialog::getColor(m_viewer->getNciKindColor(key), this,
            tr("Colour for %1").arg(m_kindCombo->currentText()));
        if (!chosen.isValid())
            return;
        m_viewer->setNciKindColor(key, chosen);
        if (m_settings)
            m_settings->setNciPalette(m_viewer->getNciPalette());
        refreshPalette();
    });
    connect(colourReset, &QPushButton::clicked, this, [this]() {
        if (!m_viewer)
            return;
        m_viewer->resetNciKindColors();
        if (m_settings)
            m_settings->setNciPalette({});
        refreshPalette();
    });

    m_labelCheck = new QCheckBox(tr("Label contacts with the distance"), this);
    connect(m_labelCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_viewer) m_viewer->setNciLabelsVisible(on);
    });
    f->addRow(QString(), m_labelCheck);

    m_liveMdCheck = new QCheckBox(tr("Live from GFN-FF during MD"), this);
    m_liveMdCheck->setToolTip(tr(
        "Take the contact list from the running GFN-FF force field on every step "
        "instead of from the geometry. The force field then rebuilds its hydrogen- "
        "and halogen-bond lists every step, which costs simulation speed."));
    connect(m_liveMdCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_viewer) m_viewer->setNciLiveMd(on);
        emit liveMdChanged(on);
    });
    f->addRow(QString(), m_liveMdCheck);

    if (m_viewer) {
        // Options also change from the View menu (Hydrogen Bonds quick toggle), from
        // a restored session and from Reset; the widgets follow read-only.
        connect(m_viewer, &MoleculeViewer::nciOptionsChanged, this, [this]() {
            if (!m_applying)
                syncFromViewer();
        });
        connect(m_viewer, &MoleculeViewer::nciSourceChanged, this, [this](int) { updatePairKinds(); });
        connect(m_viewer, &MoleculeViewer::nciPaletteChanged, this, [this]() { refreshPalette(); });
    }
    syncFromViewer();
}

void NciOptionsWidget::syncFromViewer()
{
    if (!m_viewer)
        return;
    const QSignalBlocker b1(m_hbondCheck), b2(m_xbondCheck), b3(m_piCheck), b4(m_contactCheck),
        b5(m_electrostaticCheck), b6(m_dispersionCheck), b7(m_hbDistanceSpin), b8(m_hbAngleSpin),
        b9(m_labelCheck), b10(m_liveMdCheck);
    const nci::Options o = m_viewer->getNciOptions();
    m_hbondCheck->setChecked(o.hydrogenBonds);
    m_xbondCheck->setChecked(o.halogenBonds);
    m_piCheck->setChecked(o.piStacking);
    m_contactCheck->setChecked(o.closeContacts);
    m_electrostaticCheck->setChecked(o.electrostatics);
    m_dispersionCheck->setChecked(o.dispersion);
    m_hbDistanceSpin->setValue(o.hbMaxDistance);
    m_hbAngleSpin->setValue(int(o.hbMinAngle));
    m_labelCheck->setChecked(m_viewer->getNciLabelsVisible());
    m_liveMdCheck->setChecked(m_viewer->getNciLiveMd());
    updatePairKinds();
    refreshPalette();
}

void NciOptionsWidget::applyOptions()
{
    if (!m_viewer)
        return;
    nci::Options o = m_viewer->getNciOptions();
    o.hydrogenBonds = m_hbondCheck->isChecked();
    o.halogenBonds = m_xbondCheck->isChecked();
    o.piStacking = m_piCheck->isChecked();
    o.closeContacts = m_contactCheck->isChecked();
    o.electrostatics = m_electrostaticCheck->isChecked();
    o.dispersion = m_dispersionCheck->isChecked();
    o.hbMaxDistance = float(m_hbDistanceSpin->value());
    o.hbMinAngle = float(m_hbAngleSpin->value());
    m_applying = true;
    m_viewer->setNciOptions(o);
    m_applying = false;
}

void NciOptionsWidget::refreshPalette()
{
    if (!m_viewer || !m_kindCombo)
        return;
    for (int i = 0; i < m_kindCombo->count(); ++i)
        m_kindCombo->setItemIcon(i, swatch::icon(m_viewer->getNciKindColor(m_kindCombo->itemData(i).toInt())));
    swatch::apply(m_kindColorButton, m_viewer->getNciKindColor(m_kindCombo->currentData().toInt()));
}

void NciOptionsWidget::updatePairKinds()
{
    const bool gfnff = m_viewer && m_viewer->getNciSource() == 2;
    m_electrostaticCheck->setEnabled(gfnff);
    m_dispersionCheck->setEnabled(gfnff);
}
