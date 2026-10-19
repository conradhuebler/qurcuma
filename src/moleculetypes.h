// moleculetypes.h - GUI-free atom and bond names for the simulation worker and the remote server
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - The records themselves live in core/moleculedata.h (moldata::Atom/Bond,
// shared with the file loader and the LLM tools); MolAtom/MolBond are the names used by code that
// has no widgets (qurcuma-server). MoleculeViewer::Atom/Bond are the same types.

#pragma once

#include "core/moleculedata.h"

using MolAtom = moldata::Atom;
using MolBond = moldata::Bond;
