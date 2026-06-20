// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// WP0 spike - QQuick3DInstancing subclass for bonds (cylinders). Each bond is two
// half-cylinders coloured per atom; orientation via a quaternion that rotates the
// cylinder's local +Y axis onto the bond direction. The quaternion math mirrors
// qurcuma's Qt3D path (src/view.cpp:1345). Claude Generated.
#pragma once

#include "moleculedata.h"

#include <QColor>
#include <QQuaternion>
#include <QQuick3DInstancing>
#include <QVector3D>

// Instantiated in C++ (owned by SceneController); bound into QML via the base
// QQuick3DInstancing* property type, so no QML_ELEMENT registration is needed.
class BondInstancing : public QQuick3DInstancing
{
    Q_OBJECT
public:
    explicit BondInstancing(QQuick3DObject* parent = nullptr);

    /// Rebuild the cylinder instances from atoms+bonds. radius in scene units.
    /// Recomputes orientation each call (simple & correct); the per-frame MD-proxy
    /// uses this directly. Claude Generated.
    void setBonds(const QVector<mol::Atom>& atoms, const QVector<mol::Bond>& bonds, float radius);

protected:
    QByteArray getInstanceBuffer(int* instanceCount) override;

private:
    QByteArray m_buffer;
    int m_count = 0;
};
