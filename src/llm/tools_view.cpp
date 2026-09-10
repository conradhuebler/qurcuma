// tools_view.cpp - Read-only tools over the loaded structure and the viewer.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "tools_view.h"

#include "atomselection.h"
#include "core/toolregistry.h"
#include "measurements.h"
#include "moleculebridge.h"
#include "imagemetadata.h"
#include "view.h"

#include <QDir>
#include <QFile>
#include <QTemporaryFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QPair>
#include <QSet>

#include <algorithm>
#include <vector>

namespace {

/// Nothing hands back an unbounded list. A structure can carry a hundred thousand
/// atoms and every result travels into a model's context.
constexpr int kMaxAtomsPerCall = 500;
constexpr int kMaxIndices = 1000;
constexpr int kMaxFiles = 200;

QJsonObject schema(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

ToolResult noStructure()
{
    return ToolResult::failure(QStringLiteral("no structure is loaded"));
}

QJsonArray toJsonArray(const QVector<int>& values, int cap, bool* truncated)
{
    QJsonArray array;
    const int n = qMin(values.size(), cap);
    for (int i = 0; i < n; ++i)
        array.append(values.at(i));
    if (truncated)
        *truncated = values.size() > n;
    return array;
}

/// Hill order: carbon, then hydrogen, then the rest alphabetically.
QString empiricalFormula(const QVector<MoleculeViewer::Atom>& atoms)
{
    QMap<QString, int> counts;
    for (const MoleculeViewer::Atom& a : atoms) {
        const QString e = a.element.isEmpty() ? QStringLiteral("?") : a.element;
        counts[e] += 1;
    }
    const auto piece = [&counts](const QString& element) {
        const int n = counts.value(element);
        if (n == 0)
            return QString();
        return n == 1 ? element : QStringLiteral("%1%2").arg(element).arg(n);
    };
    QString formula = piece(QStringLiteral("C")) + piece(QStringLiteral("H"));
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        if (it.key() == QLatin1String("C") || it.key() == QLatin1String("H"))
            continue;
        formula += it.value() == 1 ? it.key() : QStringLiteral("%1%2").arg(it.key()).arg(it.value());
    }
    return formula;
}

ToolSpec base(const QString& name, const QString& category, const QString& description,
              ToolEffect effect, const char* schemaJson)
{
    ToolSpec spec;
    spec.name = name;
    spec.category = category;
    spec.description = description;
    spec.effect = effect;
    spec.affinity = ToolAffinity::Gui;  // every one of these touches the viewer
    spec.paramSchema = schema(schemaJson);
    return spec;
}

}  // namespace

int registerViewTools(ToolRegistry& registry, const ViewToolContext& context)
{
    MoleculeViewer* const viewer = context.viewer;
    if (!viewer)
        return 0;
    int added = 0;
    const auto add = [&registry, &added](const ToolSpec& spec) {
        if (registry.add(spec))
            ++added;
    };

    // --- get_structure_summary ---------------------------------------------
    {
        ToolSpec spec = base(QStringLiteral("get_structure_summary"), QStringLiteral("structure"),
            QStringLiteral("Overview of what is loaded: atom and bond count, empirical formula, "
                           "trajectory length, current frame, and whether a simulation is running."),
            ToolEffect::Read, R"JSON({"type":"object"})JSON");
        spec.handler = [viewer](const QJsonObject&) {
            const QVector<MoleculeViewer::Atom> atoms = viewer->getCurrentFrameAtoms();
            QJsonObject data;
            data.insert(QStringLiteral("atom_count"), atoms.size());
            data.insert(QStringLiteral("bond_count"), viewer->getCurrentFrameBonds().size());
            data.insert(QStringLiteral("formula"), empiricalFormula(atoms));
            data.insert(QStringLiteral("frame_count"), viewer->getFrameCount());
            data.insert(QStringLiteral("current_frame"), viewer->getCurrentFrame());
            // Centre and extent, so nobody has to page through every atom to find
            // out how big the thing is or where to put a box around it.
            QVector3D lower = atoms.first().position;
            QVector3D upper = lower;
            QVector3D centre;
            for (const MoleculeViewer::Atom& atom : atoms) {
                lower.setX(qMin(lower.x(), atom.position.x()));
                lower.setY(qMin(lower.y(), atom.position.y()));
                lower.setZ(qMin(lower.z(), atom.position.z()));
                upper.setX(qMax(upper.x(), atom.position.x()));
                upper.setY(qMax(upper.y(), atom.position.y()));
                upper.setZ(qMax(upper.z(), atom.position.z()));
                centre += atom.position;
            }
            centre /= float(atoms.size());
            const auto triple = [](const QVector3D& v) {
                return QJsonArray { v.x(), v.y(), v.z() };
            };
            data.insert(QStringLiteral("centre"), triple(centre));
            data.insert(QStringLiteral("bounds_min"), triple(lower));
            data.insert(QStringLiteral("bounds_max"), triple(upper));
            data.insert(QStringLiteral("extent"), triple(upper - lower));

            data.insert(QStringLiteral("selected_count"), viewer->getSelectedAtoms().size());
            data.insert(QStringLiteral("simulation_running"), viewer->simulationActive());
            data.insert(QStringLiteral("structure_editable"), viewer->canEditStructure());
            if (atoms.isEmpty())
                return ToolResult::success(data, QStringLiteral("No structure is loaded."));
            return ToolResult::success(data);
        };
        add(spec);
    }

    // --- list_atoms ---------------------------------------------------------
    {
        ToolSpec spec = base(QStringLiteral("list_atoms"), QStringLiteral("structure"),
            QStringLiteral("Atoms of the current frame with element, position and partial charge. "
                           "Paged: pass offset to continue, at most 500 per call."),
            ToolEffect::Read, R"JSON({
              "type": "object",
              "properties": {
                "offset": { "type": "integer", "minimum": 0, "description": "first atom index" },
                "limit":  { "type": "integer", "minimum": 1, "maximum": 500,
                            "description": "how many at most (default 100)" }
              }
            })JSON");
        spec.handler = [viewer](const QJsonObject& args) {
            const QVector<MoleculeViewer::Atom> atoms = viewer->getCurrentFrameAtoms();
            if (atoms.isEmpty())
                return noStructure();
            const int offset = args.value(QStringLiteral("offset")).toInt(0);
            const int limit = qBound(1, args.value(QStringLiteral("limit")).toInt(100), kMaxAtomsPerCall);
            if (offset >= atoms.size()) {
                return ToolResult::failure(QStringLiteral("offset %1 is past the last atom (%2 atoms)")
                                               .arg(offset).arg(atoms.size()));
            }
            const int end = qMin(offset + limit, atoms.size());

            QJsonArray array;
            for (int i = offset; i < end; ++i) {
                const MoleculeViewer::Atom& a = atoms.at(i);
                QJsonObject entry;
                entry.insert(QStringLiteral("index"), i);
                entry.insert(QStringLiteral("element"), a.element);
                entry.insert(QStringLiteral("x"), a.position.x());
                entry.insert(QStringLiteral("y"), a.position.y());
                entry.insert(QStringLiteral("z"), a.position.z());
                if (a.charge != 0.0f)
                    entry.insert(QStringLiteral("charge"), a.charge);
                if (!a.type.isEmpty())
                    entry.insert(QStringLiteral("type"), a.type);
                array.append(entry);
            }

            QJsonObject data;
            data.insert(QStringLiteral("atoms"), array);
            data.insert(QStringLiteral("offset"), offset);
            data.insert(QStringLiteral("returned"), array.size());
            data.insert(QStringLiteral("total"), atoms.size());
            data.insert(QStringLiteral("next_offset"), end);
            ToolResult result = ToolResult::success(data);
            result.truncated = end < atoms.size();
            return result;
        };
        add(spec);
    }

    // --- get_selection ------------------------------------------------------
    {
        ToolSpec spec = base(QStringLiteral("get_selection"), QStringLiteral("structure"),
            QStringLiteral("Indices of the currently selected atoms."),
            ToolEffect::Read, R"JSON({"type":"object"})JSON");
        spec.handler = [viewer](const QJsonObject&) {
            const QVector<int> selected = viewer->getSelectedAtoms();
            bool truncated = false;
            QJsonObject data;
            data.insert(QStringLiteral("indices"), toJsonArray(selected, kMaxIndices, &truncated));
            data.insert(QStringLiteral("count"), selected.size());
            ToolResult result = ToolResult::success(data);
            result.truncated = truncated;
            return result;
        };
        add(spec);
    }

    // --- select_atoms -------------------------------------------------------
    {
        ToolSpec spec = base(QStringLiteral("select_atoms"), QStringLiteral("structure"),
            QStringLiteral("Select atoms, either by explicit indices or with curcuma's selection "
                           "grammar. Mind the numbering: the grammar is ONE-based -- \"1:10\" is the "
                           "first ten atoms, \"F1\" is the first fragment, \"-1\" is everything -- "
                           "while \"indices\" and every index this tool reports are ZERO-based. When "
                           "in doubt use get_fragments, which hands back a ready-made selector per "
                           "fragment. Pass neither argument to clear the selection."),
            ToolEffect::Display, R"JSON({
              "type": "object",
              "properties": {
                "expression": { "type": "string",  "description": "ONE-based selection grammar: 1:10,15 or F1 or -1 for all" },
                "indices":    { "type": "array",   "description": "explicit 0-based atom indices" },
                "append":     { "type": "boolean", "description": "add to the current selection" }
              }
            })JSON");
        spec.handler = [viewer](const QJsonObject& args) {
            const QVector<MoleculeViewer::Atom> atoms = viewer->getCurrentFrameAtoms();
            if (atoms.isEmpty())
                return noStructure();

            const QString expression = args.value(QStringLiteral("expression")).toString();
            const QJsonArray explicitIndices = args.value(QStringLiteral("indices")).toArray();
            const bool append = args.value(QStringLiteral("append")).toBool(false);

            if (expression.isEmpty() && explicitIndices.isEmpty()) {
                viewer->clearSelection();
                QJsonObject data;
                data.insert(QStringLiteral("count"), 0);
                return ToolResult::success(data, QStringLiteral("Selection cleared."));
            }
            if (!expression.isEmpty() && !explicitIndices.isEmpty()) {
                return ToolResult::failure(
                    QStringLiteral("pass either \"expression\" or \"indices\", not both"));
            }

            QVector<int> wanted;
            QString error;
            if (!resolveAtomSet(atoms, expression, explicitIndices, wanted, error))
                return ToolResult::failure(error);

            viewer->selectAtoms(wanted, append);
            QJsonObject data;
            data.insert(QStringLiteral("count"), viewer->getSelectedAtoms().size());
            data.insert(QStringLiteral("requested"), wanted.size());
            return ToolResult::success(data);
        };
        add(spec);
    }

    // --- get_fragments ------------------------------------------------------
    {
        ToolSpec spec = base(QStringLiteral("get_fragments"), QStringLiteral("structure"),
            QStringLiteral("Connected fragments of the current frame, as curcuma perceives them. "
                           "Each entry carries a ready-made \"selector\" string -- pass that verbatim "
                           "to select_atoms or get_contacts instead of building one yourself."),
            ToolEffect::Read, R"JSON({"type":"object"})JSON");
        spec.handler = [viewer](const QJsonObject&) {
            const QVector<MoleculeViewer::Atom> atoms = viewer->getCurrentFrameAtoms();
            if (atoms.isEmpty())
                return noStructure();
            curcuma::Molecule molecule = atomsToMolecule(atoms);
            const std::vector<std::vector<int>> fragments = molecule.GetFragments();

            QJsonArray array;
            for (size_t i = 0; i < fragments.size(); ++i) {
                QJsonObject entry;
                entry.insert(QStringLiteral("fragment"), static_cast<int>(i));
                // The selector is handed over ready-made. The grammar is one-based
                // while this "fragment" number is zero-based like everything else
                // here, and a model asked to bridge that gap gets it wrong.
                entry.insert(QStringLiteral("selector"), QStringLiteral("F%1").arg(i + 1));
                entry.insert(QStringLiteral("atom_count"), static_cast<int>(fragments[i].size()));
                QVector<int> indices;
                for (int index : fragments[i])
                    indices.append(index);
                bool truncated = false;
                entry.insert(QStringLiteral("indices"), toJsonArray(indices, 100, &truncated));
                if (truncated)
                    entry.insert(QStringLiteral("indices_truncated"), true);
                // Complete however long the fragment is, and short: the index list
                // is capped for context, which would otherwise leave a big fragment
                // impossible to name exactly. Zero-based, like every index here.
                entry.insert(QStringLiteral("index_range"), compactRange(indices));
                array.append(entry);
            }

            QJsonObject data;
            data.insert(QStringLiteral("fragments"), array);
            data.insert(QStringLiteral("count"), static_cast<int>(fragments.size()));
            return ToolResult::success(data);
        };
        add(spec);
    }

    // --- measure ------------------------------------------------------------
    {
        ToolSpec spec = base(QStringLiteral("measure"), QStringLiteral("analysis"),
            QStringLiteral("Measure an internal coordinate on the current frame: a distance "
                           "between 2 atoms, an angle over 3, a dihedral over 4, or the radius of "
                           "gyration over a set (all atoms when none are given)."),
            ToolEffect::Read, R"JSON({
              "type": "object",
              "properties": {
                "kind":  { "type": "string", "enum": ["distance", "angle", "dihedral", "gyration_radius"] },
                "atoms": { "type": "array",  "description": "0-based atom indices" }
              },
              "required": ["kind"]
            })JSON");
        spec.handler = [viewer](const QJsonObject& args) {
            const QVector<MoleculeViewer::Atom> atoms = viewer->getCurrentFrameAtoms();
            if (atoms.isEmpty())
                return noStructure();

            const QString kind = args.value(QStringLiteral("kind")).toString();
            QVector<int> indices;
            for (const QJsonValue& value : args.value(QStringLiteral("atoms")).toArray()) {
                if (!value.isDouble())
                    return ToolResult::failure(QStringLiteral("\"atoms\" must hold numbers"));
                const int index = value.toInt();
                if (index < 0 || index >= atoms.size()) {
                    return ToolResult::failure(QStringLiteral("atom index %1 is outside 0..%2")
                                                   .arg(index).arg(atoms.size() - 1));
                }
                indices.append(index);
            }

            const auto need = [&indices, &kind](int n) {
                return indices.size() == n
                    ? QString()
                    : QStringLiteral("\"%1\" needs exactly %2 atom indices, got %3")
                          .arg(kind).arg(n).arg(indices.size());
            };
            const auto pos = [&atoms, &indices](int i) { return atoms.at(indices.at(i)).position; };

            double value = 0.0;
            QString unit;
            if (kind == QLatin1String("distance")) {
                const QString err = need(2);
                if (!err.isEmpty()) return ToolResult::failure(err);
                value = measure::distance(pos(0), pos(1));
                unit = QStringLiteral("A");
            } else if (kind == QLatin1String("angle")) {
                const QString err = need(3);
                if (!err.isEmpty()) return ToolResult::failure(err);
                value = measure::angleDeg(pos(0), pos(1), pos(2));
                unit = QStringLiteral("deg");
            } else if (kind == QLatin1String("dihedral")) {
                const QString err = need(4);
                if (!err.isEmpty()) return ToolResult::failure(err);
                value = measure::dihedralDeg(pos(0), pos(1), pos(2), pos(3));
                unit = QStringLiteral("deg");
            } else {  // gyration_radius
                std::vector<QVector3D> positions;
                if (indices.isEmpty()) {
                    positions.reserve(atoms.size());
                    for (const MoleculeViewer::Atom& a : atoms)
                        positions.push_back(a.position);
                } else {
                    positions.reserve(indices.size());
                    for (int index : indices)
                        positions.push_back(atoms.at(index).position);
                }
                value = measure::gyrationRadius(positions);
                unit = QStringLiteral("A");
            }

            QJsonObject data;
            data.insert(QStringLiteral("kind"), kind);
            data.insert(QStringLiteral("value"), value);
            data.insert(QStringLiteral("unit"), unit);
            data.insert(QStringLiteral("atoms"), toJsonArray(indices, kMaxIndices, nullptr));
            return ToolResult::success(data,
                QStringLiteral("%1 = %2 %3").arg(kind).arg(value, 0, 'f', 4).arg(unit));
        };
        add(spec);
    }

    // --- get_frame_info -----------------------------------------------------
    {
        ToolSpec spec = base(QStringLiteral("get_frame_info"), QStringLiteral("structure"),
            QStringLiteral("Trajectory length and which frame is shown."),
            ToolEffect::Read, R"JSON({"type":"object"})JSON");
        spec.handler = [viewer](const QJsonObject&) {
            QJsonObject data;
            data.insert(QStringLiteral("frame_count"), viewer->getFrameCount());
            data.insert(QStringLiteral("current_frame"), viewer->getCurrentFrame());
            data.insert(QStringLiteral("is_trajectory"), viewer->getFrameCount() > 1);
            return ToolResult::success(data);
        };
        add(spec);
    }

    // --- get_camera ---------------------------------------------------------
    {
        ToolSpec spec = base(QStringLiteral("get_camera"), QStringLiteral("display"),
            QStringLiteral("Current camera: orientation, pan, distance and field of view."),
            ToolEffect::Read, R"JSON({"type":"object"})JSON");
        spec.handler = [viewer](const QJsonObject&) {
            const ViewPreset preset = viewer->currentViewPreset();
            QJsonObject rotation;
            rotation.insert(QStringLiteral("x"), preset.rootRotation.x());
            rotation.insert(QStringLiteral("y"), preset.rootRotation.y());
            rotation.insert(QStringLiteral("z"), preset.rootRotation.z());
            rotation.insert(QStringLiteral("scalar"), preset.rootRotation.scalar());
            QJsonObject pan;
            pan.insert(QStringLiteral("x"), preset.pan.x());
            pan.insert(QStringLiteral("y"), preset.pan.y());
            pan.insert(QStringLiteral("z"), preset.pan.z());

            QJsonObject data;
            data.insert(QStringLiteral("rotation"), rotation);
            data.insert(QStringLiteral("pan"), pan);
            data.insert(QStringLiteral("camera_distance"), preset.cameraDistance);
            data.insert(QStringLiteral("field_of_view"), preset.fieldOfView);
            data.insert(QStringLiteral("scene_extent"), preset.sceneExtent);
            return ToolResult::success(data);
        };
        add(spec);
    }

    // --- get_display --------------------------------------------------------
    {
        ToolSpec spec = base(QStringLiteral("get_display"), QStringLiteral("display"),
            QStringLiteral("How the structure is drawn: representation, colour scheme, atom scale, "
                           "bond thickness and the main effects."),
            ToolEffect::Read, R"JSON({"type":"object"})JSON");
        spec.handler = [viewer](const QJsonObject&) {
            static const QStringList kModes = {
                QStringLiteral("ball_and_stick"), QStringLiteral("wireframe"),
                QStringLiteral("space_filling"), QStringLiteral("sticks_only")
            };
            const int mode = static_cast<int>(viewer->getRenderingMode());
            QJsonObject data;
            data.insert(QStringLiteral("rendering_mode"),
                mode >= 0 && mode < kModes.size() ? kModes.at(mode) : QStringLiteral("unknown"));
            data.insert(QStringLiteral("rendering_mode_index"), mode);
            data.insert(QStringLiteral("colour_scheme_index"),
                static_cast<int>(viewer->getColorScheme()));
            data.insert(QStringLiteral("atom_scale"), viewer->getAtomScaleFactor());
            data.insert(QStringLiteral("bond_thickness"), viewer->getBondThickness());
            data.insert(QStringLiteral("atom_transparency"), viewer->getAtomTransparency());
            data.insert(QStringLiteral("fog"), viewer->getFogEnabled());
            data.insert(QStringLiteral("ssao"), viewer->getSSAOEnabled());
            data.insert(QStringLiteral("bloom"), viewer->getBloomEnabled());
            data.insert(QStringLiteral("hdr"), viewer->getHDREnabled());
            return ToolResult::success(data);
        };
        add(spec);
    }

    // --- list_workdir -------------------------------------------------------
    {
        const auto workingDirectory = context.workingDirectory;
        ToolSpec spec = base(QStringLiteral("list_workdir"), QStringLiteral("files"),
            QStringLiteral("Files in the current working directory, optionally filtered by a glob "
                           "such as \"*.xyz\"."),
            ToolEffect::Read, R"JSON({
              "type": "object",
              "properties": {
                "pattern": { "type": "string",  "description": "glob filter, e.g. *.xyz" },
                "limit":   { "type": "integer", "minimum": 1, "maximum": 200,
                             "description": "how many names at most (default 100)" }
              }
            })JSON");
        spec.handler = [workingDirectory](const QJsonObject& args) {
            const QString path = workingDirectory ? workingDirectory() : QString();
            if (path.isEmpty())
                return ToolResult::failure(QStringLiteral("no working directory is set"));
            QDir dir(path);
            if (!dir.exists())
                return ToolResult::failure(QStringLiteral("working directory %1 does not exist").arg(path));

            const QString pattern = args.value(QStringLiteral("pattern")).toString();
            const int limit = qBound(1, args.value(QStringLiteral("limit")).toInt(100), kMaxFiles);
            const QStringList filters = pattern.isEmpty() ? QStringList() : QStringList { pattern };
            const QStringList entries =
                dir.entryList(filters, QDir::Files | QDir::Readable, QDir::Name);

            QJsonArray array;
            for (int i = 0; i < qMin(entries.size(), limit); ++i)
                array.append(entries.at(i));

            QJsonObject data;
            data.insert(QStringLiteral("directory"), path);
            data.insert(QStringLiteral("files"), array);
            data.insert(QStringLiteral("returned"), array.size());
            data.insert(QStringLiteral("total"), entries.size());
            ToolResult result = ToolResult::success(data);
            result.truncated = entries.size() > array.size();
            return result;
        };
        add(spec);
    }

    // --- get_distance_matrix ------------------------------------------------
    {
        ToolSpec spec = base(QStringLiteral("get_distance_matrix"), QStringLiteral("analysis"),
            QStringLiteral("Lower-triangle distance matrix over a chosen set of atoms, in Angstrom. "
                           "The set has to be given -- a full matrix over a whole structure is "
                           "hundreds of thousands of numbers. Use get_contacts to find close pairs "
                           "without naming them first."),
            ToolEffect::Read, R"JSON({
              "type": "object",
              "properties": {
                "expression": { "type": "string",  "description": "ONE-based selection grammar: F1 or 1:20 (see select_atoms)" },
                "atoms":      { "type": "array",   "description": "explicit 0-based atom indices" },
                "max_atoms":  { "type": "integer", "minimum": 2, "maximum": 60,
                                "description": "refuse larger sets (default 40)" }
              }
            })JSON");
        spec.handler = [viewer](const QJsonObject& args) {
            const QVector<MoleculeViewer::Atom> atoms = viewer->getCurrentFrameAtoms();
            if (atoms.isEmpty())
                return noStructure();

            QVector<int> set;
            QString error;
            if (!resolveAtomSet(atoms, args.value(QStringLiteral("expression")).toString(),
                    args.value(QStringLiteral("atoms")).toArray(), set, error)) {
                return ToolResult::failure(error);
            }
            if (set.isEmpty())
                return ToolResult::failure(QStringLiteral("name an atom set: \"expression\" or \"atoms\""));

            const int maxAtoms = qBound(2, args.value(QStringLiteral("max_atoms")).toInt(40), 60);
            if (set.size() > maxAtoms) {
                return ToolResult::failure(
                    QStringLiteral("%1 atoms would be %2 pairs; raise max_atoms (up to 60) or "
                                   "narrow the selection")
                        .arg(set.size()).arg(set.size() * (set.size() - 1) / 2));
            }

            QJsonArray labels;
            for (int index : set)
                labels.append(QStringLiteral("%1 %2").arg(index).arg(atoms.at(index).element));

            QJsonArray matrix;
            for (int i = 1; i < set.size(); ++i) {
                QJsonArray row;
                for (int j = 0; j < i; ++j) {
                    row.append(measure::distance(atoms.at(set.at(i)).position,
                        atoms.at(set.at(j)).position));
                }
                matrix.append(row);
            }

            QJsonObject data;
            data.insert(QStringLiteral("labels"), labels);
            data.insert(QStringLiteral("matrix"), matrix);
            data.insert(QStringLiteral("unit"), QStringLiteral("A"));
            data.insert(QStringLiteral("layout"),
                QStringLiteral("lower triangle; matrix[i-1][j] is the distance between labels[i] and labels[j], j < i"));
            return ToolResult::success(data);
        };
        add(spec);
    }

    // --- get_contacts -------------------------------------------------------
    {
        ToolSpec spec = base(QStringLiteral("get_contacts"), QStringLiteral("analysis"),
            QStringLiteral("Atom pairs closer than a cutoff, nearest first. Give two selections to "
                           "get only the pairs BETWEEN them -- that is how you tell whether a guest "
                           "sits in a receptor's cavity and what touches what. Take the selectors "
                           "from get_fragments rather than writing them by hand; the grammar is "
                           "one-based while the reported indices are zero-based."),
            ToolEffect::Read, R"JSON({
              "type": "object",
              "properties": {
                "cutoff":         { "type": "number",  "minimum": 0.5, "maximum": 20.0,
                                    "description": "maximum distance in Angstrom (default 4.0)" },
                "selection_a":    { "type": "string",  "description": "ONE-based selection grammar (e.g. F1); with selection_b, only cross pairs" },
                "selection_b":    { "type": "string",  "description": "the other side of the cross pairs (e.g. F2)" },
                "exclude_bonded": { "type": "boolean", "description": "skip directly bonded pairs (default true)" },
                "limit":          { "type": "integer", "minimum": 1, "maximum": 300,
                                    "description": "how many pairs at most (default 50)" }
              }
            })JSON");
        spec.handler = [viewer](const QJsonObject& args) {
            const QVector<MoleculeViewer::Atom> atoms = viewer->getCurrentFrameAtoms();
            if (atoms.isEmpty())
                return noStructure();

            const double cutoff = args.value(QStringLiteral("cutoff")).toDouble(4.0);
            const int limit = qBound(1, args.value(QStringLiteral("limit")).toInt(50), 300);
            const bool excludeBonded = args.value(QStringLiteral("exclude_bonded")).toBool(true);
            const QString selA = args.value(QStringLiteral("selection_a")).toString();
            const QString selB = args.value(QStringLiteral("selection_b")).toString();

            if (selA.isEmpty() != selB.isEmpty()) {
                return ToolResult::failure(
                    QStringLiteral("give both selection_a and selection_b, or neither"));
            }

            QVector<int> setA;
            QVector<int> setB;
            QString error;
            const bool cross = !selA.isEmpty();
            if (cross) {
                if (!resolveAtomSet(atoms, selA, {}, setA, error)
                    || !resolveAtomSet(atoms, selB, {}, setB, error)) {
                    return ToolResult::failure(error);
                }
            } else {
                for (int i = 0; i < atoms.size(); ++i)
                    setA.append(i);
                setB = setA;
            }

            // Directly bonded pairs are not contacts; without this the nearest
            // neighbours of every atom drown out what one actually wants to see.
            QSet<QPair<int, int>> bonded;
            if (excludeBonded) {
                for (const MoleculeViewer::Bond& b : viewer->getCurrentFrameBonds())
                    bonded.insert(qMakePair(qMin(b.atom1, b.atom2), qMax(b.atom1, b.atom2)));
            }

            struct Contact {
                int a;
                int b;
                double d;
            };
            std::vector<Contact> contacts;
            for (int ia : setA) {
                for (int ib : setB) {
                    if (!cross && ib <= ia)
                        continue;   // each unordered pair once
                    if (cross && ia == ib)
                        continue;
                    const double d = measure::distance(atoms.at(ia).position, atoms.at(ib).position);
                    if (d > cutoff)
                        continue;
                    if (excludeBonded
                        && bonded.contains(qMakePair(qMin(ia, ib), qMax(ia, ib)))) {
                        continue;
                    }
                    contacts.push_back({ ia, ib, d });
                }
            }
            std::sort(contacts.begin(), contacts.end(),
                [](const Contact& l, const Contact& r) { return l.d < r.d; });

            QJsonArray array;
            const int shown = qMin(static_cast<int>(contacts.size()), limit);
            for (int i = 0; i < shown; ++i) {
                const Contact& c = contacts[i];
                QJsonObject entry;
                entry.insert(QStringLiteral("a"), c.a);
                entry.insert(QStringLiteral("a_element"), atoms.at(c.a).element);
                entry.insert(QStringLiteral("b"), c.b);
                entry.insert(QStringLiteral("b_element"), atoms.at(c.b).element);
                entry.insert(QStringLiteral("distance"), c.d);
                array.append(entry);
            }

            QJsonObject data;
            data.insert(QStringLiteral("contacts"), array);
            data.insert(QStringLiteral("returned"), shown);
            data.insert(QStringLiteral("total"), static_cast<int>(contacts.size()));
            data.insert(QStringLiteral("cutoff"), cutoff);
            data.insert(QStringLiteral("unit"), QStringLiteral("A"));
            data.insert(QStringLiteral("cross_selection"), cross);
            ToolResult result = ToolResult::success(data);
            result.truncated = shown < static_cast<int>(contacts.size());
            if (contacts.empty()) {
                result.text = QStringLiteral("No pair is closer than %1 A%2.")
                                  .arg(cutoff)
                                  .arg(cross ? QStringLiteral(" across the two selections") : QString());
            }
            return result;
        };
        add(spec);
    }

    // --- render_view --------------------------------------------------------
    {
        ToolSpec spec = base(QStringLiteral("render_view"), QStringLiteral("display"),
            QStringLiteral("Render what is on screen and return it as a PNG. Use this when the "
                           "question is about shape, arrangement or where something sits -- a "
                           "cavity is far easier to see than to infer from coordinates. Change "
                           "the camera or the representation first if another angle would help. "
                           "It works during a simulation too, so a run can be followed by eye as "
                           "well as by watch_simulation; the run is held for the render, so keep "
                           "the size modest there (it is capped at 800 px)."),
            ToolEffect::Display, R"JSON({
              "type": "object",
              "properties": {
                "width":  { "type": "integer", "minimum": 128, "maximum": 1600,
                            "description": "pixels (default 800)" },
                "height": { "type": "integer", "minimum": 128, "maximum": 1600,
                            "description": "pixels (default 600)" },
                "background": { "type": "string", "enum": ["scene", "white", "transparent"],
                                "description": "default white, which reads best in a chat" }
              }
            })JSON");
        spec.handler = [viewer](const QJsonObject& args) -> ToolResult {
            if (viewer->getCurrentFrameAtoms().isEmpty())
                return noStructure();

            // exportImage renders synchronously on the GUI thread, which is where the
            // MD timer also lives, so the run is held for the length of the render.
            // A running simulation is refused nothing here -- watching it is half the
            // point -- but the ceiling drops, because the stall grows with the
            // resolution and a stuttering trajectory is a worse answer than a smaller
            // picture. (SSAA is off in either case.)
            const bool running = viewer->simulationActive();
            const int ceiling = running ? 800 : 1600;
            const int width = qBound(128, args.value(QStringLiteral("width")).toInt(800), ceiling);
            const int height = qBound(128, args.value(QStringLiteral("height")).toInt(600), ceiling);
            const QString background = args.value(QStringLiteral("background"))
                                           .toString(QStringLiteral("white"));
            const int backgroundMode = background == QLatin1String("scene") ? 0
                : background == QLatin1String("transparent")                ? 2
                                                                            : 1;

            // exportImage writes a file; the bytes are what a model can look at, so
            // the file is a temporary and goes away again.
            QTemporaryFile file(QDir::tempPath() + QStringLiteral("/qurcuma-render-XXXXXX.png"));
            file.setAutoRemove(true);
            if (!file.open())
                return ToolResult::failure(QStringLiteral("could not create a temporary file"));
            const QString path = file.fileName();
            file.close();

            ImageMetadata metadata;
            metadata.embed = false;   // no provenance block in a throwaway view
            if (!viewer->exportImage(path, width, height, backgroundMode, /*ssaa=*/false, metadata))
                return ToolResult::failure(QStringLiteral("rendering failed"));

            QFile rendered(path);
            if (!rendered.open(QIODevice::ReadOnly))
                return ToolResult::failure(QStringLiteral("could not read the rendered image back"));
            const QByteArray bytes = rendered.readAll();
            rendered.close();

            QJsonObject data;
            data.insert(QStringLiteral("width"), width);
            data.insert(QStringLiteral("height"), height);
            data.insert(QStringLiteral("bytes"), bytes.size());
            ToolResult result = ToolResult::success(data, running
                    ? QStringLiteral("Rendered %1x%2 of the running simulation; it was held for "
                                     "the length of the render.").arg(width).arg(height)
                    : QStringLiteral("Rendered %1x%2.").arg(width).arg(height));
            result.image = bytes;
            result.imageMimeType = QStringLiteral("image/png");
            return result;
        };
        add(spec);
    }

    return added;
}
