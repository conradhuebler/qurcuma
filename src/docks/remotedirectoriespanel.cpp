// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// RemoteDirectoriesPanel implementation.
//
// Claude Generated 2026 - Dock system restructuring.

#include "remotedirectoriespanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

RemoteDirectoriesPanel::RemoteDirectoriesPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void RemoteDirectoriesPanel::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    QWidget* header = new QWidget;
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* label = new QLabel(tr("Remote Directories"));
    label->setStyleSheet(QStringLiteral("font-weight: bold;"));
    headerLayout->addWidget(label, 1);

    m_addButton = new QPushButton(QStringLiteral("+"));
    m_addButton->setMaximumWidth(30);
    m_addButton->setToolTip(tr("Add remote directory"));
    connect(m_addButton, &QPushButton::clicked,
            this, &RemoteDirectoriesPanel::addRemoteRequested);
    headerLayout->addWidget(m_addButton);
    layout->addWidget(header);

    m_treeView = new QTreeWidget;
    m_treeView->setHeaderHidden(true);
    layout->addWidget(m_treeView);
}

QTreeWidget* RemoteDirectoriesPanel::treeView() const
{
    return m_treeView;
}

QPushButton* RemoteDirectoriesPanel::addRemoteButton() const
{
    return m_addButton;
}
