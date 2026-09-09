// curcuma_schemas.h - Tool schemas derived from curcuma's parameter registry.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - The schemas are read out of curcuma rather than written
// here: the command-to-module mapping, the exposure tier, the permitted values,
// the units and the bounds all live in the PARAM annotations now, so a schema that
// drifts from the engine would have to be made to drift on purpose.
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

/// Commands qurcuma can run in process (the subset CurcumaJob implements).
QStringList curcumaJobCommands();

/// Parameter schema for @p command, built from the tier=primary parameters of the
/// modules that command configures. Empty when the command is unknown.
QJsonObject curcumaJobSchema(const QString& command);

/// Everything curcuma knows about @p command: its modules, their descriptions and
/// every parameter with type, default, unit, permitted values and relevance --
/// the long tail the compact schema deliberately leaves out.
QJsonObject curcumaJobDetails(const QString& command);
