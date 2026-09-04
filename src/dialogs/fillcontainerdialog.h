// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// fillcontainerdialog — pick fragments and copy counts, and the container they
// go into, for build::fillContainer. Pre-fills the container from the current
// confinement-wall settings so the packed scene matches the wall the simulation
// will use. Claude Generated 2026.
#pragma once

#include "scenefiller.h"
#include "simulationworker.h"  // SimulationConfig (wall settings)

#include <QDialog>
#include <QVector>

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QTableWidget;
class QStackedWidget;

class FillContainerDialog : public QDialog {
    Q_OBJECT
public:
    /** @param cfg supplies the container pre-fill (wall type, radius, bounds). */
    explicit FillContainerDialog(const SimulationConfig& cfg, QWidget* parent = nullptr);

    /** @brief Fragment/count rows with a count > 0. Pointers into the fragment library. */
    QVector<build::FillRequest> requests() const;
    build::Container container() const;
    float minDistance() const;
    quint32 seed() const;

    /** @brief True when the user wants the confinement wall set to this container. */
    bool applyToWall() const;

private:
    void addRow(const QString& fragmentName = QString(), int count = 1);

    QTableWidget* m_table = nullptr;
    QComboBox* m_shapeCombo = nullptr;
    QStackedWidget* m_shapeStack = nullptr;
    QDoubleSpinBox* m_radiusSpin = nullptr;
    QDoubleSpinBox* m_boxSpin[3][2] {};   // [axis][min,max]
    QDoubleSpinBox* m_minDistSpin = nullptr;
    QSpinBox* m_seedSpin = nullptr;
    class QCheckBox* m_applyWallCheck = nullptr;
};
