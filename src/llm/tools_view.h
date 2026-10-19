// tools_view.h - Read-only tools over the loaded structure and the viewer.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - These reach MoleculeViewer, so they are all
// ToolAffinity::Gui and the dispatcher marshals them. Analysis goes through
// moleculebridge.h into curcuma::Molecule rather than being re-implemented here:
// fragment perception, the selection grammar and the geometry all already exist
// there, and a second implementation would drift.
//
// Deliberately no mainwindow.h: the one thing these need from MainWindow (the
// working directory) arrives as a callable, so the coupling stays on the viewer.
#pragma once

#include <QString>
#include <functional>

class MoleculeViewer;
class ToolRegistry;

struct ViewToolContext {
    MoleculeViewer* viewer = nullptr;
    /// Current working directory; supplied by MainWindow, may be empty.
    std::function<QString()> workingDirectory;
};

/// Register the read-only view tools. Returns how many were registered.
int registerViewTools(ToolRegistry& registry, const ViewToolContext& context);
