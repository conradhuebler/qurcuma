// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// CollapsibleSection. Claude Generated.
#include "collapsiblesection.h"

#include <QToolButton>
#include <QVBoxLayout>

CollapsibleSection::CollapsibleSection(const QString& title, QWidget* parent)
    : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_header = new QToolButton(this);
    m_header->setText(title);
    m_header->setCheckable(true);
    m_header->setChecked(true);
    m_header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_header->setArrowType(Qt::DownArrow);
    m_header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_header->setStyleSheet(QStringLiteral(
        "QToolButton { border: none; font-weight: bold; padding: 3px 2px; text-align: left; }"));

    m_content = new QWidget(this);

    connect(m_header, &QToolButton::toggled, this, [this](bool on) {
        m_header->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
        m_content->setVisible(on);
    });

    outer->addWidget(m_header);
    outer->addWidget(m_content);
}

void CollapsibleSection::setContentLayout(QLayout* layout)
{
    layout->setContentsMargins(10, 2, 4, 6); // indent content under the header
    m_content->setLayout(layout);
}

void CollapsibleSection::setExpanded(bool expanded)
{
    m_header->setChecked(expanded);
}
