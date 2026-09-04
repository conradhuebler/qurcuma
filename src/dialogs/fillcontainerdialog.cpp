// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 — see fillcontainerdialog.h.

#include "fillcontainerdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

// Only fragments that stand on their own belong in a container fill; the
// substituents carry an Xx attachment point and are meant to dock onto an atom.
bool isStandalone(const build::Fragment& f)
{
    return f.attachAtom < 0;
}

} // namespace

FillContainerDialog::FillContainerDialog(const SimulationConfig& cfg, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Fill Container"));
    auto* outer = new QVBoxLayout(this);

    auto* hint = new QLabel(tr("Place randomly oriented copies of molecules inside a container. "
                               "Use this to set up a gas-phase reaction, for example nitrogen and "
                               "hydrogen for ammonia synthesis."),
        this);
    hint->setWordWrap(true);
    outer->addWidget(hint);

    // ---- fragment rows ----
    m_table = new QTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({ tr("Molecule"), tr("Copies") });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setMaximumHeight(170);
    outer->addWidget(m_table);

    auto* rowBtns = new QHBoxLayout;
    auto* addBtn = new QPushButton(tr("Add row"), this);
    auto* removeBtn = new QPushButton(tr("Remove row"), this);
    connect(addBtn, &QPushButton::clicked, this, [this]() { addRow(); });
    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        const int r = m_table->currentRow();
        if (r >= 0)
            m_table->removeRow(r);
    });
    rowBtns->addWidget(addBtn);
    rowBtns->addWidget(removeBtn);
    rowBtns->addStretch();
    outer->addLayout(rowBtns);

    // A useful starting point rather than an empty table.
    addRow(QStringLiteral("N2"), 2);
    addRow(QStringLiteral("H2"), 6);

    // ---- container ----
    auto* form = new QFormLayout;
    m_shapeCombo = new QComboBox(this);
    m_shapeCombo->addItem(tr("Sphere"), int(build::Container::Sphere));
    m_shapeCombo->addItem(tr("Box"), int(build::Container::Box));
    form->addRow(tr("Container:"), m_shapeCombo);

    m_shapeStack = new QStackedWidget(this);

    auto* spherePage = new QWidget(this);
    auto* sphereForm = new QFormLayout(spherePage);
    sphereForm->setContentsMargins(0, 0, 0, 0);
    m_radiusSpin = new QDoubleSpinBox(this);
    m_radiusSpin->setRange(1.0, 100.0);
    m_radiusSpin->setDecimals(2);
    m_radiusSpin->setSuffix(QStringLiteral(" Å"));
    m_radiusSpin->setValue(cfg.wallRadius > 0.0 ? cfg.wallRadius : 6.0);
    sphereForm->addRow(tr("Radius:"), m_radiusSpin);
    m_shapeStack->addWidget(spherePage);

    auto* boxPage = new QWidget(this);
    auto* boxForm = new QFormLayout(boxPage);
    boxForm->setContentsMargins(0, 0, 0, 0);
    const char* axisName[3] = { "X", "Y", "Z" };
    const double defMin[3] = { cfg.wallXmin, cfg.wallYmin, cfg.wallZmin };
    const double defMax[3] = { cfg.wallXmax, cfg.wallYmax, cfg.wallZmax };
    for (int axis = 0; axis < 3; ++axis) {
        auto* line = new QHBoxLayout;
        for (int k = 0; k < 2; ++k) {
            m_boxSpin[axis][k] = new QDoubleSpinBox(this);
            m_boxSpin[axis][k]->setRange(-100.0, 100.0);
            m_boxSpin[axis][k]->setDecimals(2);
            m_boxSpin[axis][k]->setSuffix(QStringLiteral(" Å"));
            line->addWidget(m_boxSpin[axis][k]);
        }
        // A wall configured as 0/0 means "auto-size" in curcuma, which we cannot
        // pre-compute here; fall back to a symmetric 12 A box.
        const bool haveBounds = (defMax[axis] - defMin[axis]) > 1e-6;
        m_boxSpin[axis][0]->setValue(haveBounds ? defMin[axis] : -6.0);
        m_boxSpin[axis][1]->setValue(haveBounds ? defMax[axis] : 6.0);
        auto* holder = new QWidget(this);
        holder->setLayout(line);
        line->setContentsMargins(0, 0, 0, 0);
        boxForm->addRow(tr("%1 min / max:").arg(QString::fromLatin1(axisName[axis])), holder);
    }
    m_shapeStack->addWidget(boxPage);

    form->addRow(QString(), m_shapeStack);
    connect(m_shapeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        m_shapeStack, &QStackedWidget::setCurrentIndex);
    if (cfg.wallEnabled && cfg.wallType == 2) {
        m_shapeCombo->setCurrentIndex(1);
        m_shapeStack->setCurrentIndex(1);
    }

    m_minDistSpin = new QDoubleSpinBox(this);
    m_minDistSpin->setRange(0.5, 10.0);
    m_minDistSpin->setSingleStep(0.1);
    m_minDistSpin->setDecimals(2);
    m_minDistSpin->setValue(2.2);
    m_minDistSpin->setSuffix(QStringLiteral(" Å"));
    m_minDistSpin->setToolTip(tr("Smallest allowed distance between atoms of different copies. "
                                 "Too small a value starts the run with molecules already pressed "
                                 "into each other's repulsion wall."));
    form->addRow(tr("Minimum distance:"), m_minDistSpin);

    m_seedSpin = new QSpinBox(this);
    m_seedSpin->setRange(0, 1000000);
    m_seedSpin->setValue(0);
    m_seedSpin->setToolTip(tr("0 draws a new random arrangement every time. Any other value "
                              "reproduces the same arrangement, which is what a lesson or a "
                              "repeatable experiment needs."));
    form->addRow(tr("Seed:"), m_seedSpin);

    m_applyWallCheck = new QCheckBox(tr("Use this container as the confinement wall"), this);
    m_applyWallCheck->setChecked(true);
    m_applyWallCheck->setToolTip(tr("Sets the simulation's confinement wall to the container, so the "
                                    "molecules stay in the volume they were packed into."));
    form->addRow(QString(), m_applyWallCheck);

    outer->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Fill"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);
}

void FillContainerDialog::addRow(const QString& fragmentName, int count)
{
    const int r = m_table->rowCount();
    m_table->insertRow(r);

    auto* combo = new QComboBox(m_table);
    const auto& lib = build::fragmentLibrary();
    QString lastCategory;
    for (int i = 0; i < lib.size(); ++i) {
        if (!isStandalone(lib[i]))
            continue;
        if (lib[i].category != lastCategory) {
            lastCategory = lib[i].category;
            combo->insertSeparator(combo->count());
        }
        combo->addItem(lib[i].name, i);
    }
    if (!fragmentName.isEmpty()) {
        const int idx = combo->findText(fragmentName, Qt::MatchFixedString);
        if (idx >= 0)
            combo->setCurrentIndex(idx);
    }
    m_table->setCellWidget(r, 0, combo);

    auto* spin = new QSpinBox(m_table);
    spin->setRange(0, 500);
    spin->setValue(count);
    m_table->setCellWidget(r, 1, spin);
}

QVector<build::FillRequest> FillContainerDialog::requests() const
{
    QVector<build::FillRequest> out;
    const auto& lib = build::fragmentLibrary();
    for (int r = 0; r < m_table->rowCount(); ++r) {
        auto* combo = qobject_cast<QComboBox*>(m_table->cellWidget(r, 0));
        auto* spin = qobject_cast<QSpinBox*>(m_table->cellWidget(r, 1));
        if (!combo || !spin || spin->value() <= 0)
            continue;
        const int libIndex = combo->currentData().toInt();
        if (libIndex >= 0 && libIndex < lib.size())
            out.append({ &lib[libIndex], spin->value() });
    }
    return out;
}

build::Container FillContainerDialog::container() const
{
    build::Container c;
    c.kind = static_cast<build::Container::Kind>(m_shapeCombo->currentData().toInt());
    c.radius = float(m_radiusSpin->value());
    c.min = QVector3D(float(m_boxSpin[0][0]->value()), float(m_boxSpin[1][0]->value()),
        float(m_boxSpin[2][0]->value()));
    c.max = QVector3D(float(m_boxSpin[0][1]->value()), float(m_boxSpin[1][1]->value()),
        float(m_boxSpin[2][1]->value()));
    return c;
}

float FillContainerDialog::minDistance() const { return float(m_minDistSpin->value()); }
quint32 FillContainerDialog::seed() const { return quint32(m_seedSpin->value()); }
bool FillContainerDialog::applyToWall() const { return m_applyWallCheck->isChecked(); }
