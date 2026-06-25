// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// WP0 spike - atom sphere instancing. Claude Generated.
#include "atominstancing.h"

namespace {
// Picked atom is recoloured to a distinct highlight colour.
const QColor kHighlightColor(255, 0, 255);
// Quick3D built-in "#Sphere" has base radius 50; divide the desired scene-unit
// radius by this to get the instance scale. (Calibrate visually if it looks off.)
constexpr float kSphereBaseRadius = 50.0f;
}

AtomInstancing::AtomInstancing(QQuick3DObject* parent)
    : QQuick3DInstancing(parent)
{
}

void AtomInstancing::setItems(const QVector<Item>& items)
{
    m_items = items;
    rebuild();
}

void AtomInstancing::setHighlight(int index)
{
    if (m_highlight == index)
        return;
    m_highlight = index;
    rebuild();
}

void AtomInstancing::rebuild()
{
    m_count = m_items.size();
    m_buffer.resize(m_count * int(sizeof(InstanceTableEntry)));
    auto* entry = reinterpret_cast<InstanceTableEntry*>(m_buffer.data());

    for (int i = 0; i < m_count; ++i) {
        const Item& it = m_items[i];
        const QColor color = (i == m_highlight) ? kHighlightColor : it.color;
        const float s = it.scale / kSphereBaseRadius;
        // No rotation for spheres; uniform scale.
        entry[i] = calculateTableEntry(it.position,
            QVector3D(s, s, s),
            QVector3D(0, 0, 0),
            color);
    }
    markDirty(); // tell Quick3D the instance table changed -> triggers redraw
}

QByteArray AtomInstancing::getInstanceBuffer(int* instanceCount)
{
    if (instanceCount)
        *instanceCount = m_count;
    return m_buffer;
}
