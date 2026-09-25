// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// StructureDock implementation.
//
// Claude Generated 2026 - Dock system restructuring.

#include "structuredock.h"

#include "atomlistpanel.h"
#include "modifiabletextedit.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

StructureDock::StructureDock(QWidget* parent)
    : QDockWidget(DockConfig::StructureDockTitle, parent)
{
    setObjectName(DockConfig::StructureDockObjectName);
    setupUI();
}

void StructureDock::setupUI()
{
    QWidget* central = new QWidget;
    QVBoxLayout* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(4, 4, 4, 4);
    centralLayout->setSpacing(4);

    // Top segment toggle: [Structure | Atoms]
    QWidget* segmentBar = new QWidget;
    QHBoxLayout* segmentLayout = new QHBoxLayout(segmentBar);
    segmentLayout->setContentsMargins(0, 0, 0, 0);
    segmentLayout->setSpacing(0);

    QButtonGroup* segmentGroup = new QButtonGroup(this);
    segmentGroup->setExclusive(true);

    m_structureSegmentBtn = new QToolButton;
    m_structureSegmentBtn->setText(tr("Structure"));
    m_structureSegmentBtn->setCheckable(true);
    m_structureSegmentBtn->setChecked(true);
    m_structureSegmentBtn->setToolTip(tr("XYZ structure editor with Apply → Viewer"));
    segmentGroup->addButton(m_structureSegmentBtn);

    m_atomsSegmentBtn = new QToolButton;
    m_atomsSegmentBtn->setText(tr("Atoms"));
    m_atomsSegmentBtn->setCheckable(true);
    m_atomsSegmentBtn->setToolTip(tr("Editable atom table synchronized with the viewer"));
    segmentGroup->addButton(m_atomsSegmentBtn);

    segmentLayout->addWidget(m_structureSegmentBtn);
    segmentLayout->addWidget(m_atomsSegmentBtn);
    segmentLayout->addStretch();

    segmentBar->setStyleSheet(QStringLiteral(
        "QToolButton { padding: 2px 10px; border: 1px solid palette(mid); }"
        "QToolButton:checked { background: palette(highlight); color: palette(highlighted-text);"
        " font-weight: bold; }"));

    centralLayout->addWidget(segmentBar);

    // Top stacked pages
    m_topStack = new QStackedWidget;
    m_topStack->addWidget(createStructurePage());
    m_topStack->addWidget(createAtomsPage());

    centralLayout->addWidget(m_topStack, 1);

    connect(m_structureSegmentBtn, &QToolButton::clicked,
            this, [this]() { setCurrentTopSegment(TopSegment::Structure); });
    connect(m_atomsSegmentBtn, &QToolButton::clicked,
            this, [this]() { setCurrentTopSegment(TopSegment::Atoms); });

    setWidget(central);
}

QWidget* StructureDock::createStructurePage()
{
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* fileLayout = new QHBoxLayout;
    fileLayout->addWidget(new QLabel(tr("Structure file:")));
    m_structureFileEdit = new QLineEdit(tr("input"));
    m_structureFileEdit->setToolTip(tr("Base name for structure file"));
    m_structureFileEditExtension = new QLineEdit(tr("xyz"));
    m_structureFileEditExtension->setMaximumWidth(60);
    fileLayout->addWidget(m_structureFileEdit);
    fileLayout->addWidget(m_structureFileEditExtension);

    QToolButton* applyBtn = new QToolButton;
    applyBtn->setText(tr("Apply → Viewer"));
    applyBtn->setToolTip(tr("Parse the editor text as XYZ and replace the current structure"));
    connect(applyBtn, &QToolButton::clicked, this, &StructureDock::structureApplyRequested);
    fileLayout->addWidget(applyBtn);

    layout->addLayout(fileLayout);

    m_structureView = new ModifiableTextEdit;
    m_structureView->setPlaceholderText(tr("Structure data"));
    layout->addWidget(m_structureView);

    return page;
}

QWidget* StructureDock::createAtomsPage()
{
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    m_atomListPanel = new AtomListPanel(this);
    layout->addWidget(m_atomListPanel);

    return page;
}

StructureDock::TopSegment StructureDock::currentTopSegment() const
{
    if (!m_topStack)
        return TopSegment::Structure;
    return (m_topStack->currentIndex() == 1) ? TopSegment::Atoms : TopSegment::Structure;
}

void StructureDock::setCurrentTopSegment(TopSegment segment)
{
    if (!m_topStack)
        return;
    const int index = (segment == TopSegment::Atoms) ? 1 : 0;
    m_topStack->setCurrentIndex(index);
    if (m_structureSegmentBtn)
        m_structureSegmentBtn->setChecked(segment == TopSegment::Structure);
    if (m_atomsSegmentBtn)
        m_atomsSegmentBtn->setChecked(segment == TopSegment::Atoms);
}

QToolButton* StructureDock::structureSegmentButton() const { return m_structureSegmentBtn; }
QToolButton* StructureDock::atomsSegmentButton() const { return m_atomsSegmentBtn; }

ModifiableTextEdit* StructureDock::structureView() const { return m_structureView; }
QLineEdit* StructureDock::structureFileEdit() const { return m_structureFileEdit; }
QLineEdit* StructureDock::structureFileEditExtension() const { return m_structureFileEditExtension; }

AtomListPanel* StructureDock::atomListPanel() const { return m_atomListPanel; }
