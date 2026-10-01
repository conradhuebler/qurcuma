// remotelog.h - Diagnostic log of the remote compute code
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute): every step of connecting (the ssh command lines,
// ssh's own messages, socket errors, protocol events) is appended to one file, and also
// handed to the GUI as text, so a failed connection can be read afterwards.

#pragma once

#include <QString>

namespace remote {

/// <AppData>/qurcuma/remote.log (rotated to remote.log.1 above 1 MB).
QString logFilePath();
/// Appends one timestamped line; returns the same line (without the timestamp) for display.
QString appendLog(const QString& source, const QString& message);

} // namespace remote
