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
#include <QDir>
#include <QFileInfo>
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

void VrSession::setStatusText(const QString& text)
{
    if (text == m_statusText)
        return;
    m_statusText = text;
    emit statusTextChanged();
}

void VrSession::setSimulationActive(bool on)
{
    if (on == m_simulationActive)
        return;
    m_simulationActive = on;
    emit simulationActiveChanged();
}

void VrSession::setTemperature(double kelvin)
{
    if (qFuzzyCompare(1.0 + kelvin, 1.0 + m_temperature))
        return;
    m_temperature = kelvin;
    emit temperatureChanged();
}

// Claude Generated 2026 - Structured status for the headset panels (VrPanelStructure/VrPanelSimulation).
void VrSession::setStatus(int frame, int frameCount, bool playing, const QString& selection, double grabStrength)
{
    if (frame == m_frame && frameCount == m_frameCount && playing == m_playing && selection == m_selectionText
        && qFuzzyCompare(1.0 + grabStrength, 1.0 + m_grabStrength))
        return;
    m_frame = frame;
    m_frameCount = frameCount;
    m_playing = playing;
    m_selectionText = selection;
    m_grabStrength = grabStrength;
    emit statusChanged();
}

void VrSession::setSimulationStatus(bool paused, const QString& message)
{
    if (paused == m_simulationPaused && message == m_simulationMessage)
        return;
    m_simulationPaused = paused;
    m_simulationMessage = message;
    emit simulationStatusChanged();
}

// Claude Generated 2026 - Headset file browser: local directories plus the structure formats
// MoleculeFileLoader reads (xyz, vtf, pdb, mol2). Hidden entries are skipped.
void VrSession::refreshEntries()
{
    static const QStringList filters = { "*.xyz", "*.vtf", "*.pdb", "*.mol2" };
    QDir dir(m_currentDir);
    m_entries.clear();
    for (const QFileInfo& d : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase))
        m_entries.append(QVariantMap { { "name", d.fileName() }, { "isDir", true } });
    for (const QFileInfo& f : dir.entryInfoList(filters, QDir::Files, QDir::Name | QDir::IgnoreCase))
        m_entries.append(QVariantMap { { "name", f.fileName() }, { "isDir", false } });
    emit currentDirChanged();
}

void VrSession::setStartDirectory(const QString& dir)
{
    m_currentDir = QDir(dir).exists() ? QDir(dir).absolutePath() : QDir::homePath();
    refreshEntries();
}

void VrSession::enter(const QString& name)
{
    const QString target = QDir(m_currentDir).absoluteFilePath(name);
    if (QFileInfo(target).isDir()) {
        m_currentDir = target;
        refreshEntries();
    }
}

void VrSession::up()
{
    QDir dir(m_currentDir);
    if (dir.cdUp()) {
        m_currentDir = dir.absolutePath();
        refreshEntries();
    }
}

void VrSession::choose(const QString& name)
{
    const QString path = QDir(m_currentDir).absoluteFilePath(name);
    if (QFileInfo(path).isFile())
        emit fileRequested(path);
}

void VrSession::setMethod(const QString& key)
{
    if (key == m_method)
        return;
    m_method = key;
    emit methodChanged();
}
