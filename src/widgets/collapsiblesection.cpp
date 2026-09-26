// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// CollapsibleSection. Claude Generated.
#include "collapsiblesection.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
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
        emit expandedChanged(on);
    });

    m_headerRow = new QHBoxLayout;
    m_headerRow->setContentsMargins(0, 0, 0, 0);
    m_headerRow->setSpacing(2);
    m_headerRow->addWidget(m_header);
    outer->addLayout(m_headerRow);
    outer->addWidget(m_content);
}

QCheckBox* CollapsibleSection::addSwitch(const QString& toolTip)
{
    if (m_switch)
        return m_switch;
    m_switch = new QCheckBox(this);
    m_switch->setToolTip(toolTip);
    m_headerRow->insertWidget(0, m_switch);
    connect(m_switch, &QCheckBox::toggled, this, [this](bool on) {
        applySwitch(on);
        emit switchToggled(on);
    });
    applySwitch(false);
    return m_switch;
}

void CollapsibleSection::setSwitchedOn(bool on)
{
    if (!m_switch)
        return;
    {
        const QSignalBlocker blocker(m_switch);
        m_switch->setChecked(on);
    }
    applySwitch(on);
}

bool CollapsibleSection::isSwitchedOn() const
{
    return m_switch && m_switch->isChecked();
}

void CollapsibleSection::applySwitch(bool on)
{
    m_content->setEnabled(on);
    setExpanded(on);
}

void CollapsibleSection::setContentLayout(QLayout* layout)
{
    layout->setContentsMargins(10, 2, 4, 6); // indent content under the header
    m_content->setLayout(layout);
}

void CollapsibleSection::setTitle(const QString& title)
{
    m_header->setText(title);
}

void CollapsibleSection::setExpanded(bool expanded)
{
    m_header->setChecked(expanded);
}

bool CollapsibleSection::isExpanded() const
{
    return m_header->isChecked();
}
