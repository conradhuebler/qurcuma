// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// ElementQuickBar + PeriodicTableWidget. Claude Generated 2026.
#include "elementpicker.h"

#include "../elementdata.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QPoint>
#include <QToolButton>

namespace {

// Standard 18-column periodic-table position (row, column) for Z = 1..118;
// lanthanides (57-71) and actinides (89-103) go to two separate rows below.
QPoint tablePosition(int z)
{
    if (z == 1) return { 0, 0 };
    if (z == 2) return { 0, 17 };
    if (z <= 4) return { 1, z - 3 };        // Li Be
    if (z <= 10) return { 1, z + 7 };       // B..Ne -> cols 12..17
    if (z <= 12) return { 2, z - 11 };      // Na Mg
    if (z <= 18) return { 2, z - 1 };       // Al..Ar -> cols 12..17
    if (z <= 36) return { 3, z - 19 };
    if (z <= 54) return { 4, z - 37 };
    if (z <= 56) return { 5, z - 55 };      // Cs Ba
    if (z <= 71) return { 8, z - 57 + 3 };  // La..Lu (lanthanide row)
    if (z <= 86) return { 5, z - 72 + 3 };  // Hf..Rn
    if (z <= 88) return { 6, z - 87 };      // Fr Ra
    if (z <= 103) return { 9, z - 89 + 3 }; // Ac..Lr (actinide row)
    return { 6, z - 104 + 3 };              // Rf..Og
}

// CPK-tinted flat button with readable text on light and dark swatches.
void tintButton(QToolButton* b, const QString& symbol)
{
    const QColor c = elem::cpkColor(symbol);
    const bool dark = c.lightness() < 110;
    b->setStyleSheet(QStringLiteral(
        "QToolButton { background-color: %1; color: %2; border: 1px solid palette(mid); }"
        "QToolButton:checked { border: 2px solid palette(highlight); font-weight: bold; }")
            .arg(c.name(QColor::HexRgb), dark ? QStringLiteral("#ffffff") : QStringLiteral("#000000")));
}

} // namespace

PeriodicTableWidget::PeriodicTableWidget(QWidget* parent)
    : QWidget(parent, Qt::Popup)
{
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(4, 4, 4, 4);
    grid->setSpacing(1);
    for (int z = 1; z <= 118; ++z) {
        const QString symbol = elem::symbolForZ(z);
        if (symbol.isEmpty())
            continue;
        auto* b = new QToolButton(this);
        b->setText(symbol);
        b->setFixedSize(26, 22);
        b->setToolTip(QStringLiteral("%1 (Z = %2)").arg(symbol).arg(z));
        tintButton(b, symbol);
        const QPoint pos = tablePosition(z);
        grid->addWidget(b, pos.x(), pos.y());
        connect(b, &QToolButton::clicked, this, [this, symbol]() {
            emit elementPicked(symbol);
            hide();
        });
    }
    grid->setRowMinimumHeight(7, 6);  // gap before the lanthanide/actinide rows
}

ElementQuickBar::ElementQuickBar(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(1);

    // Exclusivity is managed in setCurrentElement (a QButtonGroup would refuse
    // to uncheck all buttons when an exotic element comes from the table popup).
    const QStringList quick = { "H", "C", "N", "O", "S", "P", "F", "Cl", "Br" };
    for (const QString& symbol : quick) {
        auto* b = new QToolButton(this);
        b->setText(symbol);
        b->setCheckable(true);
        b->setFixedSize(26, 22);
        b->setToolTip(tr("Build with %1").arg(symbol));
        tintButton(b, symbol);
        layout->addWidget(b);
        m_buttons.insert(symbol, b);
        connect(b, &QToolButton::clicked, this, [this, symbol]() {
            setCurrentElement(symbol);
            emit elementPicked(symbol);
        });
    }
    if (auto* carbon = m_buttons.value(QStringLiteral("C")))
        carbon->setChecked(true);

    m_table = new PeriodicTableWidget(this);
    connect(m_table, &PeriodicTableWidget::elementPicked, this, [this](const QString& symbol) {
        setCurrentElement(symbol);
        emit elementPicked(symbol);
    });

    auto* more = new QToolButton(this);
    more->setText(QStringLiteral("…"));
    more->setFixedSize(26, 22);
    more->setToolTip(tr("All elements (periodic table)"));
    layout->addWidget(more);
    connect(more, &QToolButton::clicked, this, [this, more]() {
        m_table->move(more->mapToGlobal(QPoint(0, more->height())));
        m_table->show();
    });
}

void ElementQuickBar::setCurrentElement(const QString& symbol)
{
    // Highlight the matching quick button; an exotic element unchecks them all.
    bool found = false;
    for (auto it = m_buttons.constBegin(); it != m_buttons.constEnd(); ++it) {
        const bool match = (it.key() == symbol);
        it.value()->setChecked(match);
        found = found || match;
    }
    if (!found)
        for (QToolButton* b : m_buttons)
            b->setChecked(false);
}
