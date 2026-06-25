// snapshotswidget.cpp - Manual snapshot history for molecular geometries
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - Snapshot history foundation

#include "snapshotswidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QVBoxLayout>

SnapshotsWidget::SnapshotsWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

SnapshotsWidget::~SnapshotsWidget() = default;

void SnapshotsWidget::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* header = new QLabel(tr("Geometry Snapshots"), this);
    header->setStyleSheet("font-weight: bold;");
    layout->addWidget(header);

    m_listWidget = new QListWidget(this);
    m_listWidget->setToolTip(tr("Double-click a snapshot to restore it."));
    layout->addWidget(m_listWidget, 1);

    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(2);

    auto makeToolButton = [this](const QString& text, const QString& tooltip) {
        auto* b = new QToolButton(this);
        b->setText(text);
        b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        b->setFixedSize(28, 26);
        b->setToolTip(tooltip);
        return b;
    };

    m_takeBtn = makeToolButton(QStringLiteral("+"), tr("Take snapshot of current geometry"));
    m_restoreBtn = makeToolButton(QStringLiteral("↩"), tr("Restore selected snapshot"));
    m_deleteBtn = makeToolButton(QStringLiteral("🗑"), tr("Delete selected snapshot"));

    btnRow->addWidget(m_takeBtn);
    btnRow->addWidget(m_restoreBtn);
    btnRow->addWidget(m_deleteBtn);
    btnRow->addStretch();

    layout->addLayout(btnRow);

    connect(m_takeBtn, &QToolButton::clicked, this, &SnapshotsWidget::onTakeClicked);
    connect(m_restoreBtn, &QToolButton::clicked, this, &SnapshotsWidget::onRestoreClicked);
    connect(m_deleteBtn, &QToolButton::clicked, this, &SnapshotsWidget::onDeleteClicked);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &SnapshotsWidget::onItemDoubleClicked);
    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, &SnapshotsWidget::updateButtonStates);

    updateButtonStates();
}

void SnapshotsWidget::updateButtonStates()
{
    const bool hasSelection = m_listWidget && m_listWidget->currentRow() >= 0;
    if (m_restoreBtn) m_restoreBtn->setEnabled(hasSelection);
    if (m_deleteBtn) m_deleteBtn->setEnabled(hasSelection);
}

void SnapshotsWidget::addSnapshot(const MoleculeSnapshot& snapshot)
{
    m_snapshots.append(snapshot);
    auto* item = new QListWidgetItem(
        tr("%1 | %2 atoms | %3")
            .arg(snapshot.name)
            .arg(snapshot.atoms.size())
            .arg(snapshot.timestamp.toString("hh:mm:ss")),
        m_listWidget);
    item->setToolTip(QLocale::system().toString(snapshot.timestamp, QLocale::ShortFormat));
    m_listWidget->setCurrentItem(item);
    m_listWidget->scrollToItem(item);
    updateButtonStates();
}

void SnapshotsWidget::clearSnapshots()
{
    m_snapshots.clear();
    if (m_listWidget) m_listWidget->clear();
    updateButtonStates();
}

int SnapshotsWidget::count() const
{
    return m_snapshots.size();
}

MoleculeSnapshot SnapshotsWidget::snapshotAt(int index) const
{
    if (index >= 0 && index < m_snapshots.size())
        return m_snapshots[index];
    return {};
}

void SnapshotsWidget::onTakeClicked()
{
    emit takeSnapshotRequested();
}

void SnapshotsWidget::onRestoreClicked()
{
    const int row = m_listWidget ? m_listWidget->currentRow() : -1;
    if (row < 0 || row >= m_snapshots.size())
        return;
    emit restoreSnapshotRequested(row);
}

void SnapshotsWidget::onDeleteClicked()
{
    const int row = m_listWidget ? m_listWidget->currentRow() : -1;
    if (row < 0 || row >= m_snapshots.size())
        return;

    auto* item = m_listWidget->takeItem(row);
    delete item;
    m_snapshots.removeAt(row);
    updateButtonStates();

    emit deleteSnapshotRequested(row);
}

void SnapshotsWidget::onItemDoubleClicked(QListWidgetItem* item)
{
    if (!item)
        return;
    const int row = m_listWidget->row(item);
    if (row < 0 || row >= m_snapshots.size())
        return;
    emit restoreSnapshotRequested(row);
}