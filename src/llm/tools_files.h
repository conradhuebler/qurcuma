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

#include "moleculefileloader.h"

#include <QJsonObject>
#include <QString>

#include <functional>

class MoleculeViewer;
class ToolRegistry;

struct FileToolContext {
    MoleculeViewer* viewer = nullptr;
    /// Where a relative path is resolved; supplied by MainWindow, may be empty.
    std::function<QString()> workingDirectory;
    /// Claude Generated 2026 - Replace the scene with a file through MainWindow
    /// (the Unit Cell dock, the cell drawing and the save path follow), a cif built
    /// as @p cif says. False with a reason when it could not be opened.
    std::function<bool(const QString& path, const MoleculeFileLoader::CifOptions& cif,
        QString* error)> openStructure;
    /// The cif the viewer shows, empty when none.
    std::function<QString()> shownCif;
};

/// Register merge_structure, save_structure, open_structure and describe_cif.
/// Returns how many.
int registerFileTools(ToolRegistry& registry, const FileToolContext& context);

/// Claude Generated 2026 - A tool's path argument: relative means "in the working
/// directory" (what list_workdir shows), absolute is taken as given.
QString resolveToolPath(const QString& path, const std::function<QString()>& workingDirectory);

/// Claude Generated 2026 - How a cif is built, from the cif_content, disorder_group
/// and complete_molecules arguments the file tools and run_single_point share.
MoleculeFileLoader::CifOptions cifOptionsFromArgs(const QJsonObject& args, bool unitCellByDefault);
