// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Element colour/radius tables shared by the Qt Quick 3D viewer and overlays.
// Extracted so the renderer is independent of MoleculeViewer's private helpers.
// Claude Generated.
#pragma once

#include <QColor>
#include <QString>

namespace elem {

/// Standard CPK element colour (light grey fallback for unknown elements).
QColor cpkColor(const QString& element);

/// Van-der-Waals-style display radius in Angstrom (carbon-like fallback).
float vdwRadius(const QString& element);

/// Covalent radius in Angstrom for distance-based bond detection.
float covalentRadius(const QString& element);

} // namespace elem
