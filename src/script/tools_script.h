// tools_script.h - The script interpreter as a tool.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - One tool, `calculate`, registered from the script library
// so it is testable headless like the core tools. It is the assistant's way to have a
// number computed instead of working it out in prose.
#pragma once

class ToolDispatcher;
class ToolRegistry;

/// What the script tool needs from its surroundings. The interpreter itself needs
/// nothing; only the stop button does.
struct ScriptToolContext {
    /// Borrowed, may be null. Its interrupt flag ends a running script, the same way
    /// it ends a waiting job.
    ToolDispatcher* dispatcher = nullptr;
};

/// Register `calculate` into @p registry. Returns the number registered, so a caller
/// can assert it got what it expected.
int registerScriptTools(ToolRegistry& registry, const ScriptToolContext& context);
