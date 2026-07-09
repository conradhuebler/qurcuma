// imagecrop.cpp - Batch border-trim analysis implementation.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - see imagecrop.h for the algorithm rationale.

#include "imagecrop.h"

#include <QColor>
#include <QImage>
#include <QPainter>

#include <cstdlib>

namespace imagecrop {

namespace {

/// True if @p px differs from @p bg by more than @p tolerance in ANY channel
/// (R, G, B or A) — i.e. the pixel counts as content, not removable border.
inline bool isContent(QRgb px, QRgb bg, int tolerance)
{
    return std::abs(qRed(px) - qRed(bg)) > tolerance
        || std::abs(qGreen(px) - qGreen(bg)) > tolerance
        || std::abs(qBlue(px) - qBlue(bg)) > tolerance
        || std::abs(qAlpha(px) - qAlpha(bg)) > tolerance;
}

} // namespace

QRect contentRect(const QImage& img, QRgb background, int tolerance)
{
    if (img.isNull())
        return QRect();

    // Work on a known format so scanLine() gives us packed QRgb rows.
    const QImage argb = img.format() == QImage::Format_ARGB32
        ? img
        : img.convertToFormat(QImage::Format_ARGB32);

    const int w = argb.width();
    const int h = argb.height();
    if (w < 1 || h < 1)
        return QRect();

    auto rowHasContent = [&](int y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(argb.constScanLine(y));
        for (int x = 0; x < w; ++x)
            if (isContent(line[x], background, tolerance))
                return true;
        return false;
    };

    int top = 0;
    while (top < h && !rowHasContent(top))
        ++top;
    if (top == h)
        return QRect();  // fully uniform → nothing to keep

    int bottom = h - 1;
    while (bottom > top && !rowHasContent(bottom))
        --bottom;

    // Columns: only need to scan the [top, bottom] band we already found.
    auto colHasContent = [&](int x) {
        for (int y = top; y <= bottom; ++y) {
            const QRgb* line = reinterpret_cast<const QRgb*>(argb.constScanLine(y));
            if (isContent(line[x], background, tolerance))
                return true;
        }
        return false;
    };

    int left = 0;
    while (left < w && !colHasContent(left))
        ++left;
    int right = w - 1;
    while (right > left && !colHasContent(right))
        --right;

    return QRect(left, top, right - left + 1, bottom - top + 1);
}

QSize canvasSize(const QList<QImage>& imgs)
{
    int w = 0;
    int h = 0;
    for (const QImage& img : imgs) {
        if (img.isNull())
            continue;
        w = qMax(w, img.width());
        h = qMax(h, img.height());
    }
    return QSize(w, h);
}

QImage centerOnCanvas(const QImage& src, const QSize& canvas)
{
    if (src.isNull() || canvas.isEmpty())
        return QImage();

    const QRgb bg = src.pixel(0, 0);
    QImage out(canvas, QImage::Format_ARGB32);
    out.fill(QColor::fromRgba(bg));

    const int offx = (canvas.width() - src.width()) / 2;
    const int offy = (canvas.height() - src.height()) / 2;

    const QImage s = src.convertToFormat(QImage::Format_ARGB32);
    QPainter p(&out);
    p.setCompositionMode(QPainter::CompositionMode_Source);  // copy pixels verbatim
    p.drawImage(QPoint(offx, offy), s);
    p.end();
    return out;
}

QRect commonContentRect(const QList<QImage>& imgs, const QSize& canvas, int tolerance)
{
    const QRect canvasRect(QPoint(0, 0), canvas);
    QRect uni;
    for (const QImage& img : imgs) {
        if (img.isNull())
            continue;
        const QRgb bg = img.pixel(0, 0);
        QRect c = contentRect(img, bg, tolerance);
        if (c.isEmpty())
            continue;  // skip blank frames so they don't collapse the union
        // Translate into canvas coordinates (same centring as centerOnCanvas).
        const int offx = (canvas.width() - img.width()) / 2;
        const int offy = (canvas.height() - img.height()) / 2;
        c.translate(offx, offy);
        uni = uni.isNull() ? c : uni.united(c);
    }
    if (uni.isNull())
        return canvasRect;  // nothing has content → keep the whole canvas
    return uni.intersected(canvasRect);
}

bool savePngWithMetadata(const QImage& out, const QImage& src, const QString& outPath,
    const QMap<QString, QString>& extra)
{
    if (out.isNull())
        return false;

    QImage image = out;  // shallow copy (COW); setText detaches as needed

    // Carry over every text chunk from the source (QImage::load reads tEXt/zTXt
    // into the image's text table), then override the size and add provenance.
    const QStringList keys = src.textKeys();
    for (const QString& key : keys) {
        if (key.isEmpty())
            continue;
        image.setText(key, src.text(key));
    }
    image.setText(QStringLiteral("ImageWidth"), QString::number(image.width()));
    image.setText(QStringLiteral("ImageHeight"), QString::number(image.height()));
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it)
        image.setText(it.key(), it.value());

    return image.save(outPath, "PNG");
}

} // namespace imagecrop
