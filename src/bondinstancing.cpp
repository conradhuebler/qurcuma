// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Bond cylinder instancing for the Qt Quick 3D viewer. Claude Generated.
#include "bondinstancing.h"

BondInstancing::BondInstancing(QQuick3DObject* parent)
    : TableInstancing(parent)
{
}

void BondInstancing::setSegments(const QVector<Segment>& segments)
{
    m_count = segments.size();
    m_buffer.resize(m_count * int(sizeof(InstanceTableEntry)));
    auto* entry = reinterpret_cast<InstanceTableEntry*>(m_buffer.data());
    for (int i = 0; i < m_count; ++i) {
        const Segment& s = segments[i];
        entry[i] = calculateTableEntryFromQuaternion(s.center, s.scale, s.rotation, s.color);
    }
    publish(); // markDirty + tell mirrors (tableinstancing.h)
}
