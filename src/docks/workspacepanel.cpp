// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// WorkspacePanel implementation.
//
// Claude Generated 2026 - Dock system restructuring.

#include "workspacepanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

WorkspacePanel::WorkspacePanel(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void WorkspacePanel::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    QWidget* header = new QWidget;
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* label = new QLabel(tr("Workspaces"));
    label->setStyleSheet(QStringLiteral("font-weight: bold;"));
    headerLayout->addWidget(label, 1);

    m_newButton = new QPushButton(QStringLiteral("+"));
    m_newButton->setMaximumWidth(30);
    m_newButton->setToolTip(tr("Save current state as workspace"));
    connect(m_newButton, &QPushButton::clicked,
            this, &WorkspacePanel::saveWorkspaceRequested);
    headerLayout->addWidget(m_newButton);
    layout->addWidget(header);

    m_listView = new QListWidget;
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_listView);
}

QListWidget* WorkspacePanel::listView() const
{
    return m_listView;
}

QPushButton* WorkspacePanel::newWorkspaceButton() const
{
    return m_newButton;
}
