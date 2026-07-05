// imagemetadata.h - Metadata embedded into exported molecular images.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Reproducible figures with embedded provenance.

#pragma once

#include <QColor>
#include <QQuaternion>
#include <QString>
#include <QStringList>
#include <QVector3D>

#include "viewpreset.h"  // ZoomMode

/** @brief Metadata written as PNG text chunks into exported images so the
 *  figure stays reproducible: source files, camera, display settings, and
 *  authorship (license/ORCID). Claude Generated 2026.
 */
struct ImageMetadata {
    bool embed = true;

    // --- authorship (from Settings) ---
    QString authorName;
    QString authorOrcid;
    QString authorInstitution;
    QString license;          // e.g. "CC-BY-4.0"

    // --- source ---
    QStringList sourceFiles;  // primary file + optional overlay files

    // --- camera (reproduction) ---
    QQuaternion cameraRotation;
    QVector3D cameraPan;
    float cameraDistance = 0.0f;
    ZoomMode zoomMode = ZoomMode::Absolute;
    float zoomFactor = 3.0f;

    // --- display ---
    int renderingMode = 0;
    int colorScheme = 0;
    float atomScaleFactor = 1.0f;
    float bondThickness = 0.15f;
    float atomTransparency = 1.0f;
    QColor backgroundColor;
    QString effects;          // "SSAO=1,Bloom=1,HDR=1,Fog=0"

    // --- meta ---
    QString qurcumaVersion;
    QString exportTimestamp;  // ISO-8601
    int width = 0;
    int height = 0;
    QString viewPresetName;   // empty when no preset was applied
};