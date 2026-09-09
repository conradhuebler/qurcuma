// tools_edit.cpp - Tools that change the structure.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "tools_edit.h"

#include "core/toolregistry.h"
#include "fragmentlibrary.h"
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

}  // namespace

int registerEditTools(ToolRegistry& registry, const EditToolContext& context)
{
    MoleculeViewer* const viewer = context.viewer;
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
        add(spec);
    }

    return added;
}
