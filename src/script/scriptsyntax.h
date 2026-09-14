// scriptsyntax.h - What a script says before it runs.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - A script the operator starts may call tools, and the
// operator should know which ones before it does. Only a literal name can be shown
// ahead of time, so only a literal name counts here: a name built at run time cannot
// be previewed, and a preview that may be wrong is not a preview.
#pragma once

#include <QString>
#include <QStringList>

namespace script {

/// The tool names a script names as string literals, in order of first appearance.
///
/// This reads the source, it does not parse it: an occurrence inside a comment or a
/// string is counted too. That is deliberate for a preview (a name mentioned but not
/// called costs a line in a dialog, a name missed would be a surprise), and it is not
/// what enforces anything. A name assembled from pieces is left out, because naming its
/// first fragment would be a statement about the wrong tool.
QStringList namedToolsIn(const QString& source);

}  // namespace script
