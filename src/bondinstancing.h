// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// QQuick3DInstancing subclass for bond cylinders. The scene controller computes the
// per-segment transform (orientation quaternion from view.cpp's Y->bond math) and
// colour; this class just uploads them. Replaces the Qt3D BondInstancingSystem.
// Claude Generated.
#pragma once

#include <QColor>
#include <QQuaternion>
#include <QQuick3DInstancing>
#include <QVector3D>

class BondInstancing : public QQuick3DInstancing
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

protected:
    QByteArray getInstanceBuffer(int* instanceCount) override;

private:
    QByteArray m_buffer;
    int m_count = 0;
};
