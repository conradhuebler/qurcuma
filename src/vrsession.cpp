/*
 * <Qurcuma VR session: mirrors the viewer scene into an OpenXR headset.>
 * Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "vrsession.h"

#include "scenecontroller.h"

#include <QQmlComponent>
#include <QQmlContext>

VrSession::VrSession(QObject* parent)
    : QObject(parent)
{
}

VrSession::~VrSession()
{
    stop();
}

bool VrSession::start()
{
    if (m_engine)
        return true;

    m_scene = new SceneController(this);
    m_engine = std::make_unique<QQmlEngine>();
    m_engine->rootContext()->setContextProperty(QStringLiteral("controller"), m_scene);
    m_engine->rootContext()->setContextProperty(QStringLiteral("vr"), this);

    QQmlComponent component(m_engine.get(), QUrl(QStringLiteral("qrc:/qml/src/qml/viewerxr.qml")));
    m_root = component.create();
    if (!m_root) {
        qWarning() << "viewerxr.qml:" << component.errors();
        stop();
        return false;
    }
    m_root->setParent(m_engine.get());
    return true;
}

void VrSession::stop()
{
    // Drop the QML root before the scene controller so its bindings go first.
    delete m_root;
    m_root = nullptr;
    m_engine.reset();
    delete m_scene;
    m_scene = nullptr;
}
