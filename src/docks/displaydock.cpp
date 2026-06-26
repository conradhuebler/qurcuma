// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// DisplayDock implementation.
//
// Claude Generated 2026 - Dock system restructuring.

#include "displaydock.h"

#include "displaypanel.h"
#include "settings.h"
#include "view.h"

DisplayDock::DisplayDock(MoleculeViewer* viewer, Settings* settings, QWidget* parent)
    : QDockWidget(DockConfig::DisplayDockTitle, parent)
{
    setObjectName(DockConfig::DisplayDockObjectName);
    setupUI(viewer, settings);
}

void DisplayDock::setupUI(MoleculeViewer* viewer, Settings* settings)
{
    m_displayPanel = new DisplayPanel(viewer, settings, this);
    setWidget(m_displayPanel);
}

DisplayPanel* DisplayDock::displayPanel() const
{
    return m_displayPanel;
}
