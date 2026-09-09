// tools_files.h - Bringing a structure in from disk, and writing one back out.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - The docking scenario starts with two molecules, and a
// model that can only look at what a human already opened cannot get there. These
// two reach the filesystem, so they ask before they run: merge_structure changes
// the scene (Mutate), save_structure creates a file (FileWrite).
//
// No mainwindow.h here either. Replacing the whole scene stays in the GUI, because
// MainWindow owns the trajectory, the frame count and the window state, and a tool
// that swapped the geometry underneath it would leave all three stale.
#pragma once

#include <QString>

#include <functional>

class MoleculeViewer;
class ToolRegistry;

struct FileToolContext {
    MoleculeViewer* viewer = nullptr;
    /// Where a relative path is resolved; supplied by MainWindow, may be empty.
    std::function<QString()> workingDirectory;
};

/// Register merge_structure and save_structure. Returns how many.
int registerFileTools(ToolRegistry& registry, const FileToolContext& context);
