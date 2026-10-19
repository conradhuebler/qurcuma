// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// QQuick3DInstancing subclass for bond cylinders. The scene controller computes the
// per-segment transform (orientation quaternion from view.cpp's Y->bond math) and
// colour; this class just uploads them. Replaces the Qt3D BondInstancingSystem.
// Also used for every other segment-like table (measurement lines, arrows, walls, NCI).
// Claude Generated.
#pragma once

#include <QColor>
#include <QQuaternion>
#include <QVector3D>

#include "tableinstancing.h"

class BondInstancing : public TableInstancing
{
    Q_OBJECT
public:
    explicit BondInstancing(QQuick3DObject* parent = nullptr);

    struct Segment {
        QVector3D center;     // scene units
        QVector3D scale;      // (radius/50, halfLength/50, radius/50) for #Cylinder
        QQuaternion rotation; // local +Y -> bond direction
        QColor color;
    };

    void setSegments(const QVector<Segment>& segments);
};
