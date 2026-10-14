// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Instance tables of the desktop SceneController for the headset scene. Claude Generated 2026.
//
// Exposes the SAME property names as SceneController's instancing properties, each one a
// MirrorInstancing of the controller's table (tableinstancing.h). MoleculeModels.qml takes
// an "inst" object for the tables and a "ctl" object for everything else: the desktop passes
// the controller twice, the headset passes (controller, bridge). test_instancing_mirror checks
// that every QQuick3DInstancing* property of SceneController has its mirror here.
#pragma once

#include <QObject>

class QQuick3DInstancing;
class MirrorInstancing;
class SceneController;

class XrSceneBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuick3DInstancing* atomInstancing READ atomInstancing CONSTANT)
    Q_PROPERTY(QQuick3DInstancing* bondInstancing READ bondInstancing CONSTANT)
    Q_PROPERTY(QQuick3DInstancing* arrowShaftInstancing READ arrowShaftInstancing CONSTANT)
    Q_PROPERTY(QQuick3DInstancing* arrowTipInstancing READ arrowTipInstancing CONSTANT)
    Q_PROPERTY(QQuick3DInstancing* measureLineInstancing READ measureLineInstancing CONSTANT)
    Q_PROPERTY(QQuick3DInstancing* overlayAtomInstancing READ overlayAtomInstancing CONSTANT)
    Q_PROPERTY(QQuick3DInstancing* overlayBondInstancing READ overlayBondInstancing CONSTANT)
    Q_PROPERTY(QQuick3DInstancing* wallInstancing READ wallInstancing CONSTANT)
    Q_PROPERTY(QQuick3DInstancing* wallPotShellsInstancing READ wallPotShellsInstancing CONSTANT)
    Q_PROPERTY(QQuick3DInstancing* wallForceShaftsInstancing READ wallForceShaftsInstancing CONSTANT)
    Q_PROPERTY(QQuick3DInstancing* wallForceTipsInstancing READ wallForceTipsInstancing CONSTANT)
    Q_PROPERTY(QQuick3DInstancing* nciInstancing READ nciInstancing CONSTANT)

public:
    /// Creates one mirror per instance table of @p scene; the mirrors are children of the bridge.
    explicit XrSceneBridge(SceneController* scene, QObject* parent = nullptr);

    QQuick3DInstancing* atomInstancing() const;
    QQuick3DInstancing* bondInstancing() const;
    QQuick3DInstancing* arrowShaftInstancing() const;
    QQuick3DInstancing* arrowTipInstancing() const;
    QQuick3DInstancing* measureLineInstancing() const;
    QQuick3DInstancing* overlayAtomInstancing() const;
    QQuick3DInstancing* overlayBondInstancing() const;
    QQuick3DInstancing* wallInstancing() const;
    QQuick3DInstancing* wallPotShellsInstancing() const;
    QQuick3DInstancing* wallForceShaftsInstancing() const;
    QQuick3DInstancing* wallForceTipsInstancing() const;
    QQuick3DInstancing* nciInstancing() const;

private:
    MirrorInstancing* mirror(QQuick3DInstancing* source);

    MirrorInstancing* m_atoms = nullptr;
    MirrorInstancing* m_bonds = nullptr;
    MirrorInstancing* m_arrowShaft = nullptr;
    MirrorInstancing* m_arrowTip = nullptr;
    MirrorInstancing* m_measureLines = nullptr;
    MirrorInstancing* m_overlayAtoms = nullptr;
    MirrorInstancing* m_overlayBonds = nullptr;
    MirrorInstancing* m_wallLines = nullptr;
    MirrorInstancing* m_potShells = nullptr;
    MirrorInstancing* m_wallForceShafts = nullptr;
    MirrorInstancing* m_wallForceTips = nullptr;
    MirrorInstancing* m_nciLines = nullptr;
};
