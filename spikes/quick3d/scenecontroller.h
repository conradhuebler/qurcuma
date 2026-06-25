// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// WP0 spike - central model shared between the QWidget shell (side panel: effect
// toggles, dataset, FPS) and the QML scene (View3D bindings). Owns the atom/bond
// instancing objects and drives the MD-proxy animation and picking selection.
// Claude Generated.
#pragma once

#include "moleculedata.h"

#include <QColor>
#include <QObject>
#include <QTimer>
#include <QVector3D>

class AtomInstancing;
class BondInstancing;
class QQuick3DInstancing;

class SceneController : public QObject
{
    Q_OBJECT
    // Instancing objects bound into QML: Model { instancing: controller.atomInstancing }
    Q_PROPERTY(QQuick3DInstancing* atomInstancing READ atomInstancing CONSTANT)
    Q_PROPERTY(QQuick3DInstancing* bondInstancing READ bondInstancing CONSTANT)

    // Effect toggles (driven by the side-panel checkboxes, read by ExtendedSceneEnvironment).
    Q_PROPERTY(bool ssaoEnabled READ ssaoEnabled WRITE setSsaoEnabled NOTIFY effectsChanged)
    Q_PROPERTY(bool shadowsEnabled READ shadowsEnabled WRITE setShadowsEnabled NOTIFY effectsChanged)
    Q_PROPERTY(bool bloomEnabled READ bloomEnabled WRITE setBloomEnabled NOTIFY effectsChanged)
    Q_PROPERTY(bool tonemapEnabled READ tonemapEnabled WRITE setTonemapEnabled NOTIFY effectsChanged)
    Q_PROPERTY(bool bondsVisible READ bondsVisible WRITE setBondsVisible NOTIFY effectsChanged)
    Q_PROPERTY(bool animating READ animating WRITE setAnimating NOTIFY animatingChanged)

    // Scene info for the in-scene HUD / camera fit.
    Q_PROPERTY(int atomCount READ atomCount NOTIFY structureChanged)
    Q_PROPERTY(int bondCount READ bondCount NOTIFY structureChanged)
    Q_PROPERTY(QVector3D sceneCenter READ sceneCenter NOTIFY structureChanged)
    Q_PROPERTY(float sceneExtent READ sceneExtent NOTIFY structureChanged)
    Q_PROPERTY(QString selectionLabel READ selectionLabel NOTIFY selectionChanged)
    Q_PROPERTY(QString backendName READ backendName WRITE setBackendName NOTIFY infoChanged)
    Q_PROPERTY(QString embedMode READ embedMode WRITE setEmbedMode NOTIFY infoChanged)
    Q_PROPERTY(double fps READ fps WRITE setFps NOTIFY fpsChanged)

public:
    explicit SceneController(QObject* parent = nullptr);

    QQuick3DInstancing* atomInstancing() const;
    QQuick3DInstancing* bondInstancing() const;

    bool ssaoEnabled() const { return m_ssao; }
    bool shadowsEnabled() const { return m_shadows; }
    bool bloomEnabled() const { return m_bloom; }
    bool tonemapEnabled() const { return m_tonemap; }
    bool bondsVisible() const { return m_bondsVisible; }
    bool animating() const { return m_animating; }

    int atomCount() const { return m_structure.atoms.size(); }
    int bondCount() const { return m_structure.bonds.size(); }
    QVector3D sceneCenter() const { return m_center; }
    float sceneExtent() const { return m_extent; }
    QString selectionLabel() const { return m_selectionLabel; }
    QString backendName() const { return m_backend; }
    QString embedMode() const { return m_embed; }
    double fps() const { return m_fps; }

    /// Load one of the synthetic stress datasets (1000 / 5000 / 10000 atoms).
    Q_INVOKABLE void loadDataset(int targetCount);
    /// Load a molecule from an XYZ file.
    Q_INVOKABLE bool loadFile(const QString& path);
    /// Picking: instanceIndex from View3D.pick on the atom model (-1 = cleared).
    Q_INVOKABLE void setSelectedAtom(int instanceIndex);

public slots:
    void setSsaoEnabled(bool on);
    void setShadowsEnabled(bool on);
    void setBloomEnabled(bool on);
    void setTonemapEnabled(bool on);
    void setBondsVisible(bool on);
    void setAnimating(bool on);
    void setBackendName(const QString& name);
    void setEmbedMode(const QString& mode);
    void setFps(double fps);

signals:
    void effectsChanged();
    void animatingChanged();
    void structureChanged();
    void selectionChanged();
    void infoChanged();
    void fpsChanged();

private:
    void rebuildInstances(); // push current structure into the instancing objects
    void recomputeBounds();
    void stepAnimation(); // MD-proxy: jitter positions and re-upload

    mol::Structure m_structure;
    QVector<QVector3D> m_basePositions; // undisturbed positions for the animation
    AtomInstancing* m_atomInstancing = nullptr;
    BondInstancing* m_bondInstancing = nullptr;

    QVector3D m_center;
    float m_extent = 50.0f;
    float m_atomScale = 0.0f;  // sphere display radius factor (scene units)
    float m_bondRadius = 0.0f;

    bool m_ssao = false;
    bool m_shadows = false;
    bool m_bloom = false;
    bool m_tonemap = true;
    bool m_bondsVisible = true;
    bool m_animating = false;

    int m_selected = -1;
    QString m_selectionLabel;
    QString m_backend;
    QString m_embed;
    double m_fps = 0.0;

    QTimer m_animTimer;
    double m_animPhase = 0.0;
};
