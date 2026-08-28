// nciwidget.cpp - Contact table of the non-covalent interaction analysis
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "nciwidget.h"

#include "ncianalysis.h"

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
