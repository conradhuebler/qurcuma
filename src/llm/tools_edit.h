// tools_edit.h - Tools that change the structure.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - All Effect::Mutate, so every one asks before it runs,
// and all of them refuse while a simulation is going: the viewer rebuilds frame 0
// from the worker's geometry, so an atom added mid-run would be overwritten or, if
// it changes the count, take the whole bond graph with it (see WP0.1).
//
// Every change goes through the viewer's own edit path, which means it lands in
// the snapshot stack and Ctrl+Z takes it back -- a model's mistake is as
// recoverable as a user's.
#pragma once

class MoleculeViewer;
class ToolRegistry;

struct EditToolContext {
    MoleculeViewer* viewer = nullptr;
};

/// Register the structure-editing tools. Returns how many.
int registerEditTools(ToolRegistry& registry, const EditToolContext& context);
