// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Instance tables shared between two Qt Quick 3D scenes, see tableinstancing.h.
// Claude Generated 2026.
#include "tableinstancing.h"

TableInstancing::TableInstancing(QQuick3DObject* parent)
    : QQuick3DInstancing(parent)
{
}

void TableInstancing::publish()
{
    markDirty();
    emit tableChanged();
}

QByteArray TableInstancing::getInstanceBuffer(int* instanceCount)
{
    if (instanceCount)
        *instanceCount = m_count;
    return m_buffer;
}

MirrorInstancing::MirrorInstancing(TableInstancing* source, QQuick3DObject* parent)
    : QQuick3DInstancing(parent)
    , m_source(source)
{
    if (!m_source)
        return;
    connect(m_source, &TableInstancing::tableChanged, this, [this] { markDirty(); });
    setHasTransparency(m_source->hasTransparency());
    setDepthSortingEnabled(m_source->depthSortingEnabled());
    connect(m_source, &QQuick3DInstancing::hasTransparencyChanged, this,
        [this] { setHasTransparency(m_source->hasTransparency()); });
    connect(m_source, &QQuick3DInstancing::depthSortingEnabledChanged, this,
        [this] { setDepthSortingEnabled(m_source->depthSortingEnabled()); });
}

QByteArray MirrorInstancing::getInstanceBuffer(int* instanceCount)
{
    if (!m_source) {
        if (instanceCount)
            *instanceCount = 0;
        return {};
    }
    if (instanceCount)
        *instanceCount = m_source->tableCount();
    return m_source->table();
}
