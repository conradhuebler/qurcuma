// tools_files.cpp - Bringing a structure in from disk, and writing one back out.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "tools_files.h"

#include "atomselection.h"
#include "core/toolregistry.h"
#include "lesson.h"              // atomsToXyz
#include "moleculefileloader.h"
#include "view.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTextStream>

namespace {

QJsonObject schema(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

/// A relative path means "in the working directory", the same place list_workdir
/// reports. An absolute path is taken as given.
QString resolve(const QString& path, const std::function<QString()>& workingDirectory)
{
    if (path.isEmpty())
        return QString();
    QFileInfo info(path);
    if (info.isAbsolute())
        return QDir::cleanPath(path);
    const QString base = workingDirectory ? workingDirectory() : QString();
    if (base.isEmpty())
        return QDir::cleanPath(QDir::current().absoluteFilePath(path));
    return QDir::cleanPath(QDir(base).absoluteFilePath(path));
}

}  // namespace

int registerFileTools(ToolRegistry& registry, const FileToolContext& context)
{
    MoleculeViewer* const viewer = context.viewer;
    if (!viewer)
        return 0;
    const std::function<QString()> workingDirectory = context.workingDirectory;

    int added = 0;
    const auto add = [&registry, &added](const ToolSpec& spec) {
        if (registry.add(spec))
            ++added;
    };

    // --- merge_structure ----------------------------------------------------
    {
        ToolSpec spec;
        spec.name = QStringLiteral("merge_structure");
        spec.category = QStringLiteral("build");
        spec.description = QStringLiteral(
            "Add the structure in a file to the current scene, keeping what is already "
            "there. The new atoms arrive at the coordinates the file gives them, so they "
            "may overlap -- check with get_contacts. xyz, vtf, pdb and mol2; of a "
            "multi-frame file only the first frame.");
        spec.effect = ToolEffect::Mutate;
        spec.affinity = ToolAffinity::Gui;
        spec.paramSchema = schema(R"JSON({
          "type": "object",
          "properties": {
            "path": { "type": "string",
                      "description": "file to read; relative to the working directory" }
          },
          "required": ["path"]
        })JSON");

        spec.handler = [viewer, workingDirectory](const QJsonObject& args) {
            if (viewer->simulationActive()) {
                return ToolResult::failure(QStringLiteral(
                    "not while a simulation is running: the next frame would overwrite the "
                    "merge, and the changed atom count would drop the bond graph. Stop the "
                    "run first."));
            }
            if (!viewer->canEditStructure()) {
                return ToolResult::failure(QStringLiteral(
                    "a structure can only be merged into a single frame, not into a trajectory"));
            }

            const QString path = resolve(args.value(QStringLiteral("path")).toString(),
                workingDirectory);
            if (!QFileInfo::exists(path))
                return ToolResult::failure(QStringLiteral("no such file: %1").arg(path));

            const MoleculeFileLoader::Result parsed = MoleculeFileLoader::load(path);
            if (!parsed.supported) {
                return ToolResult::failure(
                    QStringLiteral("%1 is not a structure format this reads (xyz, vtf, pdb, mol2)")
                        .arg(QFileInfo(path).suffix()));
            }
            if (!parsed.ok) {
                return ToolResult::failure(parsed.error.isEmpty()
                        ? QStringLiteral("could not read a structure from %1").arg(path)
                        : parsed.error);
            }

            const QVector<moldata::Atom> atoms = parsed.frames.first();
            const QVector<moldata::Bond> bonds = parsed.frameBonds.isEmpty()
                ? QVector<moldata::Bond>()
                : parsed.frameBonds.first();
            if (atoms.isEmpty())
                return ToolResult::failure(QStringLiteral("%1 holds no atoms").arg(path));

            const int before = viewer->getCurrentFrameAtoms().size();
            // startPlacement=false: the GUI path drops the user into a drag, which a
            // model cannot do. The atoms land where the file put them.
            viewer->appendMolecule(atoms, bonds, false);

            QJsonObject data;
            data.insert(QStringLiteral("path"), path);
            data.insert(QStringLiteral("added_atoms"), atoms.size());
            data.insert(QStringLiteral("total_atoms"), before + atoms.size());
            data.insert(QStringLiteral("first_new_index"), before);
            data.insert(QStringLiteral("frames_in_file"), parsed.frameCount());

            QString note = QStringLiteral("Added %1 atoms as indices %2..%3; %4 in the scene now.")
                               .arg(atoms.size()).arg(before).arg(before + atoms.size() - 1)
                               .arg(before + atoms.size());
            if (parsed.frameCount() > 1)
                note += QStringLiteral(" The file had %1 frames; the first was used.")
                            .arg(parsed.frameCount());
            return ToolResult::success(data, note);
        };
        add(spec);
    }

    // --- save_structure -----------------------------------------------------
    {
        ToolSpec spec;
        spec.name = QStringLiteral("save_structure");
        spec.category = QStringLiteral("build");
        spec.description = QStringLiteral(
            "Write the current structure, or a selection of it, to an xyz file.");
        spec.effect = ToolEffect::FileWrite;
        spec.affinity = ToolAffinity::Gui;
        spec.paramSchema = schema(R"JSON({
          "type": "object",
          "properties": {
            "path": { "type": "string",
                      "description": "file to write, ending in .xyz; relative to the working directory" },
            "atoms": { "type": "string",
                       "description": "restrict to part of the structure, in curcuma's selection grammar (\"F1\", \"1:20\"); omitted means all of it" },
            "atom_indices": { "type": "array",
                              "description": "zero-based atom indices, as an alternative to atoms. Not both." },
            "comment": { "type": "string", "description": "second line of the xyz file" },
            "overwrite": { "type": "boolean",
                           "description": "replace the file if it already exists (default false)" }
          },
          "required": ["path"]
        })JSON");

        spec.handler = [viewer, workingDirectory](const QJsonObject& args) {
            const QVector<moldata::Atom> all = viewer->getCurrentFrameAtoms();
            if (all.isEmpty())
                return ToolResult::failure(QStringLiteral("no structure is loaded"));

            const QString path = resolve(args.value(QStringLiteral("path")).toString(),
                workingDirectory);
            if (QFileInfo(path).suffix().compare(QLatin1String("xyz"), Qt::CaseInsensitive) != 0)
                return ToolResult::failure(QStringLiteral("the path has to end in .xyz"));
            if (QFileInfo::exists(path) && !args.value(QStringLiteral("overwrite")).toBool()) {
                return ToolResult::failure(
                    QStringLiteral("%1 already exists; pass overwrite to replace it").arg(path));
            }

            QVector<moldata::Atom> atoms = all;
            const QString expression = args.value(QStringLiteral("atoms")).toString();
            const QJsonArray indices = args.value(QStringLiteral("atom_indices")).toArray();
            int severed = 0;
            const bool restricted = !expression.isEmpty() || !indices.isEmpty();
            if (restricted) {
                QVector<int> wanted;
                QString error;
                if (!resolveAtomSet(all, expression, indices, wanted, error))
                    return ToolResult::failure(error);
                atoms = subsetAtoms(all, wanted);
                severed = severedBondCount(all, wanted);
            }

            const QString comment = args.value(QStringLiteral("comment")).toString();
            // QSaveFile so a failed write leaves the old file intact rather than a
            // truncated one.
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
                return ToolResult::failure(QStringLiteral("cannot write %1: %2").arg(path, file.errorString()));
            QTextStream out(&file);
            out << atomsToXyz(atoms, comment);
            if (!file.commit())
                return ToolResult::failure(QStringLiteral("cannot write %1: %2").arg(path, file.errorString()));

            QJsonObject data;
            data.insert(QStringLiteral("path"), path);
            data.insert(QStringLiteral("atom_count"), atoms.size());
            data.insert(QStringLiteral("of_total"), all.size());
            if (restricted)
                data.insert(QStringLiteral("severed_bonds"), severed);

            QString note = QStringLiteral("Wrote %1 atoms to %2.").arg(atoms.size()).arg(path);
            if (severed > 0) {
                note += QStringLiteral(" The selection cuts %1 covalent bond(s), so the saved "
                                       "fragment has open valences.").arg(severed);
            }
            return ToolResult::success(data, note);
        };
        add(spec);
    }

    return added;
}
