// atomselection.cpp - One place that knows how an atom set is named.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "atomselection.h"

#include "moleculebridge.h"

#include <QSet>

#include <map>
#include <vector>

bool resolveAtomSet(const QVector<moldata::Atom>& atoms, const QString& expression,
                    const QJsonArray& explicitIndices, QVector<int>& out, QString& error)
{
    out.clear();
    if (!expression.isEmpty() && !explicitIndices.isEmpty()) {
        error = QStringLiteral("pass either an expression or explicit indices, not both");
        return false;
    }

    if (!expression.isEmpty()) {
        curcuma::Molecule molecule = atomsToMolecule(atoms);
        // FragString2Indicies reads m_fragments directly, and that cache is only
        // filled by GetFragments(). Without this call every "Fn" silently matches
        // nothing -- which is exactly how it failed the first time it was used.
        molecule.GetFragments();
        const std::vector<int> resolved = molecule.FragString2Indicies(expression.toStdString());
        if (resolved.empty()) {
            error = QStringLiteral("selection \"%1\" matched no atoms").arg(expression);
            return false;
        }
        for (int index : resolved)
            out.append(index);
    } else {
        for (const QJsonValue& value : explicitIndices) {
            if (!value.isDouble()) {
                error = QStringLiteral("atom indices must be numbers");
                return false;
            }
            out.append(value.toInt());
        }
    }

    for (int index : out) {
        if (index < 0 || index >= atoms.size()) {
            error = QStringLiteral("atom index %1 is outside 0..%2").arg(index).arg(atoms.size() - 1);
            return false;
        }
    }
    return true;
}

QVector<moldata::Atom> subsetAtoms(const QVector<moldata::Atom>& atoms,
                                   const QVector<int>& indices)
{
    QVector<moldata::Atom> out;
    out.reserve(indices.size());
    for (int index : indices) {
        if (index >= 0 && index < atoms.size())
            out.append(atoms.at(index));
    }
    return out;
}

int severedBondCount(const QVector<moldata::Atom>& atoms, const QVector<int>& indices)
{
    if (indices.isEmpty() || indices.size() == atoms.size())
        return 0;

    const QSet<int> inside(indices.constBegin(), indices.constEnd());
    // Distance-based connectivity from curcuma rather than the viewer's bond list:
    // this stays GUI-free, and it is the same perception GetFragments() uses, so
    // "whole fragment selected" and "no bond cut" agree by construction.
    const curcuma::Molecule molecule = atomsToMolecule(atoms);
    const std::map<int, std::vector<int>> connectivity = molecule.getConnectivtiy();

    int severed = 0;
    for (const auto& [atom, neighbours] : connectivity) {
        for (int neighbour : neighbours) {
            if (atom >= neighbour)
                continue;  // count each bond once
            if (inside.contains(atom) != inside.contains(neighbour))
                ++severed;
        }
    }
    return severed;
}
