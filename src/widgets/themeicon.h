// themeicon.h - Icon of the desktop theme with the style's standard icon as fallback
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026: one helper for the dialogs (settings, add computer) and the dock buttons.

#pragma once

#include <QApplication>
#include <QIcon>
#include <QString>
#include <QStyle>

/// The theme icon @p themeName, or the style's @p fallback icon when the theme has none.
/// Claude Generated 2026
inline QIcon themeIcon(const QString& themeName, QStyle::StandardPixmap fallback)
{
    const QIcon themed = QIcon::fromTheme(themeName);
    return themed.isNull() ? QApplication::style()->standardIcon(fallback) : themed;
}
