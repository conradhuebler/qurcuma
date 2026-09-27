// tools_files.cpp - Bringing a structure in from disk, and writing one back out.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "tools_files.h"

#include "atomselection.h"
#include "core/toolregistry.h"
#include "lesson.h"              // atomsToXyz
#include "moleculefileloader.h"
#include "view.h"

#include <src/core/elements.h>
#include <src/tools/cif.h>

#include <QDir>
#include <QMap>
#include <QRegularExpression>
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

}  // namespace

/// A relative path means "in the working directory", the same place list_workdir
/// reports. An absolute path is taken as given.
QString resolveToolPath(const QString& path, const std::function<QString()>& workingDirectory)
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

/// Claude Generated 2026 - How a cif is built, from the arguments merge_structure,
/// open_structure and run_single_point share. @p unitCellByDefault: merge keeps its
/// documented unit cell, open follows the GUI (asymmetric unit).
MoleculeFileLoader::CifOptions cifOptionsFromArgs(const QJsonObject& args, bool unitCellByDefault)
{
    MoleculeFileLoader::CifOptions options;
    const QString content = args.value(QStringLiteral("cif_content")).toString();
    options.unitCell = content.isEmpty() ? unitCellByDefault : content == QLatin1String("unit_cell");
    if (args.contains(QStringLiteral("disorder_group")))
        options.disorderGroup = args.value(QStringLiteral("disorder_group")).toInt();
    options.completeMolecules = options.unitCell
        && args.value(QStringLiteral("complete_molecules")).toBool();
    return options;
}

namespace {

/// What the scene now holds of a cif, in the words and numbers a model needs.
void describeLoadedCif(const MoleculeFileLoader::CifInfo& cif, QJsonObject& data, QString& note)
{
    data.insert(QStringLiteral("cif_content"), cif.unitCell
            ? QStringLiteral("unit_cell") : QStringLiteral("asymmetric_unit"));
    data.insert(QStringLiteral("sites_in_file"), cif.asymmetricAtoms);
    data.insert(QStringLiteral("symmetry_operations"), cif.symmetryOperations);
    data.insert(QStringLiteral("atoms_asymmetric_unit"), cif.unitAtoms);
    data.insert(QStringLiteral("atoms_unit_cell"), cif.cellAtoms);
    if (cif.unitCell)
        data.insert(QStringLiteral("complete_molecules"), cif.completeMolecules);
    QJsonArray groups;
    for (const MoleculeFileLoader::CifDisorderGroup& group : cif.disorderGroups) {
        groups.append(QJsonObject { { QStringLiteral("group"), group.group },
            { QStringLiteral("sites"), group.sites },
            { QStringLiteral("mean_occupancy"), group.meanOccupancy } });
    }
    if (!groups.isEmpty()) {
        data.insert(QStringLiteral("disorder_groups"), groups);
        data.insert(QStringLiteral("disorder_group_shown"), cif.shownGroup);
    }
    note += QStringLiteral(" cif: %1 sites in the file, %2 symmetry operation(s); shown: %3")
                .arg(cif.asymmetricAtoms).arg(cif.symmetryOperations)
                .arg(cif.unitCell ? QStringLiteral("unit cell %1x%2x%3%4").arg(cif.na).arg(cif.nb)
                                        .arg(cif.nc).arg(cif.completeMolecules
                                                ? QStringLiteral(", molecules completed") : QString())
                                  : QStringLiteral("asymmetric unit"));
    if (!groups.isEmpty()) {
        note += cif.shownGroup == 0
            ? QStringLiteral(", every disorder alternative (they overlap)")
            : QStringLiteral(", disorder group %1 of %2").arg(cif.shownGroup).arg(groups.size());
    }
    note += QLatin1Char('.');
}

/// Element counts of a formula such as "C16 H41 Cl4 N7 Si2". Claude Generated 2026.
QMap<QString, int> parseFormula(const QString& formula)
{
    QMap<QString, int> counts;
    static const QRegularExpression token(QStringLiteral("([A-Z][a-z]?)(\\d*)"));
    auto it = token.globalMatch(formula);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        counts[m.captured(1)] += m.captured(2).isEmpty() ? 1 : m.captured(2).toInt();
    }
    return counts;
}

QMap<QString, int> elementCounts(const curcuma::Molecule& molecule)
{
    QMap<QString, int> counts;
    for (int i = 0; i < molecule.AtomCount(); ++i) {
        const int z = molecule.Atom(i).first;
        if (z > 0 && z < int(Elements::ElementAbbr.size()))
            counts[QString::fromStdString(Elements::ElementAbbr[size_t(z)])] += 1;
    }
    return counts;
}

QString formulaText(const QMap<QString, int>& counts)
{
    QStringList parts;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        parts << (it.value() == 1 ? it.key() : QStringLiteral("%1%2").arg(it.key()).arg(it.value()));
    return parts.join(QLatin1Char(' '));
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
            "may overlap -- check with get_contacts. xyz, vtf, pdb, mol2 and cif; of a "
            "multi-frame file only the first frame. A cif arrives as its unit cell (every "
            "symmetry image) unless cif_content says asymmetric_unit, with one disorder "
            "conformation (the major one unless disorder_group says otherwise), and can be "
            "replicated on the way in with supercell.");
        spec.effect = ToolEffect::Mutate;
        spec.affinity = ToolAffinity::Gui;
        spec.paramSchema = schema(R"JSON({
          "type": "object",
          "properties": {
            "path": { "type": "string",
                      "description": "file to read; relative to the working directory" },
            "supercell": { "type": "array",
                           "description": "cif only: [a, b, c] repeats along the cell vectors, built before the atoms arrive. Omitted means the cell as written" },
            "cif_content": { "type": "string", "enum": ["unit_cell", "asymmetric_unit"],
                           "description": "cif only: every symmetry image (default), or the sites as the file writes them" },
            "disorder_group": { "type": "integer", "minimum": 0,
                           "description": "cif only: the disorder group (SHELX PART) to build, 0 for every alternative at once; omitted means the group with the highest occupancy" },
            "complete_molecules": { "type": "boolean",
                           "description": "cif unit cell only: put molecules cut by the cell faces back together (default false)" }
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

            const QString path = resolveToolPath(args.value(QStringLiteral("path")).toString(),
                workingDirectory);
            if (!QFileInfo::exists(path))
                return ToolResult::failure(QStringLiteral("no such file: %1").arg(path));

            // A cif is built on the way in: unit cell or asymmetric unit, one
            // disorder group, repeats. All of it happens while reading, in curcuma,
            // where the cell lives -- qurcuma's atom records carry no cell, so a
            // structure that has arrived here can no longer be replicated.
            const bool isCif = QFileInfo(path).suffix().compare(QLatin1String("cif"),
                                   Qt::CaseInsensitive) == 0;
            MoleculeFileLoader::CifOptions cifOptions = cifOptionsFromArgs(args, /*unitCellByDefault=*/true);
            const QJsonArray repeats = args.value(QStringLiteral("supercell")).toArray();
            if (!repeats.isEmpty()) {
                if (repeats.size() != 3)
                    return ToolResult::failure(QStringLiteral("supercell needs three numbers"));
                if (!isCif)
                    return ToolResult::failure(QStringLiteral(
                        "only a cif carries a unit cell, so only a cif can be replicated"));
                if (!cifOptions.unitCell)
                    return ToolResult::failure(QStringLiteral(
                        "a supercell repeats the unit cell; drop cif_content asymmetric_unit"));
                cifOptions.na = repeats.at(0).toInt();
                cifOptions.nb = repeats.at(1).toInt();
                cifOptions.nc = repeats.at(2).toInt();
            }

            const MoleculeFileLoader::Result parsed = MoleculeFileLoader::load(path, cifOptions);
            if (!parsed.supported) {
                return ToolResult::failure(
                    QStringLiteral("%1 is not a structure format this reads (xyz, vtf, pdb, mol2, cif)")
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
            if (parsed.cif.present) {
                describeLoadedCif(parsed.cif, data, note);
                for (const QString& cifNote : parsed.cif.notes)
                    note += QStringLiteral(" %1.").arg(cifNote);
            }
            return ToolResult::success(data, note);
        };
        add(spec);
    }

    // --- open_structure -----------------------------------------------------
    //
    // Claude Generated 2026 - Replacing the scene is what "load this file" means;
    // merge_structure adds to it, and a model that only had merge piled a whole
    // unit cell onto the molecule already shown. Goes through MainWindow's load,
    // so the Unit Cell dock, the cell drawing and the save path follow.
    const auto openStructure = context.openStructure;
    if (openStructure) {
        ToolSpec spec;
        spec.name = QStringLiteral("open_structure");
        spec.category = QStringLiteral("build");
        spec.description = QStringLiteral(
            "Open a structure file in place of the current scene (merge_structure adds to it "
            "instead). xyz, vtf, pdb, mol2, cif. A cif opens as its asymmetric unit -- the "
            "atoms the file lists, as Avogadro and Mercury show it -- with the major disorder "
            "group, unless cif_content / disorder_group say otherwise. describe_cif tells what "
            "a cif contains before opening it.");
        spec.effect = ToolEffect::Mutate;
        spec.affinity = ToolAffinity::Gui;
        spec.paramSchema = schema(R"JSON({
          "type": "object",
          "properties": {
            "path": { "type": "string", "description": "file to open; relative to the working directory" },
            "cif_content": { "type": "string", "enum": ["asymmetric_unit", "unit_cell"],
                             "description": "cif only: the sites as written (default), or every symmetry image" },
            "disorder_group": { "type": "integer", "minimum": 0,
                             "description": "cif only: disorder group (SHELX PART) to build, 0 = every alternative at once; omitted = the group with the highest occupancy" },
            "complete_molecules": { "type": "boolean",
                             "description": "cif unit cell only: put molecules cut by the cell faces back together" },
            "supercell": { "type": "array",
                           "description": "cif unit cell only: [a, b, c] repeats" }
          },
          "required": ["path"]
        })JSON");
        spec.handler = [viewer, workingDirectory, openStructure](const QJsonObject& args) {
            const QString path = resolveToolPath(args.value(QStringLiteral("path")).toString(), workingDirectory);
            MoleculeFileLoader::CifOptions options = cifOptionsFromArgs(args, /*unitCellByDefault=*/false);
            const QJsonArray repeats = args.value(QStringLiteral("supercell")).toArray();
            if (!repeats.isEmpty()) {
                if (repeats.size() != 3)
                    return ToolResult::failure(QStringLiteral("supercell needs three numbers"));
                if (!options.unitCell)
                    return ToolResult::failure(QStringLiteral(
                        "a supercell repeats the unit cell: add cif_content unit_cell"));
                options.na = repeats.at(0).toInt();
                options.nb = repeats.at(1).toInt();
                options.nc = repeats.at(2).toInt();
            }
            QString error;
            if (!openStructure(path, options, &error))
                return ToolResult::failure(error);

            QJsonObject data;
            data.insert(QStringLiteral("path"), path);
            data.insert(QStringLiteral("atom_count"), viewer->getCurrentFrameAtoms().size());
            QString note = QStringLiteral("Opened %1: %2 atoms in the scene.")
                               .arg(QFileInfo(path).fileName())
                               .arg(viewer->getCurrentFrameAtoms().size());
            if (QFileInfo(path).suffix().compare(QLatin1String("cif"), Qt::CaseInsensitive) == 0) {
                // Read again for the description; cheap next to the load it mirrors.
                const MoleculeFileLoader::Result r = MoleculeFileLoader::load(path, options);
                if (r.cif.present)
                    describeLoadedCif(r.cif, data, note);
            }
            return ToolResult::success(data, note);
        };
        add(spec);
    }

    // --- describe_cif -------------------------------------------------------
    //
    // Claude Generated 2026 - What a cif holds, before anything is built from it.
    // Without it a model looked at a unit cell cut into 64 fragments and went
    // looking for two independent molecules, when the "two conformations" were the
    // file's two disorder groups. Read-only.
    const auto shownCif = context.shownCif;
    {
        ToolSpec spec;
        spec.name = QStringLiteral("describe_cif");
        spec.category = QStringLiteral("structure");
        spec.description = QStringLiteral(
            "What a cif contains, without changing the scene: space group, Z, formula and the "
            "moiety formula (which particles carry which charge), cell, sites, symmetry "
            "operations, disorder groups (SHELX PART, e.g. two conformations of a disordered "
            "part) with their occupancy, displacement parameters, and how many atoms each "
            "choice builds, checked against Z x formula. Without path: the cif shown now.");
        spec.effect = ToolEffect::Read;
        spec.affinity = ToolAffinity::Gui;
        spec.paramSchema = schema(R"JSON({
          "type": "object",
          "properties": {
            "path": { "type": "string", "description": "cif file; relative to the working directory; omitted = the cif shown in the viewer" }
          }
        })JSON");
        spec.handler = [workingDirectory, shownCif](const QJsonObject& args) {
            QString path = args.value(QStringLiteral("path")).toString();
            path = path.isEmpty() ? (shownCif ? shownCif() : QString()) : resolveToolPath(path, workingDirectory);
            if (path.isEmpty())
                return ToolResult::failure(QStringLiteral("no path given and no cif is shown"));
            if (!QFileInfo::exists(path))
                return ToolResult::failure(QStringLiteral("no such file: %1").arg(path));

            const curcuma::CifData cif = curcuma::ReadCifData(path.toStdString());
            if (!cif.ok())
                return ToolResult::failure(QString::fromStdString(cif.error));

            QJsonObject data;
            data.insert(QStringLiteral("path"), path);
            if (!cif.space_group.empty())
                data.insert(QStringLiteral("space_group"), QString::fromStdString(cif.space_group));
            if (cif.space_group_number > 0)
                data.insert(QStringLiteral("space_group_number"), cif.space_group_number);
            if (cif.formula_units_z > 0)
                data.insert(QStringLiteral("z"), cif.formula_units_z);
            if (!cif.formula_sum.empty())
                data.insert(QStringLiteral("formula_sum"), QString::fromStdString(cif.formula_sum));
            if (!cif.formula_moiety.empty())
                data.insert(QStringLiteral("formula_moiety"), QString::fromStdString(cif.formula_moiety));
            if (cif.cell.valid) {
                data.insert(QStringLiteral("cell"), QJsonObject {
                    { QStringLiteral("a"), cif.cell.a }, { QStringLiteral("b"), cif.cell.b },
                    { QStringLiteral("c"), cif.cell.c }, { QStringLiteral("alpha"), cif.cell.alpha },
                    { QStringLiteral("beta"), cif.cell.beta }, { QStringLiteral("gamma"), cif.cell.gamma } });
            }
            data.insert(QStringLiteral("sites_in_file"), int(cif.sites.size()));
            data.insert(QStringLiteral("symmetry_operations"), int(cif.operations.size()));
            int aniso = 0, iso = 0;
            for (const curcuma::CifSite& site : cif.sites)
                site.has_aniso ? ++aniso : (site.has_iso ? ++iso : 0);
            data.insert(QStringLiteral("anisotropic_sites"), aniso);
            data.insert(QStringLiteral("isotropic_sites"), iso);

            // Every choice of content x disorder group, and what it builds.
            const auto count = [&cif](curcuma::CifContent content, bool select, int group) {
                curcuma::CifBuildOptions options;
                options.content = content;
                options.select_disorder_group = select;
                options.disorder_group = group;
                return curcuma::BuildCif(cif, options);
            };
            QJsonArray builds;
            const auto addBuild = [&](const QString& label, bool select, int group) {
                const curcuma::Molecule unit = count(curcuma::CifContent::AsymmetricUnit, select, group);
                const curcuma::Molecule cell = count(curcuma::CifContent::UnitCell, select, group);
                builds.append(QJsonObject { { QStringLiteral("disorder"), label },
                    { QStringLiteral("atoms_asymmetric_unit"), int(unit.AtomCount()) },
                    { QStringLiteral("atoms_unit_cell"), int(cell.AtomCount()) },
                    { QStringLiteral("formula_asymmetric_unit"), formulaText(elementCounts(unit)) } });
            };
            QJsonArray groups;
            for (const curcuma::CifDisorderGroup& group : cif.disorder_groups) {
                QStringList assemblies;
                for (const curcuma::CifSite& site : cif.sites)
                    if (std::abs(site.disorder_group) == group.group && !site.disorder_assembly.empty()
                        && !assemblies.contains(QString::fromStdString(site.disorder_assembly)))
                        assemblies << QString::fromStdString(site.disorder_assembly);
                groups.append(QJsonObject { { QStringLiteral("group"), group.group },
                    { QStringLiteral("sites"), group.sites },
                    { QStringLiteral("mean_occupancy"), group.mean_occupancy },
                    { QStringLiteral("assemblies"), QJsonArray::fromStringList(assemblies) } });
                addBuild(QStringLiteral("group %1").arg(group.group), true, group.group);
            }
            addBuild(cif.disorder_groups.empty() ? QStringLiteral("ordered") : QStringLiteral("all alternatives"),
                false, 0);
            if (!groups.isEmpty()) {
                data.insert(QStringLiteral("disorder_groups"), groups);
                data.insert(QStringLiteral("major_disorder_group"), cif.majorDisorderGroup());
            }
            data.insert(QStringLiteral("builds"), builds);

            QString note = QStringLiteral("%1: %2%3, %4 sites, %5 symmetry operation(s).")
                               .arg(QFileInfo(path).fileName())
                               .arg(cif.space_group.empty() ? QStringLiteral("space group not given")
                                                            : QString::fromStdString(cif.space_group))
                               .arg(cif.formula_units_z > 0 ? QStringLiteral(", Z = %1").arg(cif.formula_units_z) : QString())
                               .arg(cif.sites.size()).arg(cif.operations.size());

            // Z' and the formula check: the asymmetric unit of one conformation
            // should hold Z/ops formula units.
            if (!cif.formula_sum.empty() && cif.formula_units_z > 0 && !cif.operations.empty()) {
                const double zPrime = double(cif.formula_units_z) / double(cif.operations.size());
                data.insert(QStringLiteral("z_prime"), zPrime);
                const QMap<QString, int> formula = parseFormula(QString::fromStdString(cif.formula_sum));
                const curcuma::Molecule unit = count(curcuma::CifContent::AsymmetricUnit,
                    !cif.disorder_groups.empty(), cif.majorDisorderGroup());
                const QMap<QString, int> built = elementCounts(unit);
                QStringList differences;
                QSet<QString> elements;
                for (auto it = formula.cbegin(); it != formula.cend(); ++it) elements.insert(it.key());
                for (auto it = built.cbegin(); it != built.cend(); ++it) elements.insert(it.key());
                for (const QString& element : elements) {
                    const double expected = formula.value(element) * zPrime;
                    const int have = built.value(element);
                    if (std::abs(expected - have) > 1e-6)
                        differences << QStringLiteral("%1 %2 instead of %3").arg(element).arg(have).arg(expected);
                }
                data.insert(QStringLiteral("formula_check"), differences.isEmpty()
                        ? QStringLiteral("matches") : differences.join(QStringLiteral("; ")));
                note += QStringLiteral(" Z' = %1. The asymmetric unit (%2) %3 Z' x %4.")
                            .arg(zPrime).arg(cif.disorder_groups.empty() ? QStringLiteral("ordered")
                                                                          : QStringLiteral("major group"))
                            .arg(differences.isEmpty() ? QStringLiteral("matches")
                                                       : QStringLiteral("does NOT match (%1)").arg(differences.join(QStringLiteral("; "))))
                            .arg(QString::fromStdString(cif.formula_sum));
            }
            if (!cif.formula_moiety.empty())
                note += QStringLiteral(" Moiety: %1 -- use it for the charge of whatever you calculate.")
                            .arg(QString::fromStdString(cif.formula_moiety));
            if (!cif.disorder_groups.empty()) {
                note += QStringLiteral(" %1 disorder groups: alternative positions of the same part "
                                       "(two conformations when there are two), sharing the ordered "
                                       "atoms. To compare them: for each group, open_structure (or "
                                       "run_single_point with cif_path) with cif_content "
                                       "asymmetric_unit and disorder_group n; relax the hydrogens "
                                       "first (run_simulation mode opt, hold_atoms heavy) because "
                                       "X-ray X-H bonds are short; then subtract the energies. That "
                                       "is the energy difference in the crystal geometry, packing "
                                       "not included.")
                            .arg(cif.disorder_groups.size());
            }
            for (const std::string& cifNote : cif.notes)
                note += QStringLiteral(" %1.").arg(QString::fromStdString(cifNote));
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

            const QString path = resolveToolPath(args.value(QStringLiteral("path")).toString(),
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
