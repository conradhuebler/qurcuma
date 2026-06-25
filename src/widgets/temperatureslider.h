// temperatureslider.h - Vertical, temperature-colored slider (thermometer)
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - runtime temperature control for interactive MD.
//
// A compact vertical control for the MD thermostat setpoint: a blue(min)..red(max)
// thermal gradient track with a draggable handle, editable min/max spin boxes, and a
// numeric readout. Stays live during a running simulation so the user can change the
// target temperature on the fly (emitted as valueChanged()).

#pragma once

#include <QWidget>

class QDoubleSpinBox;
class QLabel;

/** @brief Inner draggable colored track. blue = min (bottom), red = max (top). */
class ThermoTrack : public QWidget {
    Q_OBJECT
public:
    explicit ThermoTrack(QWidget* parent = nullptr);

    double value() const { return m_value; }
    double minimum() const { return m_min; }
    double maximum() const { return m_max; }
    void setRange(double mn, double mx);
    void setValue(double v, bool emitSignal = true);

    /** @brief Thermal colour for a temperature value (blue->cyan->green->yellow->red). */
    QColor colorAt(double v) const;

signals:
    void valueChanged(double v);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    QSize sizeHint() const override { return QSize(56, 170); }
    QSize minimumSizeHint() const override { return QSize(44, 90); }

private:
    QRect trackRect() const;
    double valueFromY(int y) const;
    int yFromValue(double v) const;

    double m_min = 1.0;
    double m_max = 1000.0;
    double m_value = 300.0;
};

/** @brief Composite widget: max spin (top), thermal track, min spin (bottom), value label. */
class TemperatureSlider : public QWidget {
    Q_OBJECT
public:
    explicit TemperatureSlider(QWidget* parent = nullptr);

    double value() const;
    double minimum() const;
    double maximum() const;

public slots:
    void setValue(double v);
    void setRange(double mn, double mx);

signals:
    /** @brief Emitted whenever the handle moves (drag or setValue from code). */
    void valueChanged(double v);

private slots:
    void onTrackChanged(double v);
    void onSpinRangeChanged();

private:
    ThermoTrack* m_track = nullptr;
    QDoubleSpinBox* m_minSpin = nullptr;
    QDoubleSpinBox* m_maxSpin = nullptr;
    QLabel* m_valueLabel = nullptr;
};
