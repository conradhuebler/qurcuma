// tools_palette.h - Registry tools as command-palette entries.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - The palette gets the tools ADDED to its menu entries,
// not instead of them: collectMenuCommands() hands it ~120 actions together with
// their enabled state, and replacing that would mean rebuilding all of it by hand
// while shrinking to the argument-free tools.
//
// The point of the pairing is that a human then exercises the registry every day.
// A tool that stops working shows up at Ctrl+K rather than the first time a model
// trips over it.
#pragma once

#include "core/tool.h"
#include "widgets/commandpalette.h"

#include <QString>
#include <QVector>

#include <functional>

class ToolDispatcher;
class ToolRegistry;

/// Palette entries for every registered tool that can run with no arguments --
/// which includes tools whose parameters are all optional, since those have
/// sensible defaults.
///
/// @p dispatcher, not the registry, runs them: that is what applies the GUI-thread
/// marshalling and writes the audit record, so the palette path behaves exactly
/// like the one a model will take.
/// @p onResult is called on the GUI thread with the tool's name and its result.
QVector<CommandPalette::Command> paletteCommandsForTools(
    const ToolRegistry& registry,
    ToolDispatcher* dispatcher,
    std::function<void(const QString&, const ToolResult&)> onResult);
