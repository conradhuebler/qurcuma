// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Instance tables of the desktop SceneController for the headset scene, see xrscenebridge.h.
// Claude Generated 2026.
#include "xrscenebridge.h"

#include "scenecontroller.h"
#include "tableinstancing.h"

XrSceneBridge::XrSceneBridge(SceneController* scene, QObject* parent)
    : QObject(parent)
{
    m_atoms = mirror(scene->atomInstancing());
    m_bonds = mirror(scene->bondInstancing());
    m_arrowShaft = mirror(scene->arrowShaftInstancing());
    m_arrowTip = mirror(scene->arrowTipInstancing());
    m_measureLines = mirror(scene->measureLineInstancing());
    m_overlayAtoms = mirror(scene->overlayAtomInstancing());
    m_overlayBonds = mirror(scene->overlayBondInstancing());
    m_wallLines = mirror(scene->wallInstancing());
    m_potShells = mirror(scene->wallPotShellsInstancing());
    m_wallForceShafts = mirror(scene->wallForceShaftsInstancing());
    m_wallForceTips = mirror(scene->wallForceTipsInstancing());
    m_nciLines = mirror(scene->nciInstancing());
}

// Same ownership pattern as SceneController: a QQuick3DObject without a scene parent (the
// headset scene adopts it when a Model uses it), lifetime handled by the QObject parent.
MirrorInstancing* XrSceneBridge::mirror(QQuick3DInstancing* source)
{
    auto* m = new MirrorInstancing(qobject_cast<TableInstancing*>(source), nullptr);
    m->setParent(this);
    return m;
}

QQuick3DInstancing* XrSceneBridge::atomInstancing() const { return m_atoms; }
QQuick3DInstancing* XrSceneBridge::bondInstancing() const { return m_bonds; }
QQuick3DInstancing* XrSceneBridge::arrowShaftInstancing() const { return m_arrowShaft; }
QQuick3DInstancing* XrSceneBridge::arrowTipInstancing() const { return m_arrowTip; }
QQuick3DInstancing* XrSceneBridge::measureLineInstancing() const { return m_measureLines; }
QQuick3DInstancing* XrSceneBridge::overlayAtomInstancing() const { return m_overlayAtoms; }
QQuick3DInstancing* XrSceneBridge::overlayBondInstancing() const { return m_overlayBonds; }
QQuick3DInstancing* XrSceneBridge::wallInstancing() const { return m_wallLines; }
QQuick3DInstancing* XrSceneBridge::wallPotShellsInstancing() const { return m_potShells; }
QQuick3DInstancing* XrSceneBridge::wallForceShaftsInstancing() const { return m_wallForceShafts; }
QQuick3DInstancing* XrSceneBridge::wallForceTipsInstancing() const { return m_wallForceTips; }
QQuick3DInstancing* XrSceneBridge::nciInstancing() const { return m_nciLines; }
