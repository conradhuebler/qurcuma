// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// WP0 spike - QQuick3DInstancing subclass for atoms (spheres). One draw call for
// all atoms; per-instance position/scale/colour built via the Qt helper
// calculateTableEntry(). Replaces qurcuma's Qt3D AtomInstancingSystem.
// Claude Generated.
#pragma once

#include <QColor>
#include <QQuick3DInstancing>
#include <QVector3D>

// Instantiated in C++ (owned by SceneController) and bound into QML via the base
// QQuick3DInstancing* property type, so no QML_ELEMENT registration is needed.
class AtomInstancing : public QQuick3DInstancing
{
    Q_OBJECT
public:
    explicit AtomInstancing(QQuick3DObject* parent = nullptr);

    struct Item {
        QVector3D position; // scene units (Angstrom)
        float scale = 1.0f; // sphere display radius in scene units
        QColor color;
    };

    /// Replace the full instance list and re-upload. Cheap enough to call every
    /// frame for the MD-proxy animation. Claude Generated.
    void setItems(const QVector<Item>& items);

    /// Recolour a single instance as the picked atom (-1 = none) without changing
    /// geometry; used by the picking path. Claude Generated.
    void setHighlight(int index);

protected:
    QByteArray getInstanceBuffer(int* instanceCount) override;

private:
    void rebuild();

    QVector<Item> m_items;
    int m_highlight = -1;
    QByteArray m_buffer;
    int m_count = 0;
};
