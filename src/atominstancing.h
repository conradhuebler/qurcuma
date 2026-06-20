// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// QQuick3DInstancing subclass for atom spheres — one draw call for all atoms,
// per-instance position/scale/colour. Replaces the Qt3D AtomInstancingSystem.
// Validated in spikes/quick3d. Claude Generated.
#pragma once

#include <QColor>
#include <QQuick3DInstancing>
#include <QVector3D>

// Instantiated in C++ (owned by the scene controller) and bound into QML via the
// base QQuick3DInstancing* property type — no QML_ELEMENT registration needed.
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

    /// Replace the full instance list and re-upload (cheap enough per frame).
    void setItems(const QVector<Item>& items);
    /// Recolour a single instance as the picked atom (-1 = none).
    void setHighlight(int index, const QColor& color);

protected:
    QByteArray getInstanceBuffer(int* instanceCount) override;

private:
    void rebuild();

    QVector<Item> m_items;
    int m_highlight = -1;
    QColor m_highlightColor{ 255, 0, 255 };
    QByteArray m_buffer;
    int m_count = 0;
};
