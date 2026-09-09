// tools_palette.cpp - Registry tools as command-palette entries.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "tools_palette.h"

#include "core/tooldispatcher.h"
#include "core/toolregistry.h"

#include <QJsonObject>

QVector<CommandPalette::Command> paletteCommandsForTools(
    const ToolRegistry& registry,
    ToolDispatcher* dispatcher,
    std::function<void(const QString&, const ToolResult&)> onResult)
{
    QVector<CommandPalette::Command> commands;
    if (!dispatcher)
        return commands;

    for (const ToolSpec& spec : registry.all()) {
        // "Runs without arguments" is not "has no parameters": read_log,
        // get_contacts and list_workdir all take optional ones with defaults, and
        // those are exactly the ones worth having at Ctrl+K. Ask the validator
        // rather than guessing from the schema shape.
        if (!ToolRegistry::validateAgainst(spec.paramSchema, QJsonObject {}).ok)
            continue;

        CommandPalette::Command command;
        command.title = spec.name;
        command.context = spec.category.isEmpty()
            ? QStringLiteral("Tool")
            : QStringLiteral("Tool ▸ %1").arg(spec.category);
        command.enabled = true;

        const QString name = spec.name;
        command.run = [dispatcher, name, onResult]() {
            const ToolResult result = dispatcher->dispatch(name, QJsonObject {});
            if (onResult)
                onResult(name, result);
        };
        commands.append(command);
    }
    return commands;
}
