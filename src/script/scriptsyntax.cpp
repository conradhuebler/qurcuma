// scriptsyntax.cpp - Reading a script's tool names without running it.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026
#include "scriptsyntax.h"

#include <QRegularExpression>

namespace script {

QStringList namedToolsIn(const QString& source)
{
    // tool("name")  |  tool('name', args)  |  tool( "name" , args )
    //
    // The name has to be the whole first argument, so the quote is followed by a comma
    // or the closing parenthesis. Without that, `tool("mea" + "sure", {})` would be
    // previewed as a tool called "mea", and a preview that names the wrong thing is
    // worse than one that admits it cannot tell.
    static const QRegularExpression call(
        QStringLiteral("\\btool\\s*\\(\\s*[\"']([A-Za-z_][A-Za-z0-9_]*)[\"']\\s*[,)]"));

    QStringList names;
    auto it = call.globalMatch(source);
    while (it.hasNext()) {
        const QString name = it.next().captured(1);
        if (!names.contains(name))
            names.append(name);
    }
    return names;
}

}  // namespace script
