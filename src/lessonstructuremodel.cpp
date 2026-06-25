// lessonstructuremodel.cpp - Read-only list model over a lesson's structures
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - see lessonstructuremodel.h.

#include "lessonstructuremodel.h"

LessonStructureModel::LessonStructureModel(const QVector<LessonStructure>* structures, QObject* parent)
    : QAbstractListModel(parent)
    , m_structures(structures)
{
}

int LessonStructureModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !m_structures)
        return 0;
    return static_cast<int>(m_structures->size());
}

const LessonStructure* LessonStructureModel::at(int row) const
{
    if (!m_structures || row < 0 || row >= m_structures->size())
        return nullptr;
    return &(*m_structures)[row];
}

QVariant LessonStructureModel::data(const QModelIndex& index, int role) const
{
    const LessonStructure* s = at(index.row());
    if (!s)
        return {};

    if (role == Qt::DisplayRole) {
        QString text = s->name.isEmpty() ? QObject::tr("(unnamed)") : s->name;
        if (!s->role.isEmpty())
            text += QStringLiteral("  ·  %1").arg(s->role);
        return text;
    }
    if (role == Qt::ToolTipRole) {
        // Compact summary of the stored simulation conditions.
        const QString mode = (s->sim.mode == SimulationConfig::Mode::GeometryOptimization)
            ? QStringLiteral("Opt") : QStringLiteral("MD");
        QStringList parts;
        parts << s->sim.method.toUpper() << mode;
        if (s->sim.mode == SimulationConfig::Mode::MolecularDynamics)
            parts << QStringLiteral("%1 K").arg(s->sim.temperature, 0, 'f', 0);
        if (s->sim.wallEnabled)
            parts << (s->sim.wallType == 1 ? QObject::tr("spheric wall")
                                           : QObject::tr("box wall"));
        QString tip = parts.join(QStringLiteral(" · "));
        if (!s->description.isEmpty())
            tip = s->description + QStringLiteral("\n") + tip;
        return tip;
    }
    return {};
}

void LessonStructureModel::refresh()
{
    beginResetModel();
    endResetModel();
}
