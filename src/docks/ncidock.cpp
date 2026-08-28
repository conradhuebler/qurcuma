// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// NciDock implementation.
//
// Claude Generated 2026 - NCI analysis.

#include "ncidock.h"

#include "../nciwidget.h"

NciDock::NciDock(QWidget* parent)
    : QDockWidget(DockConfig::NciDockTitle, parent)
{
    setObjectName(DockConfig::NciDockObjectName);
    setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);

    m_nci = new NciWidget(this);
    setWidget(m_nci);

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
