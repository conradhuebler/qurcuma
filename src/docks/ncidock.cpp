// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// NciDock implementation.
//
// Claude Generated 2026 - NCI analysis.

#include "ncidock.h"

#include "../nciwidget.h"
#include "../widgets/collapsiblesection.h"

#include <QSettings>
#include <QVBoxLayout>

NciDock::NciDock(QWidget* parent)
    : QDockWidget(DockConfig::NciDockTitle, parent)
{
    setObjectName(DockConfig::NciDockObjectName);
    setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);

    m_nci = new NciWidget(this);
    // Claude Generated 2026 - Options above the table, collapsible (expand state persisted);
    // hidden until setOptionsWidget() provides them.
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    m_optionsSection = new CollapsibleSection(tr("Options"), page);
    m_optionsSection->setVisible(false);
    const QString expandKey = QStringLiteral("ui/nciDock/optionsExpanded");
    m_optionsSection->setExpanded(QSettings().value(expandKey, false).toBool());
    connect(m_optionsSection, &CollapsibleSection::expandedChanged, this,
        [expandKey](bool on) { QSettings().setValue(expandKey, on); });
    layout->addWidget(m_optionsSection);
    layout->addWidget(m_nci, 1);
    setWidget(page);

    connect(m_nci, &NciWidget::sourceChanged, this, &NciDock::sourceChanged);
    connect(m_nci, &NciWidget::analysisRequested, this, &NciDock::analysisRequested);
    connect(m_nci, &NciWidget::contactSelected, this, &NciDock::contactSelected);
    connect(m_nci, &NciWidget::contactFocused, this, &NciDock::contactFocused);
}

void NciDock::setResult(const nci::Result& result, const QVector<MoleculeViewer::Atom>& atoms)
{
    m_nci->setResult(result, atoms);
}

void NciDock::setKindPalette(const nci::Palette& palette)
{
    m_nci->setKindPalette(palette);
}

void NciDock::setSource(int source)
{
    m_nci->setSource(source);
}

void NciDock::setBusy(bool busy)
{
    m_nci->setBusy(busy);
}

void NciDock::setStatus(const QString& text)
{
    m_nci->setStatus(text);
}

void NciDock::setOptionsWidget(QWidget* options)
{
    if (!m_optionsSection || !options)
        return;
    auto* layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(options);
    m_optionsSection->setContentLayout(layout);
    m_optionsSection->setVisible(true);
}

void NciDock::expandOptions()
{
    if (m_optionsSection)
        m_optionsSection->setExpanded(true);
}
