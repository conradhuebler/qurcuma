// moleculefileloader.h - Unified structure-file reader.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Single entry point for reading molecular structure
// files (xyz / vtf / pdb / mol2) into viewer atoms/bonds. Replaces the format
// dispatch ladder that was copy-pasted across MainWindow (full trajectory load,
// first-frame merge, remote download). Parsing only — no UI side effects, and
// since 2026-09 no dependency on view.h either: the whole parse path speaks
// moldata::Atom / ::Bond, so it is usable without QtWidgets.

#pragma once

#include "core/moleculedata.h"  // moldata::Atom / ::Bond

#include <QString>
#include <QVector>

class MoleculeFileLoader
{
public:
    /// Result of parsing a structure file. @a frames / @a frameBonds hold one
    /// entry per trajectory frame (xyz/vtf/pdb multi-model; mol2 single frame).
    struct Result {
        bool supported = false;  // extension is a known structure format
        bool ok = false;         // supported AND at least one frame parsed
        QVector<QVector<moldata::Atom>> frames;
        QVector<QVector<moldata::Bond>> frameBonds;
        QString error;           // parser error message (pdb/mol2), empty otherwise

        int frameCount() const { return frames.size(); }
    };

    /// Parse @p path into atoms/bonds. Uses stack-local parser instances (no
    /// shared state, no leak). Returns Result with ok=false on any failure.
    static Result load(const QString& path);
};
