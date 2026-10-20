// setupcheck.h - What a simulation protocol needs, and what this stack can do of it.
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - A published MD setup (system, force field, stages with
// ensemble, temperature, length ...) is written down as one JSON protocol. This
// file holds the one list of capabilities the protocol is checked against and the
// check itself. Qt Core only, no viewer: the tools in tools_protocol.cpp are thin
// wrappers, and the test runs without a GUI.
//
// The list is the single source of truth for "can we do X". Every entry names
// where the thing lives (`qurcuma:` or `curcuma:` plus a path), and test_setupcheck
// fails when such a path does not exist, so the list cannot point at code that was
// moved away. Whether a capability WORKS is not tested here; "available" means a
// code path and a tool exist, and the note says what was and was not tried.
//
// Protocol shape (all keys optional unless stated; see docs/WP-min2015-reproduction.md):
//   source, method,
//   system   { solute_file, charge, solvent, solvent_count, density_g_cm3, periodic,
//              electrostatics, ions { anion, cation } },
//   stages[] { name (required), type "minimize"|"md" (required), ensemble "nvt"|"nve"|"npt",
//              temperature_K (number or list), series "independent"|"chained"|"ramp",
//              pressure_bar, length_ps, timestep_fs, thermostat, coupling_fs,
//              barostat_coupling_fs },
//   execution { parallel }   -- a resource choice, never part of the science,
//   analysis[] { quantity, last_ns }
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace setupcheck {

enum class State { Available, Partial, Missing };

struct Capability {
    QString id;       ///< "md.npt"
    State state;
    QString note;     ///< what exists, what does not
    QString where;    ///< "qurcuma:path" or "curcuma:path"
    QString package;  ///< item of docs/WP-min2015-reproduction.md that closes it, empty if none
};

/// The capability list, in a stable order.
const QVector<Capability>& capabilities();

/// The entry with this id, or nullptr.
const Capability* findCapability(const QString& id);

QString stateName(State state);

/// Check @p protocol against the capability list. Never touches the disk. The result:
///   ok          false when the protocol itself is malformed (a blocked stage is not malformed)
///   errors      what is malformed
///   prepare     tool calls that build the system, with the arguments the tools take
///   stages      per stage: name, state ready|blocked|invalid, blockers[], notes[], runs[]
///   analysis    per quantity: state and blockers
///   summary     counts
QJsonObject checkProtocol(const QJsonObject& protocol);

// --- stage progress ------------------------------------------------------------
// status.json in the project folder: { "stages": { "<name>": { "state", "updated", "note" } } }.
// States: pending, running, done, failed.

QString statusPath(const QString& directory);
QJsonObject readStatus(const QString& directory);
/// Create the file with every stage pending; stages already in it keep their state.
bool initStatus(const QString& directory, const QStringList& stages, QString* error);
/// Set one stage's state. The stage must exist in the file.
bool setStageState(const QString& directory, const QString& stage, const QString& state,
    const QString& note, QString* error);

} // namespace setupcheck
