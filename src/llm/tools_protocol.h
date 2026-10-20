// tools_protocol.h - Tools around a simulation protocol: capability list, check, save, progress.
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Thin wrappers over setupcheck.h. None of them runs a
// simulation: check_protocol and list_capabilities only read, save_protocol and
// protocol_stage write protocol.json and status.json into the project folder.
#pragma once

#include <QString>

#include <functional>

class ToolRegistry;

struct ProtocolToolContext {
    /// Project folder; protocol.json and status.json live here. Supplied by MainWindow.
    std::function<QString()> workingDirectory;
};

/// Register list_capabilities, check_protocol, save_protocol and protocol_stage. Returns how many.
int registerProtocolTools(ToolRegistry& registry, const ProtocolToolContext& context);
