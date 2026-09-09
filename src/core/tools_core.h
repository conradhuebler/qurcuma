// tools_core.h - Tools that need nothing but the core.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Registration is by whoever owns the capability. These two
// own nothing but the log and the catalogue itself, so they live in qurcuma_core
// and are the only tools a headless consumer gets for free.
#pragma once

class LogHub;
class ToolRegistry;

/// Register `read_log` and `describe_tools` into @p registry.
///
/// @p hub is borrowed and must outlive the registry. Returns the number of tools
/// registered, so a caller can assert it got what it expected.
int registerCoreTools(ToolRegistry& registry, LogHub& hub);
