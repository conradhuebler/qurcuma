// tools_view.cpp - Read-only tools over the loaded structure and the viewer.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "tools_view.h"

#include "core/toolregistry.h"
#include "measurements.h"
#include "moleculebridge.h"
#include "view.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>

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
                           "grammar: \"1:10,15\" is a range plus a single atom, \"F2\" is fragment 2, "
                           "\"-1\" is everything. Pass neither to clear the selection."),
            ToolEffect::Display, R"JSON({
              "type": "object",
              "properties": {
                "expression": { "type": "string",  "description": "selection grammar, e.g. 1:10,15 or F2" },
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
            if (!expression.isEmpty()) {
                // curcuma owns this grammar; re-implementing it here would be a
                // second parser to keep in step with the engine.
                const curcuma::Molecule molecule = atomsToMolecule(atoms);
                const std::vector<int> resolved =
                    molecule.FragString2Indicies(expression.toStdString());
                if (resolved.empty()) {
                    return ToolResult::failure(
                        QStringLiteral("selection \"%1\" matched no atoms").arg(expression));
                }
                for (int index : resolved)
                    wanted.append(index);
            } else {
                for (const QJsonValue& value : explicitIndices) {
                    if (!value.isDouble())
                        return ToolResult::failure(QStringLiteral("\"indices\" must hold numbers"));
                    wanted.append(value.toInt());
                }
            }

            for (int index : wanted) {
                if (index < 0 || index >= atoms.size()) {
                    return ToolResult::failure(QStringLiteral("atom index %1 is outside 0..%2")
                                                   .arg(index).arg(atoms.size() - 1));
                }
            }

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
                           "Fragment n can then be selected with the expression \"Fn\"."),
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
                entry.insert(QStringLiteral("atom_count"), static_cast<int>(fragments[i].size()));
                QVector<int> indices;
                for (int index : fragments[i])
                    indices.append(index);
                bool truncated = false;
                entry.insert(QStringLiteral("indices"), toJsonArray(indices, 100, &truncated));
                if (truncated)
                    entry.insert(QStringLiteral("indices_truncated"), true);
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

    return added;
}
