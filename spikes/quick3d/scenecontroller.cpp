// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// WP0 spike - scene controller. Claude Generated.
#include "scenecontroller.h"

#include "atominstancing.h"
#include "bondinstancing.h"

#include <QtMath>
#include <limits>

SceneController::SceneController(QObject* parent)
    : QObject(parent)
{
    m_atomInstancing = new AtomInstancing(nullptr);
    m_atomInstancing->setParent(this);
    m_bondInstancing = new BondInstancing(nullptr);
    m_bondInstancing->setParent(this);

    // MD-proxy: re-upload jittered positions ~60x/s so we measure the per-frame
    // instance-buffer update path (K2). Driven on a timer rather than the render
    // loop so it is independent of the embedding route.
    m_animTimer.setInterval(16);
    connect(&m_animTimer, &QTimer::timeout, this, &SceneController::stepAnimation);

    loadDataset(1000); // default starting dataset
}

QQuick3DInstancing* SceneController::atomInstancing() const { return m_atomInstancing; }
QQuick3DInstancing* SceneController::bondInstancing() const { return m_bondInstancing; }

void SceneController::loadDataset(int targetCount)
{
    m_structure = mol::makeGrid(targetCount);
    setSelectedAtom(-1);
    recomputeBounds();
    rebuildInstances();
    emit structureChanged();
}

bool SceneController::loadFile(const QString& path)
{
    mol::Structure s;
    QString err;
    if (!mol::loadXyz(path, s, &err)) {
        qWarning("loadFile: %s", qPrintable(err));
        return false;
    }
    m_structure = s;
    setSelectedAtom(-1);
    recomputeBounds();
    rebuildInstances();
    emit structureChanged();
    return true;
}

void SceneController::recomputeBounds()
{
    if (m_structure.atoms.isEmpty()) {
        m_center = QVector3D();
        m_extent = 50.0f;
        return;
    }
    QVector3D lo(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max());
    QVector3D hi = -lo;
    for (const mol::Atom& a : m_structure.atoms) {
        lo.setX(qMin(lo.x(), a.position.x()));
        lo.setY(qMin(lo.y(), a.position.y()));
        lo.setZ(qMin(lo.z(), a.position.z()));
        hi.setX(qMax(hi.x(), a.position.x()));
        hi.setY(qMax(hi.y(), a.position.y()));
        hi.setZ(qMax(hi.z(), a.position.z()));
    }
    m_center = 0.5f * (lo + hi);
    m_extent = qMax(1.0f, 0.5f * (hi - lo).length()); // bounding-sphere radius

    // Ball-and-stick proportions in scene units (1 unit == 1 Angstrom).
    m_atomScale = 0.35f; // multiplied by per-element vdW radius below
    m_bondRadius = 0.12f;

    // Cache the undisturbed positions for the animation.
    m_basePositions.resize(m_structure.atoms.size());
    for (int i = 0; i < m_structure.atoms.size(); ++i)
        m_basePositions[i] = m_structure.atoms[i].position;
}

void SceneController::rebuildInstances()
{
    QVector<AtomInstancing::Item> items;
    items.reserve(m_structure.atoms.size());
    for (const mol::Atom& a : m_structure.atoms) {
        AtomInstancing::Item it;
        it.position = a.position;
        it.scale = m_atomScale * mol::vdwRadius(a.element); // scene-unit radius
        it.color = mol::cpkColor(a.element);
        items.append(it);
    }
    m_atomInstancing->setItems(items);
    m_atomInstancing->setHighlight(m_selected);

    if (m_bondsVisible)
        m_bondInstancing->setBonds(m_structure.atoms, m_structure.bonds, m_bondRadius);
    else
        m_bondInstancing->setBonds(m_structure.atoms, {}, m_bondRadius);
}

void SceneController::setSelectedAtom(int instanceIndex)
{
    if (instanceIndex < 0 || instanceIndex >= m_structure.atoms.size())
        instanceIndex = -1;
    m_selected = instanceIndex;
    if (m_selected < 0) {
        m_selectionLabel = QStringLiteral("—");
    } else {
        const mol::Atom& a = m_structure.atoms[m_selected];
        m_selectionLabel = QStringLiteral("%1 #%2  (%3, %4, %5)")
                               .arg(a.element)
                               .arg(m_selected)
                               .arg(a.position.x(), 0, 'f', 2)
                               .arg(a.position.y(), 0, 'f', 2)
                               .arg(a.position.z(), 0, 'f', 2);
    }
    if (m_atomInstancing)
        m_atomInstancing->setHighlight(m_selected);
    emit selectionChanged();
}

void SceneController::stepAnimation()
{
    // Cheap deterministic jitter around the cached base positions; exercises a full
    // instance-buffer rebuild + re-upload every frame (worst case for K2).
    m_animPhase += 0.15;
    const int n = m_structure.atoms.size();
    for (int i = 0; i < n; ++i) {
        const float ph = m_animPhase + i * 0.3f;
        const QVector3D jitter(0.15f * std::sin(ph),
            0.15f * std::sin(ph * 1.3f + 1.0f),
            0.15f * std::sin(ph * 0.7f + 2.0f));
        m_structure.atoms[i].position = m_basePositions[i] + jitter;
    }
    rebuildInstances();
}

void SceneController::setSsaoEnabled(bool on)
{
    if (m_ssao == on)
        return;
    m_ssao = on;
    emit effectsChanged();
}

void SceneController::setShadowsEnabled(bool on)
{
    if (m_shadows == on)
        return;
    m_shadows = on;
    emit effectsChanged();
}

void SceneController::setBloomEnabled(bool on)
{
    if (m_bloom == on)
        return;
    m_bloom = on;
    emit effectsChanged();
}

void SceneController::setTonemapEnabled(bool on)
{
    if (m_tonemap == on)
        return;
    m_tonemap = on;
    emit effectsChanged();
}

void SceneController::setBondsVisible(bool on)
{
    if (m_bondsVisible == on)
        return;
    m_bondsVisible = on;
    rebuildInstances();
    emit effectsChanged();
}

void SceneController::setAnimating(bool on)
{
    if (m_animating == on)
        return;
    m_animating = on;
    if (on) {
        m_animPhase = 0.0;
        m_animTimer.start();
    } else {
        m_animTimer.stop();
        // restore the undisturbed structure
        for (int i = 0; i < m_structure.atoms.size(); ++i)
            m_structure.atoms[i].position = m_basePositions[i];
        rebuildInstances();
    }
    emit animatingChanged();
}

void SceneController::setBackendName(const QString& name)
{
    if (m_backend == name)
        return;
    m_backend = name;
    emit infoChanged();
}

void SceneController::setEmbedMode(const QString& mode)
{
    if (m_embed == mode)
        return;
    m_embed = mode;
    emit infoChanged();
}

void SceneController::setFps(double fps)
{
    if (qFuzzyCompare(m_fps, fps))
        return;
    m_fps = fps;
    emit fpsChanged();
}
