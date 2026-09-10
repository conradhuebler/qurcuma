// tools_edit.cpp - Tools that change the structure.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "tools_edit.h"

#include "atomselection.h"
#include "core/toolregistry.h"
#include "fragmentlibrary.h"
#include "scenefiller.h"
#include "view.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace {

QJsonObject schema(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

/// One message for the one reason an edit is refused, rather than each tool
/// inventing its own wording.
ToolResult refusedIfNotEditable(MoleculeViewer* viewer)
{
    if (viewer->simulationActive()) {
        return ToolResult::failure(QStringLiteral(
            "not while a simulation is running: the next frame would overwrite the change, "
            "and a changed atom count would drop the bond graph. Stop the run first."));
    }
    if (!viewer->canEditStructure()) {
        return ToolResult::failure(QStringLiteral(
            "only single-frame structures can be edited, not a trajectory"));
    }
    return ToolResult::success();
}

ToolSpec base(const QString& name, const QString& description, ToolEffect effect,
              const char* schemaJson)
{
    ToolSpec spec;
    spec.name = name;
    spec.category = QStringLiteral("build");
    spec.description = description;
    spec.effect = effect;
    spec.affinity = ToolAffinity::Gui;
    spec.paramSchema = schema(schemaJson);
    return spec;
}

/// True while the structure can actually be edited. All of these refuse during a
/// run and on a trajectory anyway; leaving them out of the catalogue then saves
/// the tokens AND stops a model spending a round being told no.
/// Claude Generated 2026.
std::function<bool()> whenEditable(MoleculeViewer* viewer)
{
    return [viewer] { return viewer && viewer->canEditStructure(); };
}

}  // namespace

namespace {

/// Shared by transform_atoms: the set to move, from a selection expression, from
/// explicit indices, or -- when neither is given -- whatever is selected in the
/// viewer, which is what a follow-up to select_atoms means.
bool setToMove(MoleculeViewer* viewer, const QJsonObject& args, QVector<int>& out, QString& error)
{
    const QString expression = args.value(QStringLiteral("atoms")).toString();
    const QJsonArray indices = args.value(QStringLiteral("atom_indices")).toArray();
    if (expression.isEmpty() && indices.isEmpty()) {
        out = viewer->getSelectedAtoms();
        if (out.isEmpty()) {
            error = QStringLiteral("nothing to move: pass atoms or atom_indices, or select "
                                   "something first");
            return false;
        }
        return true;
    }
    return resolveAtomSet(viewer->getCurrentFrameAtoms(), expression, indices, out, error);
}

}  // namespace

int registerEditTools(ToolRegistry& registry, const EditToolContext& context)
{
    MoleculeViewer* const viewer = context.viewer;
    const auto setContainerWall = context.setContainerWall;
    if (!viewer)
        return 0;
    int added = 0;
    const auto add = [&registry, &added](const ToolSpec& spec) {
        if (registry.add(spec))
            ++added;
    };

    // --- list_fragments -----------------------------------------------------
    {
        ToolSpec spec = base(QStringLiteral("list_fragments"),
            QStringLiteral("The built-in fragments that add_fragment can insert, with their "
                           "category and whether they attach to an existing atom."),
            ToolEffect::Read, R"JSON({"type":"object"})JSON");
        spec.affinity = ToolAffinity::Any;
        spec.handler = [](const QJsonObject&) {
            QJsonArray list;
            for (const build::Fragment& fragment : build::fragmentLibrary()) {
                QJsonObject entry;
                entry.insert(QStringLiteral("name"), fragment.name);
                entry.insert(QStringLiteral("category"), fragment.category);
                entry.insert(QStringLiteral("atom_count"), fragment.atoms.size());
                // A substituent carries an open valence and needs a target atom;
                // a standalone molecule is dropped in as it is.
                entry.insert(QStringLiteral("substituent"), fragment.attachAtom >= 0);
                list.append(entry);
            }
            QJsonObject data;
            data.insert(QStringLiteral("fragments"), list);
            data.insert(QStringLiteral("count"), list.size());
            return ToolResult::success(data);
        };
        add(spec);
    }

    // --- add_atoms ----------------------------------------------------------
    {
        ToolSpec spec = base(QStringLiteral("add_atoms"),
            QStringLiteral("Add atoms at given positions, in Angstrom. Bonds are detected from "
                           "the geometry afterwards, so place them at sensible distances. "
                           "Undoable with Ctrl+Z like any other edit."),
            ToolEffect::Mutate, R"JSON({
              "type": "object",
              "properties": {
                "atoms": { "type": "array",
                           "description": "objects with element, x, y and z" }
              },
              "required": ["atoms"]
            })JSON");
        spec.handler = [viewer](const QJsonObject& args) {
            const ToolResult blocked = refusedIfNotEditable(viewer);
            if (!blocked.ok)
                return blocked;

            const QJsonArray requested = args.value(QStringLiteral("atoms")).toArray();
            if (requested.isEmpty())
                return ToolResult::failure(QStringLiteral("\"atoms\" is empty"));

            QVector<moldata::Atom> atoms;
            for (const QJsonValue& value : requested) {
                const QJsonObject entry = value.toObject();
                const QString element = entry.value(QStringLiteral("element")).toString();
                if (element.isEmpty())
                    return ToolResult::failure(QStringLiteral("every atom needs an \"element\""));
                moldata::Atom atom;
                atom.element = element;
                atom.position = QVector3D(
                    static_cast<float>(entry.value(QStringLiteral("x")).toDouble()),
                    static_cast<float>(entry.value(QStringLiteral("y")).toDouble()),
                    static_cast<float>(entry.value(QStringLiteral("z")).toDouble()));
                atoms.append(atom);
            }

            const int before = viewer->getCurrentFrameAtoms().size();
            viewer->appendMolecule(atoms, {}, /*startPlacement=*/false);
            const int after = viewer->getCurrentFrameAtoms().size();

            QJsonObject data;
            data.insert(QStringLiteral("added"), after - before);
            data.insert(QStringLiteral("first_index"), before);
            data.insert(QStringLiteral("atom_count"), after);
            return ToolResult::success(data);
        };
        spec.available = whenEditable(viewer);
        add(spec);
    }

    // --- add_fragment -------------------------------------------------------
    {
        // The names come from the library, so the model is offered exactly what
        // exists rather than guessing at a spelling.
        QStringList names;
        for (const build::Fragment& fragment : build::fragmentLibrary())
            names << fragment.name;

        QJsonArray enumeration;
        for (const QString& name : names)
            enumeration.append(name);

        ToolSpec spec = base(QStringLiteral("add_fragment"),
            QStringLiteral("Insert one of the built-in fragments. A substituent needs "
                           "\"attach_to\": the atom it should replace a hydrogen on. A standalone "
                           "molecule is placed next to the structure. See list_fragments."),
            ToolEffect::Mutate, R"JSON({
              "type": "object",
              "properties": {
                "name":      { "type": "string" },
                "attach_to": { "type": "integer", "minimum": 0,
                               "description": "atom index for a substituent" }
              },
              "required": ["name"]
            })JSON");
        // Fill the enum in after the fact: it is data, not a literal.
        QJsonObject properties = spec.paramSchema.value(QStringLiteral("properties")).toObject();
        QJsonObject nameProperty = properties.value(QStringLiteral("name")).toObject();
        nameProperty.insert(QStringLiteral("enum"), enumeration);
        properties.insert(QStringLiteral("name"), nameProperty);
        spec.paramSchema.insert(QStringLiteral("properties"), properties);

        spec.handler = [viewer](const QJsonObject& args) {
            const ToolResult blocked = refusedIfNotEditable(viewer);
            if (!blocked.ok)
                return blocked;

            const QString wanted = args.value(QStringLiteral("name")).toString();
            const build::Fragment* found = nullptr;
            for (const build::Fragment& fragment : build::fragmentLibrary()) {
                if (fragment.name == wanted)
                    found = &fragment;
            }
            if (!found)
                return ToolResult::failure(QStringLiteral("no fragment called \"%1\"").arg(wanted));

            const int atomCount = viewer->getCurrentFrameAtoms().size();
            const bool hasTarget = args.contains(QStringLiteral("attach_to"));
            const int target = args.value(QStringLiteral("attach_to")).toInt(-1);

            if (found->attachAtom >= 0 && !hasTarget) {
                return ToolResult::failure(
                    QStringLiteral("\"%1\" is a substituent and needs \"attach_to\"").arg(wanted));
            }
            if (hasTarget && (target < 0 || target >= atomCount)) {
                return ToolResult::failure(QStringLiteral("atom index %1 is outside 0..%2")
                                               .arg(target).arg(atomCount - 1));
            }

            const int before = atomCount;
            if (hasTarget && found->attachAtom >= 0)
                viewer->attachFragment(*found, target);
            else
                viewer->insertFragment(*found);
            const int after = viewer->getCurrentFrameAtoms().size();

            QJsonObject data;
            data.insert(QStringLiteral("fragment"), wanted);
            data.insert(QStringLiteral("atom_count"), after);
            data.insert(QStringLiteral("change"), after - before);
            return ToolResult::success(data);
        };
        spec.available = whenEditable(viewer);
        add(spec);
    }

    // --- add_hydrogens ------------------------------------------------------
    {
        ToolSpec spec = base(QStringLiteral("add_hydrogens"),
            QStringLiteral("Saturate open valences with hydrogens, placed by VSEPR. Without "
                           "\"atoms\" it treats every atom that has an open valence."),
            ToolEffect::Mutate, R"JSON({
              "type": "object",
              "properties": {
                "atoms": { "type": "array", "description": "0-based indices; omit for all" }
              }
            })JSON");
        spec.handler = [viewer](const QJsonObject& args) {
            const ToolResult blocked = refusedIfNotEditable(viewer);
            if (!blocked.ok)
                return blocked;

            const int atomCount = viewer->getCurrentFrameAtoms().size();
            QVector<int> targets;
            for (const QJsonValue& value : args.value(QStringLiteral("atoms")).toArray()) {
                const int index = value.toInt(-1);
                if (index < 0 || index >= atomCount) {
                    return ToolResult::failure(QStringLiteral("atom index %1 is outside 0..%2")
                                                   .arg(index).arg(atomCount - 1));
                }
                targets.append(index);
            }

            viewer->addHydrogens(targets);
            const int after = viewer->getCurrentFrameAtoms().size();

            QJsonObject data;
            data.insert(QStringLiteral("added"), after - atomCount);
            data.insert(QStringLiteral("atom_count"), after);
            return ToolResult::success(data);
        };
        spec.available = whenEditable(viewer);
        add(spec);
    }

    // --- delete_atoms -------------------------------------------------------
    {
        ToolSpec spec = base(QStringLiteral("delete_atoms"),
            QStringLiteral("Remove atoms and the bonds that touched them. Undoable with Ctrl+Z."),
            ToolEffect::Mutate, R"JSON({
              "type": "object",
              "properties": {
                "atoms": { "type": "array", "description": "0-based indices" }
              },
              "required": ["atoms"]
            })JSON");
        spec.handler = [viewer](const QJsonObject& args) {
            const ToolResult blocked = refusedIfNotEditable(viewer);
            if (!blocked.ok)
                return blocked;

            const int atomCount = viewer->getCurrentFrameAtoms().size();
            QVector<int> indices;
            for (const QJsonValue& value : args.value(QStringLiteral("atoms")).toArray()) {
                const int index = value.toInt(-1);
                if (index < 0 || index >= atomCount) {
                    return ToolResult::failure(QStringLiteral("atom index %1 is outside 0..%2")
                                                   .arg(index).arg(atomCount - 1));
                }
                indices.append(index);
            }
            if (indices.isEmpty())
                return ToolResult::failure(QStringLiteral("\"atoms\" is empty"));

            viewer->selectAtoms(indices, /*append=*/false);
            viewer->deleteSelection();

            QJsonObject data;
            data.insert(QStringLiteral("removed"), atomCount - viewer->getCurrentFrameAtoms().size());
            data.insert(QStringLiteral("atom_count"), viewer->getCurrentFrameAtoms().size());
            return ToolResult::success(data);
        };
        spec.available = whenEditable(viewer);
        add(spec);
    }

    // --- transform_atoms ----------------------------------------------------
    {
        ToolSpec spec = base(QStringLiteral("transform_atoms"),
            QStringLiteral(
                "Move a set of atoms rigidly: rotate it about its own centroid, then shift it. "
                "This is how a guest is placed in a cavity -- rotate to the right orientation, "
                "translate into the pocket, then check with get_contacts. Not available while a "
                "simulation runs; pull on the atoms instead."),
            ToolEffect::Mutate, R"JSON({
              "type": "object",
              "properties": {
                "atoms":        { "type": "string",
                                  "description": "selection grammar, e.g. \"F2\" for the second fragment" },
                "atom_indices": { "type": "array",
                                  "description": "explicit 0-based indices, as an alternative to atoms" },
                "translate":    { "type": "array",
                                  "description": "[dx, dy, dz] in Angstrom; omitted means no shift" },
                "rotate_axis":  { "type": "array",
                                  "description": "[x, y, z] axis through the set's centroid" },
                "rotate_degrees": { "type": "number", "minimum": -360, "maximum": 360,
                                    "description": "rotation about rotate_axis, applied before the shift" }
              }
            })JSON");

        spec.handler = [viewer](const QJsonObject& args) {
            const ToolResult blocked = refusedIfNotEditable(viewer);
            if (!blocked.ok)
                return blocked;

            QVector<int> wanted;
            QString error;
            if (!setToMove(viewer, args, wanted, error))
                return ToolResult::failure(error);

            const auto vector3 = [](const QJsonArray& a) {
                return a.size() == 3
                    ? QVector3D(float(a.at(0).toDouble()), float(a.at(1).toDouble()),
                          float(a.at(2).toDouble()))
                    : QVector3D();
            };
            const QJsonArray translateArray = args.value(QStringLiteral("translate")).toArray();
            const QJsonArray axisArray = args.value(QStringLiteral("rotate_axis")).toArray();
            if (!translateArray.isEmpty() && translateArray.size() != 3)
                return ToolResult::failure(QStringLiteral("translate needs three numbers"));
            if (!axisArray.isEmpty() && axisArray.size() != 3)
                return ToolResult::failure(QStringLiteral("rotate_axis needs three numbers"));

            const QVector3D translation = vector3(translateArray);
            const QVector3D axis = vector3(axisArray);
            const double degrees = args.value(QStringLiteral("rotate_degrees")).toDouble();
            if (!qFuzzyIsNull(degrees) && axis.isNull())
                return ToolResult::failure(QStringLiteral("a rotation needs rotate_axis as well"));
            if (translation.isNull() && qFuzzyIsNull(degrees))
                return ToolResult::failure(QStringLiteral("nothing to do: give translate, a "
                                                          "rotation, or both"));

            if (!viewer->transformAtoms(wanted, translation, axis, degrees, &error))
                return ToolResult::failure(error);

            QJsonObject data;
            data.insert(QStringLiteral("moved_atoms"), wanted.size());
            data.insert(QStringLiteral("clashes"), viewer->getCollisionCount());
            return ToolResult::success(data,
                QStringLiteral("Moved %1 atoms; %2 clash(es) now.")
                    .arg(wanted.size()).arg(viewer->getCollisionCount()));
        };
        spec.available = whenEditable(viewer);
        add(spec);
    }

    // --- fill_container -----------------------------------------------------
    //
    // What this is, and is not, matters enough to say twice. It packs randomly
    // oriented copies into a volume with a minimum separation. That is a scene
    // setup -- a gas-phase mixture, or a handful of explicit waters around a
    // binding site -- and it is NOT a solvation box: the packing does not reach
    // liquid density, there are no periodic boundaries in the non-bonded terms
    // (see curcuma's WP-PERIODIC-NONBONDED), and nothing here equilibrates it.
    {
        QJsonArray enumeration;
        for (const build::Fragment& fragment : build::fragmentLibrary())
            enumeration.append(fragment.name);

        ToolSpec spec = base(QStringLiteral("fill_container"),
            QStringLiteral(
                "Pack copies of a library molecule around what is loaded, at random positions "
                "and orientations, keeping a minimum distance to everything already there. "
                "Use it for a microsolvation shell (a few dozen explicit waters around a site) "
                "or a gas-phase mixture. It is NOT a solvation box: the packing does not reach "
                "liquid density, there are no periodic boundaries, and it is not equilibrated -- "
                "optimise afterwards before drawing anything from it."),
            ToolEffect::Mutate, R"JSON({
              "type": "object",
              "properties": {
                "molecule":     { "type": "string" },
                "count":        { "type": "integer", "minimum": 1, "maximum": 2000,
                                  "description": "how many copies to attempt" },
                "shape":        { "type": "string", "enum": ["box", "sphere"],
                                  "description": "default box, wrapped around the structure" },
                "padding":      { "type": "number", "minimum": 0, "maximum": 50,
                                  "description": "Angstrom added around the structure's own extent (default 6)" },
                "radius":       { "type": "number", "minimum": 1, "maximum": 200,
                                  "description": "sphere radius about the origin; overrides padding" },
                "min_distance": { "type": "number", "minimum": 1, "maximum": 10,
                                  "description": "closest approach allowed, in Angstrom (default 2.2)" },
                "seed":         { "type": "integer", "minimum": 0,
                                  "description": "0 draws fresh each time; anything else repeats exactly" },
                "set_wall":     { "type": "boolean",
                                  "description": "make the packed volume the simulation's container, so a run afterwards is held in it (default false)" },
                "wall_potential": { "type": "string", "enum": ["harmonic", "logfermi", "pbc"],
                                  "description": "with set_wall: how the container acts. pbc puts a molecule that leaves back in on the opposite side and does no work on it; the other two push it back. Default pbc" }
              },
              "required": ["molecule", "count"]
            })JSON");
        {
            QJsonObject properties = spec.paramSchema.value(QStringLiteral("properties")).toObject();
            QJsonObject moleculeProperty = properties.value(QStringLiteral("molecule")).toObject();
            moleculeProperty.insert(QStringLiteral("enum"), enumeration);
            properties.insert(QStringLiteral("molecule"), moleculeProperty);
            spec.paramSchema.insert(QStringLiteral("properties"), properties);
        }

        spec.handler = [viewer, setContainerWall](const QJsonObject& args) {
            const ToolResult blocked = refusedIfNotEditable(viewer);
            if (!blocked.ok)
                return blocked;

            const QString wanted = args.value(QStringLiteral("molecule")).toString();
            const build::Fragment* found = nullptr;
            for (const build::Fragment& fragment : build::fragmentLibrary()) {
                if (fragment.name == wanted)
                    found = &fragment;
            }
            if (!found)
                return ToolResult::failure(QStringLiteral("no molecule called \"%1\"").arg(wanted));
            if (found->attachAtom >= 0) {
                return ToolResult::failure(QStringLiteral(
                    "\"%1\" is a substituent with an open valence, not a free molecule; "
                    "packing copies of it would leave dangling bonds").arg(wanted));
            }

            const QVector<moldata::Atom> existing = viewer->getCurrentFrameAtoms();
            build::Container container;
            const QString shape = args.value(QStringLiteral("shape"))
                                      .toString(QStringLiteral("box"));
            if (shape == QLatin1String("sphere")) {
                container.kind = build::Container::Sphere;
                container.radius = float(args.value(QStringLiteral("radius")).toDouble(12.0));
            } else {
                container.kind = build::Container::Box;
                const float padding = float(args.value(QStringLiteral("padding")).toDouble(6.0));
                if (existing.isEmpty()) {
                    container.min = QVector3D(-padding, -padding, -padding);
                    container.max = QVector3D(padding, padding, padding);
                } else {
                    QVector3D lower = existing.first().position;
                    QVector3D upper = lower;
                    for (const moldata::Atom& atom : existing) {
                        lower.setX(qMin(lower.x(), atom.position.x()));
                        lower.setY(qMin(lower.y(), atom.position.y()));
                        lower.setZ(qMin(lower.z(), atom.position.z()));
                        upper.setX(qMax(upper.x(), atom.position.x()));
                        upper.setY(qMax(upper.y(), atom.position.y()));
                        upper.setZ(qMax(upper.z(), atom.position.z()));
                    }
                    const QVector3D pad(padding, padding, padding);
                    container.min = lower - pad;
                    container.max = upper + pad;
                }
            }

            const int count = args.value(QStringLiteral("count")).toInt();
            const float minDistance =
                float(args.value(QStringLiteral("min_distance")).toDouble(2.2));
            const quint32 seed = quint32(args.value(QStringLiteral("seed")).toInt(0));

            const build::FillResult result = build::fillContainer(
                { { found, count } }, container, existing, minDistance, 2000, seed);
            if (result.atoms.isEmpty()) {
                return ToolResult::failure(QStringLiteral(
                    "nothing fitted: the volume is too small for even one copy at a %1 A "
                    "separation").arg(minDistance));
            }

            viewer->appendMolecule(result.atoms, result.bonds, false);

            QJsonObject data;
            data.insert(QStringLiteral("placed"), result.placed);
            data.insert(QStringLiteral("requested"), result.requested);
            data.insert(QStringLiteral("added_atoms"), result.atoms.size());
            data.insert(QStringLiteral("total_atoms"), existing.size() + result.atoms.size());
            data.insert(QStringLiteral("shape"), shape);
            if (container.kind == build::Container::Sphere) {
                data.insert(QStringLiteral("radius"), container.radius);
            } else {
                data.insert(QStringLiteral("bounds_min"),
                    QJsonArray { container.min.x(), container.min.y(), container.min.z() });
                data.insert(QStringLiteral("bounds_max"),
                    QJsonArray { container.max.x(), container.max.y(), container.max.z() });
            }

            // The packed volume becomes the container the run is held in, which is
            // what makes the box a box rather than a cloud that expands on the
            // first step.
            if (args.value(QStringLiteral("set_wall")).toBool()) {
                const QString potential = args.value(QStringLiteral("wall_potential"))
                                              .toString(QStringLiteral("pbc"));
                ToolContainer wall;
                wall.sphere = container.kind == build::Container::Sphere;
                wall.radius = container.radius;
                wall.min = container.min;
                wall.max = container.max;
                wall.potential = potential == QLatin1String("pbc") ? 2
                    : potential == QLatin1String("logfermi")       ? 1
                                                                   : 0;
                QString wallError;
                if (setContainerWall && setContainerWall(wall, &wallError)) {
                    data.insert(QStringLiteral("wall_set"), true);
                    data.insert(QStringLiteral("wall_potential"), potential);
                } else {
                    data.insert(QStringLiteral("wall_set"), false);
                    data.insert(QStringLiteral("wall_error"), wallError.isEmpty()
                            ? QStringLiteral("no simulation dock to set it on")
                            : wallError);
                }
            }

            QString note = QStringLiteral("Placed %1 of %2 copies of %3 (%4 atoms, %5 in the "
                                          "scene now).")
                               .arg(result.placed).arg(result.requested).arg(wanted)
                               .arg(result.atoms.size())
                               .arg(existing.size() + result.atoms.size());
            if (result.placed < result.requested) {
                note += QStringLiteral(" The rest did not fit at a %1 A separation; a larger "
                                       "volume or a smaller min_distance takes more.")
                            .arg(minDistance);
            }
            if (data.value(QStringLiteral("wall_set")).toBool()) {
                note += QStringLiteral(" The container is now the simulation's wall, so a run "
                                       "is held in it.");
            }
            note += QStringLiteral(" This is a random packing, not an equilibrated liquid: run it "
                                   "and watch the energy settle before reading anything out of it.");
            return ToolResult::success(data, note);
        };
        spec.available = whenEditable(viewer);
        add(spec);
    }

    return added;
}
