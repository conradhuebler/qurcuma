// scriptbuiltins.h - What a script has beyond plain JavaScript.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Everything in here is what JavaScript does not already
// bring. Arithmetic, arrays, objects, loops and Math are the engine's own business;
// this file adds the chemistry: a mean and a spread over measured values, a slope,
// unit conversions with their sources, and print().
#pragma once

#include "scriptinterpreter.h"

#include <QString>
#include <QStringList>

class QJSEngine;

namespace script {

/// Numbers as a tool result and a log line want them: ten significant digits, no
/// trailing zeros, so 0.1 + 0.2 reads as 0.3 while a real difference stays visible.
/// The script side formats with the same rule (__num in builtinsSource) for the
/// ordinary range; the exponent forms are each language's own, and a test pins the
/// part that has to agree.
QString formatNumber(double value);

/// The JavaScript that defines the builtin functions and constants, as one program so
/// the whole surface can be read in one place. Evaluated in a fresh engine before
/// every script; an error in it is an error of this program, not of the script.
const char* builtinsSource();

/// What print() collected, in order, cut at @p maxPrints.
///
/// Claude Generated 2026 - C++ cannot hand a JS function to the engine (no
/// newFunction in Qt 6.11), so print pushes into an array inside the script's own
/// engine and this reads it back after the run.
QStringList readPrints(QJSEngine& engine, int maxPrints, bool* truncated);

/// A value out of the engine, as a QVariant a tool result can carry: lists and objects
/// cut at @p limits.maxListValues, long strings shortened, nesting bounded. Sets
/// @p truncated when something was left out.
QVariant cappedValue(const QVariant& value, const ScriptLimits& limits, bool* truncated);

}  // namespace script
