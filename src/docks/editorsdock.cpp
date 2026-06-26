// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// EditorsDock implementation.
//
// Claude Generated 2026 - Dock system restructuring.

#include "editorsdock.h"

#include "modifiabletextedit.h"
#include "rmsdwidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

EditorsDock::EditorsDock(QWidget* parent)
    : QDockWidget(DockConfig::EditorsDockTitle, parent)
{
    setObjectName(DockConfig::EditorsDockObjectName);
    setupUI();
}

void EditorsDock::setupUI()
{
    m_editorsTabs = new QTabWidget(this);
    m_editorsTabs->setTabPosition(QTabWidget::North);
    m_editorsTabs->setDocumentMode(true);

    // Structure tab
    QWidget* structureWidget = new QWidget;
    QVBoxLayout* structureLayout = new QVBoxLayout(structureWidget);
    structureLayout->setContentsMargins(4, 4, 4, 4);

    QHBoxLayout* structureFileLayout = new QHBoxLayout;
    structureFileLayout->addWidget(new QLabel(tr("Structure file:")));
    m_structureFileEdit = new QLineEdit(tr("input"));
    m_structureFileEdit->setToolTip(tr("Base name for structure file"));
    m_structureFileEditExtension = new QLineEdit(tr("xyz"));
    m_structureFileEditExtension->setMaximumWidth(60);
    structureFileLayout->addWidget(m_structureFileEdit);
    structureFileLayout->addWidget(m_structureFileEditExtension);

    QToolButton* applyStructBtn = new QToolButton;
    applyStructBtn->setText(tr("Apply → Viewer"));
    applyStructBtn->setToolTip(tr("Parse the editor text as XYZ and replace the current structure"));
    connect(applyStructBtn, &QToolButton::clicked, this, &EditorsDock::structureApplyRequested);
    structureFileLayout->addWidget(applyStructBtn);
    structureLayout->addLayout(structureFileLayout);

    m_structureView = new ModifiableTextEdit;
    m_structureView->setPlaceholderText(tr("Structure data"));
    structureLayout->addWidget(m_structureView);
    m_editorsTabs->addTab(structureWidget, tr("Structure"));

    // Input tab
    QWidget* inputWidget = new QWidget;
    QVBoxLayout* inputLayout = new QVBoxLayout(inputWidget);
    inputLayout->setContentsMargins(4, 4, 4, 4);

    QHBoxLayout* inputFileLayout = new QHBoxLayout;
    inputFileLayout->addWidget(new QLabel(tr("Input file:")));
    m_inputFileEdit = new QLineEdit(tr("input"));
    m_inputFileEdit->setToolTip(tr("Base name for input file"));
    m_inputFileEditExtension = new QLineEdit;
    m_inputFileEditExtension->setMaximumWidth(60);
    inputFileLayout->addWidget(m_inputFileEdit);
    inputFileLayout->addWidget(m_inputFileEditExtension);
    inputLayout->addLayout(inputFileLayout);

    m_inputView = new ModifiableTextEdit;
    m_inputView->setPlaceholderText(tr("Input data"));
    inputLayout->addWidget(m_inputView);
    m_editorsTabs->addTab(inputWidget, tr("Input"));

    // RMSD / Align tab
    m_rmsdWidget = new RMSDWidget(this);
    m_editorsTabs->addTab(m_rmsdWidget, tr("RMSD / Align"));

    setWidget(m_editorsTabs);
}

QTabWidget* EditorsDock::editorsTabs() const
{
    return m_editorsTabs;
}

ModifiableTextEdit* EditorsDock::structureView() const
{
    return m_structureView;
}

ModifiableTextEdit* EditorsDock::inputView() const
{
    return m_inputView;
}

RMSDWidget* EditorsDock::rmsdWidget() const
{
    return m_rmsdWidget;
}

QLineEdit* EditorsDock::structureFileEdit() const { return m_structureFileEdit; }
QLineEdit* EditorsDock::structureFileEditExtension() const { return m_structureFileEditExtension; }
QLineEdit* EditorsDock::inputFileEdit() const { return m_inputFileEdit; }
QLineEdit* EditorsDock::inputFileEditExtension() const { return m_inputFileEditExtension; }

void EditorsDock::setCurrentTab(int index)
{
    if (m_editorsTabs && index >= 0 && index < m_editorsTabs->count())
        m_editorsTabs->setCurrentIndex(index);
}
