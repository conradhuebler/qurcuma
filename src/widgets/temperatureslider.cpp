// temperatureslider.cpp - Vertical, temperature-colored slider (thermometer)
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - runtime temperature control for interactive MD.

#include "temperatureslider.h"

#include <QDoubleSpinBox>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

#include <algorithm>

// ---------------------------------------------------------------------------
// ThermoTrack
// ---------------------------------------------------------------------------

ThermoTrack::ThermoTrack(QWidget* parent)
    : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void ThermoTrack::setRange(double mn, double mx)
{
    if (mx <= mn)
        mx = mn + 1.0;
    m_min = mn;
    m_max = mx;
    setValue(std::clamp(m_value, m_min, m_max));  // re-clamp + repaint
    update();
}

void ThermoTrack::setValue(double v, bool emitSignal)
{
    const double clamped = std::clamp(v, m_min, m_max);
    if (qFuzzyCompare(clamped, m_value)) {
        update();
        return;
    }
    m_value = clamped;
    update();
    if (emitSignal)
        emit valueChanged(m_value);
}

// Thermal colour ramp: blue -> cyan -> green -> yellow -> red. Classic "cold to hot".
QColor ThermoTrack::colorAt(double v) const
{
    const double span = (m_max > m_min) ? (m_max - m_min) : 1.0;
    const double t = std::clamp((v - m_min) / span, 0.0, 1.0);

    static const struct { double pos; int r, g, b; } stops[] = {
        { 0.00, 30, 60, 255 },   // blue (cold)
        { 0.25, 0, 200, 255 },   // cyan
        { 0.50, 30, 200, 30 },   // green
        { 0.75, 255, 210, 0 },   // yellow
        { 1.00, 235, 40, 30 },   // red (hot)
    };
    for (int i = 1; i < 5; ++i) {
        if (t <= stops[i].pos) {
            const double f = (t - stops[i - 1].pos) / (stops[i].pos - stops[i - 1].pos);
            return QColor(
                static_cast<int>(stops[i - 1].r + f * (stops[i].r - stops[i - 1].r)),
                static_cast<int>(stops[i - 1].g + f * (stops[i].g - stops[i - 1].g)),
                static_cast<int>(stops[i - 1].b + f * (stops[i].b - stops[i - 1].b)));
        }
    }
    return QColor(stops[4].r, stops[4].g, stops[4].b);
}

QRect ThermoTrack::trackRect() const
{
    // Narrow vertical bar centred horizontally, with margins for the handle.
    const int w = 16;
    const int x = (width() - w) / 2;
    return QRect(x, 8, w, std::max(1, height() - 16));
}

double ThermoTrack::valueFromY(int y) const
{
    const QRect r = trackRect();
    const double frac = std::clamp(double(r.bottom() - y) / std::max(1, r.height()), 0.0, 1.0);
    return m_min + frac * (m_max - m_min);
}

int ThermoTrack::yFromValue(double v) const
{
    const QRect r = trackRect();
    const double span = (m_max > m_min) ? (m_max - m_min) : 1.0;
    const double frac = std::clamp((v - m_min) / span, 0.0, 1.0);
    return r.bottom() - static_cast<int>(frac * r.height());
}

void ThermoTrack::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = trackRect();

    // Gradient fill (top = max = red, bottom = min = blue).
    QLinearGradient grad(r.topLeft(), r.bottomLeft());
    grad.setColorAt(0.0, colorAt(m_max));
    grad.setColorAt(0.25, colorAt(m_min + 0.75 * (m_max - m_min)));
    grad.setColorAt(0.50, colorAt(m_min + 0.50 * (m_max - m_min)));
    grad.setColorAt(0.75, colorAt(m_min + 0.25 * (m_max - m_min)));
    grad.setColorAt(1.0, colorAt(m_min));
    p.setPen(QPen(palette().mid().color(), 1));
    p.setBrush(grad);
    p.drawRoundedRect(r, 6, 6);

    // Handle: a horizontal bar at the current value, tinted with the value's colour.
    const int y = yFromValue(m_value);
    const QColor c = colorAt(m_value);
    QRect handle(r.left() - 5, y - 4, r.width() + 10, 8);
    p.setBrush(c);
    p.setPen(QPen(c.darker(160), 1.5));
    p.drawRoundedRect(handle, 3, 3);
}

void ThermoTrack::mousePressEvent(QMouseEvent* e)
{
    setValue(valueFromY(static_cast<int>(e->position().y())));
}

void ThermoTrack::mouseMoveEvent(QMouseEvent* e)
{
    if (e->buttons() & Qt::LeftButton)
        setValue(valueFromY(static_cast<int>(e->position().y())));
}

// ---------------------------------------------------------------------------
// TemperatureSlider
// ---------------------------------------------------------------------------

TemperatureSlider::TemperatureSlider(QWidget* parent)
    : QWidget(parent)
{
    m_track = new ThermoTrack(this);

    m_maxSpin = new QDoubleSpinBox(this);
    m_maxSpin->setRange(1.0, 100000.0);
    m_maxSpin->setDecimals(0);
    m_maxSpin->setSuffix(" K");
    m_maxSpin->setValue(1000.0);
    m_maxSpin->setToolTip(tr("Slider maximum temperature"));

    m_minSpin = new QDoubleSpinBox(this);
    m_minSpin->setRange(0.0, 100000.0);
    m_minSpin->setDecimals(0);
    m_minSpin->setSuffix(" K");
    m_minSpin->setValue(1.0);
    m_minSpin->setToolTip(tr("Slider minimum temperature"));

    m_valueLabel = new QLabel(this);
    m_valueLabel->setAlignment(Qt::AlignCenter);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(3);
    lay->addWidget(m_maxSpin);
    lay->addWidget(m_track, 1, Qt::AlignHCenter);
    lay->addWidget(m_minSpin);
    lay->addWidget(m_valueLabel);

    m_track->setRange(1.0, 1000.0);
    m_track->setValue(300.0, false);

    connect(m_track, &ThermoTrack::valueChanged, this, &TemperatureSlider::onTrackChanged);
    connect(m_minSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &TemperatureSlider::onSpinRangeChanged);
    connect(m_maxSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &TemperatureSlider::onSpinRangeChanged);

    onTrackChanged(m_track->value());  // initialise the readout
}

double TemperatureSlider::value() const { return m_track->value(); }
double TemperatureSlider::minimum() const { return m_track->minimum(); }
double TemperatureSlider::maximum() const { return m_track->maximum(); }

void TemperatureSlider::setValue(double v)
{
    m_track->setValue(v, false);
    onTrackChanged(m_track->value());
}

void TemperatureSlider::setRange(double mn, double mx)
{
    const QSignalBlocker b1(m_minSpin);
    const QSignalBlocker b2(m_maxSpin);
    m_minSpin->setValue(mn);
    m_maxSpin->setValue(mx);
    m_track->setRange(mn, mx);
    onTrackChanged(m_track->value());
}

void TemperatureSlider::onTrackChanged(double v)
{
    const QColor c = m_track->colorAt(v);
    m_valueLabel->setText(QStringLiteral("%1 K").arg(v, 0, 'f', 0));
    m_valueLabel->setStyleSheet(QStringLiteral("font-weight:bold; color:%1;").arg(c.darker(140).name()));
    emit valueChanged(v);
}

void TemperatureSlider::onSpinRangeChanged()
{
    double mn = m_minSpin->value();
    double mx = m_maxSpin->value();
    if (mx <= mn)
        mx = mn + 1.0;
    m_track->setRange(mn, mx);
    onTrackChanged(m_track->value());
}
