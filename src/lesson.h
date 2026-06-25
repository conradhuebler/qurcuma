// lesson.h - Teaching scenario ("Lesson") data model + JSON serialization
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - OER teaching scenarios for Qurcuma.
//
// A *Lesson* bundles several molecular structures, each carrying its own
// simulation conditions (a full SimulationConfig) plus pedagogical metadata
// (title, authors with ORCID/affiliation, license ...). It is distributed as a
// single self-contained "*.qlesson.json" file with every structure embedded as
// inline XYZ text. On load the structures are unpacked into a working directory
// as plain ".xyz" files (so they appear in the existing file browser) next to a
// sidecar "lesson.json" that keeps the per-structure conditions.
//
// One schema, two storage variants: a structure carries EITHER an inline `xyz`
// string (distributable file) OR a `file` reference (unpacked sidecar).

#pragma once

#include "simulationworker.h"   // SimulationConfig
#include "view.h"               // MoleculeViewer::Atom

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

/** @brief One author/contributor of a lesson. Claude Generated 2026. */
struct LessonAuthor {
    QString name;
    QString orcid;        // e.g. "0000-0002-1825-0097"
    QString institution;
    QString email;
};

/** @brief Lesson-level metadata (the pedagogical / provenance block). */
struct LessonMetadata {
    QString title;
    QString description;
    QVector<LessonAuthor> authors;
    QString license = QStringLiteral("CC-BY-4.0");
    QString language = QStringLiteral("de");
    QStringList keywords;
    QString created;          // ISO-8601; set on first save
    QString modified;         // ISO-8601; refreshed on every save
    QString qurcumaVersion;   // provenance: writing app version
};

/**
 * @brief One structure inside a lesson: geometry + its simulation conditions.
 *
 * `xyz` (inline) and `file` (reference) are mutually exclusive storage variants
 * of the same geometry. `role` is a reserved pedagogical label
 * (start|intermediate|target); in v1 it is only carried through, not acted on.
 */
struct LessonStructure {
    QString name;
    QString description;
    QString role;             // reserved: "start" | "intermediate" | "target" | ""
    QString xyz;              // inline XYZ text (self-contained file) ...
    QString file;             // ... OR referenced ".xyz" filename (unpacked sidecar)
    SimulationConfig sim;     // full per-structure simulation conditions
};

/** @brief A complete teaching scenario. */
struct Lesson {
    LessonMetadata meta;
    QVector<LessonStructure> structures;
};

// --- Serialization (free functions) -----------------------------------------

/** @brief Lossless round-trip of a SimulationConfig to/from JSON. The only such
 *  pair; uses the struct's own field names (decoupled from curcuma's param
 *  schema in simulationworker.cpp). Claude Generated 2026. */
QJsonObject simConfigToJson(const SimulationConfig& cfg);
SimulationConfig simConfigFromJson(const QJsonObject& obj);

/** @brief Serialize a lesson. @p inlineXyz=true embeds each structure's geometry
 *  as inline XYZ (self-contained ".qlesson.json"); false writes `file`
 *  references (unpacked sidecar "lesson.json"). */
QJsonObject lessonToJson(const Lesson& lesson, bool inlineXyz);

/** @brief Parse a lesson. On failure returns an empty lesson and (if non-null)
 *  fills @p error. Reads both `xyz` and `file` structure variants. */
Lesson lessonFromJson(const QJsonObject& obj, QString* error = nullptr);

/** @brief Unpack a lesson into @p targetDir: writes each structure's inline XYZ
 *  verbatim as "<slug>.xyz" and a sidecar "lesson.json" (with `file`
 *  references). Populates each structure's `file` field. Returns false + @p error
 *  on I/O failure. Claude Generated 2026. */
bool extractLesson(Lesson& lesson, const QString& targetDir, QString* error = nullptr);

/** @brief Build XYZ text from viewer atoms (count / comment / "El x y z" lines). */
QString atomsToXyz(const QVector<MoleculeViewer::Atom>& atoms, const QString& comment);

/** @brief Parse an inline XYZ string (as produced by atomsToXyz / embedded in a
 *  lesson) into viewer atoms. Returns false if the text is not valid XYZ.
 *  Bonds are left to the viewer's auto-detection. Claude Generated 2026. */
bool xyzToAtoms(const QString& xyz, QVector<MoleculeViewer::Atom>& atoms);
