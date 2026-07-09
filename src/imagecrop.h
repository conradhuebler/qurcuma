// imagecrop.h - Batch border-trim analysis for exported molecular images.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - FlipBooQ-style uniform whitespace removal that
// PRESERVES the PNG text-chunk metadata qurcuma embeds (see imagemetadata.h).
//
// Goal (think flip book / Daumenkino): a batch of exported frames must all end up
// the SAME X×Y and stay spatially registered, so flipping through them shows the
// molecule move smoothly rather than jump around. The steps:
//   1. Put every frame on a common canvas = (max source width) × (max source
//      height), each centred and padded with its own corner background. Equal-size
//      inputs (the normal case) are unchanged by this step.
//   2. The common crop rectangle is the UNION of every frame's content box on that
//      canvas — the smallest rectangle that still contains all content of every
//      frame. Cropping all frames to this one rectangle removes only the border
//      that is empty in EVERY frame, keeps motion, and yields identical output
//      dimensions.
// "Background" of a frame is read from its top-left corner pixel (robust for
// qurcuma's transparent / white / scene backgrounds).

#pragma once

#include <QList>
#include <QMap>
#include <QRect>
#include <QSize>
#include <QString>
#include <QtGui/qrgb.h>  // QRgb (typedef — cannot be forward-declared)

class QImage;

namespace imagecrop {

/** @brief Bounding box of the non-background content of @p img.
 *
 * Scans rows from the top and bottom and columns from the left and right,
 * stopping as soon as a line contains a pixel whose per-channel difference
 * (R,G,B,A) from @p background exceeds @p tolerance. A fully-uniform image
 * (nothing but background) yields an empty QRect. Claude Generated 2026.
 */
QRect contentRect(const QImage& img, QRgb background, int tolerance);

/** @brief Common canvas size for a batch: the maximum width and maximum height
 *  across @p imgs. Every frame is centred on a canvas of this size before the
 *  crop is computed, so differently-sized exports become comparable. */
QSize canvasSize(const QList<QImage>& imgs);

/** @brief Copy @p src centred onto a @p canvas-sized image, padding the margin
 *  with @p src's corner background colour. @p canvas must be at least as large as
 *  @p src in both dimensions (use canvasSize()). Returns an ARGB32 image. */
QImage centerOnCanvas(const QImage& src, const QSize& canvas);

/** @brief The common crop rectangle in @p canvas coordinates: the union of each
 *  frame's content box after centring on the canvas. Clamped to the canvas; falls
 *  back to the whole canvas if no frame has content. Claude Generated 2026. */
QRect commonContentRect(const QList<QImage>& imgs, const QSize& canvas, int tolerance);

/** @brief Save @p out as PNG to @p outPath, carrying over every text chunk of
 *  @p src (QImage::load reads tEXt/zTXt into the image), rewriting
 *  ImageWidth/ImageHeight to @p out's size and adding the @p extra provenance
 *  keys. Output is always PNG (JPEG cannot store text chunks). Claude Generated 2026. */
bool savePngWithMetadata(const QImage& out, const QImage& src, const QString& outPath,
    const QMap<QString, QString>& extra);

} // namespace imagecrop
