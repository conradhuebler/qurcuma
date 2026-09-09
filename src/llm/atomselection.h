// atomselection.h - One place that knows how an atom set is named.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Selection is the hinge of the whole tool layer: nearly
// every tool that does something to part of a structure has to be told which part.
// The grammar is curcuma's (FragString2Indicies), and it has two traps that cost
// a real session three failed calls: it is one-based where the tools are
// zero-based, and "Fn" resolves against a fragment cache that only GetFragments()
// fills. Both are handled here, once, and pinned by test_selectiongrammar.
//
// No viewer, no widgets: this works on moldata::Atom, so it is testable headless
// and usable from the compute tools as well as the view tools.
#pragma once

#include "core/moleculedata.h"

#include <QJsonArray>
#include <QString>
#include <QVector>

/// Resolve an atom set given either as curcuma's selection grammar ("1:10,15",
/// "F2", "-1") or as explicit zero-based indices. Exactly one of the two must be
/// given. Returns false and fills @p error on an empty match or an index out of
/// range; @p out holds zero-based indices on success.
bool resolveAtomSet(const QVector<moldata::Atom>& atoms, const QString& expression,
                    const QJsonArray& explicitIndices, QVector<int>& out, QString& error);

/// The atoms at @p indices, in the order the indices are given. Callers are
/// expected to have run the indices through resolveAtomSet(), which range-checks.
QVector<moldata::Atom> subsetAtoms(const QVector<moldata::Atom>& atoms,
                                   const QVector<int>& indices);

/// How many covalent bonds have exactly one end inside @p indices, i.e. how many
/// bonds a calculation on that subset would cut. Zero means the selection is a set
/// of whole molecules and its energy stands on its own; anything above zero means
/// open valences, and an energy computed from it is not comparable to the whole.
int severedBondCount(const QVector<moldata::Atom>& atoms, const QVector<int>& indices);
