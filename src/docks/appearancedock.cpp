// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// AppearanceDock implementation. Claude Generated 2026 - UX stage 4.

#include "appearancedock.h"

#include "displaypanel.h"
#include "viewpresetwidget.h"

#include <QSettings>
#include <QSplitter>

AppearanceDock::AppearanceDock(MoleculeViewer* viewer, Settings* settings, QWidget* parent)
    : QDockWidget(DockConfig::AppearanceDockTitle, parent)
{
    setObjectName(DockConfig::AppearanceDockObjectName);

    m_displayPanel = new DisplayPanel(viewer, settings, this);
    m_viewPresetWidget = new ViewPresetWidget(viewer, settings, this);

    // Settings on top, saved camera views below; the split survives restarts.
    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(m_displayPanel);
    splitter->addWidget(m_viewPresetWidget);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    const QString key = QStringLiteral("ui/appearanceDock/splitter");
    const QByteArray state = QSettings().value(key).toByteArray();
    if (!state.isEmpty())
        splitter->restoreState(state);
    connect(splitter, &QSplitter::splitterMoved, this, [splitter, key]() {
        QSettings().setValue(key, splitter->saveState());
    });
    setWidget(splitter);
}
