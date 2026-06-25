// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Atom sphere instancing for the Qt Quick 3D viewer. Claude Generated.
#include "atominstancing.h"

namespace {
// Quick3D built-in "#Sphere" has base radius 50; divide the desired scene-unit
// radius by this to get the instance scale.
constexpr float kSphereBaseRadius = 50.0f;
}

AtomInstancing::AtomInstancing(QQuick3DObject* parent)
    : QQuick3DInstancing(parent)
{
}

void AtomInstancing::setItems(const QVector<Item>& items)
{
    m_items = items;
    if (m_highlight >= m_items.size())
        m_highlight = -1;
    rebuild();
}

void AtomInstancing::setHighlight(int index, const QColor& color)
{
    if (index >= m_items.size())
        index = -1;
    if (m_highlight == index && m_highlightColor == color)
        return;
    m_highlight = index;
    m_highlightColor = color;
    rebuild();
}

void AtomInstancing::rebuild()
{
    m_count = m_items.size();
    m_buffer.resize(m_count * int(sizeof(InstanceTableEntry)));
    auto* entry = reinterpret_cast<InstanceTableEntry*>(m_buffer.data());

    for (int i = 0; i < m_count; ++i) {
        const Item& it = m_items[i];
        const QColor color = (i == m_highlight) ? m_highlightColor : it.color;
        const float s = it.scale / kSphereBaseRadius;
        entry[i] = calculateTableEntry(it.position,
            QVector3D(s, s, s), QVector3D(0, 0, 0), color);
    }
    markDirty();
}

QByteArray AtomInstancing::getInstanceBuffer(int* instanceCount)
{
    if (instanceCount)
        *instanceCount = m_count;
    return m_buffer;
}
