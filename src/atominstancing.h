// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// QQuick3DInstancing subclass for atom spheres — one draw call for all atoms,
// per-instance position/scale/colour. Replaces the Qt3D AtomInstancingSystem.
// Validated in spikes/quick3d. Claude Generated.
#pragma once

#include <QColor>
#include <QQuaternion>
#include <QVector3D>

#include "tableinstancing.h"

// Instantiated in C++ (owned by the scene controller) and bound into QML via the
// base QQuick3DInstancing* property type — no QML_ELEMENT registration needed.
// The table can be shown in a second scene through a MirrorInstancing (tableinstancing.h).
class AtomInstancing : public TableInstancing
{
    Q_OBJECT
public:
    explicit AtomInstancing(QQuick3DObject* parent = nullptr);

    struct Item {
        QVector3D position; // scene units (Angstrom)
        float scale = 1.0f; // sphere display radius in scene units
        QColor color;
        int atomIndex = -1; // structure atom this instance draws (hidden atoms get no instance)
        // Claude Generated 2026 - A thermal ellipsoid instead of a sphere: the
        // sphere scaled by semiAxes (Angstrom) along its local axes and turned.
        bool ellipsoid = false;
        QVector3D semiAxes;
        QQuaternion rotation;
    };

    /// Replace the full instance list and re-upload (cheap enough per frame).
    void setItems(const QVector<Item>& items);
    /// Recolour a single instance as the picked atom (-1 = none).
    void setHighlight(int index, const QColor& color);
    /// Claude Generated 2026 - Structure atom drawn by instance @p instance (-1 if out of range).
    /// Instances are compacted (hidden atoms are skipped), so this is not the identity.
    int atomIndexOf(int instance) const;

private:
    void rebuild();

    QVector<Item> m_items;
    int m_highlight = -1;
    QColor m_highlightColor{ 255, 0, 255 };
};
