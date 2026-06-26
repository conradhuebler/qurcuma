// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// DisplayDock — right-side dock hosting the DisplayPanel (style/effects/lighting/tools/presets).
// Simple wrapper so MainWindow can obtain the panel without knowing the dock internals.
//
// Claude Generated 2026 - Dock system restructuring.

#pragma once

#include "dockconfig.h"

#include <QDockWidget>

class DisplayPanel;
class MoleculeViewer;
class Settings;

class DisplayDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit DisplayDock(MoleculeViewer* viewer, Settings* settings, QWidget* parent = nullptr);

    DisplayPanel* displayPanel() const;

private:
    void setupUI(MoleculeViewer* viewer, Settings* settings);

    DisplayPanel* m_displayPanel = nullptr;
};
