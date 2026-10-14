// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Instance tables that a second Qt Quick 3D scene can show without recomputing them.
// Claude Generated 2026.
//
// SceneController computes every instance table (atoms, bonds, overlays, walls, NCI, ...)
// once. The desktop View3D draws them through AtomInstancing/BondInstancing, both derived
// from TableInstancing. The headset scene (XrView) is a second window, and Qt Quick 3D does
// not let one instancing object serve two windows (it warns "Cannot use same item on
// different windows at the same time" and the second window stops following rebuilds).
// So the headset draws the same tables through a MirrorInstancing per table, which hands
// out the source's finished QByteArray (implicitly shared, no copy) and re-fetches it
// whenever the source publishes a rebuild.
//
// Dirty propagation (pinned by test_instancing_mirror): QQuick3DInstancing asks
// getInstanceBuffer() only after markDirty(), during the scene-graph sync of the window
// that draws it (render thread with the GUI thread blocked, or the GUI thread itself), so a
// mirror must be marked dirty for every rebuild of its source. publish() does that through
// tableChanged().
#pragma once

#include <QByteArray>
#include <QPointer>
#include <QQuick3DInstancing>

/**
 * @brief Base class for qurcuma's instance tables (Claude Generated 2026).
 *
 * Subclasses fill m_buffer (InstanceTableEntry rows) and m_count, then call publish().
 */
class TableInstancing : public QQuick3DInstancing
{
    Q_OBJECT
public:
    explicit TableInstancing(QQuick3DObject* parent = nullptr);

    /// The finished table (m_count InstanceTableEntry rows); implicitly shared.
    QByteArray table() const { return m_buffer; }
    int tableCount() const { return m_count; }

signals:
    /// Emitted by publish() after every rebuild; mirrors mark themselves dirty on it.
    void tableChanged();

protected:
    /// Marks this table dirty for its own scene and announces the rebuild to mirrors.
    void publish();
    QByteArray getInstanceBuffer(int* instanceCount) override;

    QByteArray m_buffer;
    int m_count = 0;
};

/**
 * @brief Read-only view of a TableInstancing for a second scene (Claude Generated 2026).
 *
 * Holds no table of its own: getInstanceBuffer() returns the source's current table.
 * Follows the source's hasTransparency/depthSortingEnabled flags as well.
 */
class MirrorInstancing : public QQuick3DInstancing
{
    Q_OBJECT
public:
    explicit MirrorInstancing(TableInstancing* source, QQuick3DObject* parent = nullptr);

    TableInstancing* source() const { return m_source; }

protected:
    QByteArray getInstanceBuffer(int* instanceCount) override;

private:
    QPointer<TableInstancing> m_source;
};
