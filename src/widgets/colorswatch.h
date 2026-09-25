// colorswatch.h - Claude Generated 2026
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Colour-selector helpers shared by the Display panel (fragments, bead types) and the
// NCI options (interaction classes): the button itself shows the colour, combo
// entries carry a small square.
#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QPushButton>
#include <QString>

namespace swatch {

/// Paint a colour button so the button itself is the swatch (label readable on
/// light and dark colours). An invalid colour clears it.
inline void apply(QPushButton* button, const QColor& color)
{
    if (!button)
        return;
    button->setText(color.isValid() ? color.name(QColor::HexRgb) : QString());
    if (!color.isValid()) {
        button->setStyleSheet(QString());
        return;
    }
    const bool dark = color.lightness() < 128;
    button->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                              .arg(color.name(QColor::HexRgb), dark ? "#ffffff" : "#000000"));
}

/// Small colour square for a combo-box entry.
inline QIcon icon(const QColor& color)
{
    QPixmap pm(14, 14);
    pm.fill(color.isValid() ? color : QColor(Qt::transparent));
    return QIcon(pm);
}

} // namespace swatch
