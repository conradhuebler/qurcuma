// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// AppearanceDock — the detailed display settings (DisplayPanel) and the saved
// camera views (ViewPresetWidget). Closed by default: the frequent switches are
// quick toggles in the viewer bar, the rest comes from Looks (Look menu); this
// dock is opened through Look ▸ Details… or View ▸ Dock Panels.
//
// Claude Generated 2026 - UX stage 4 (split off the former Structure & Display dock).

#pragma once

#include "dockconfig.h"

#include <QDockWidget>

class DisplayPanel;
class MoleculeViewer;
class Settings;
class ViewPresetWidget;

class AppearanceDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit AppearanceDock(MoleculeViewer* viewer, Settings* settings, QWidget* parent = nullptr);

    DisplayPanel* displayPanel() const { return m_displayPanel; }
    ViewPresetWidget* viewPresetWidget() const { return m_viewPresetWidget; }

private:
    DisplayPanel* m_displayPanel = nullptr;
    ViewPresetWidget* m_viewPresetWidget = nullptr;
};
