// simulationparameterswidget.h - "All parameters" tab of the Simulation dock.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 (UX stage 6 S3). Generated from curcuma's ParameterRegistry:
// every parameter of the simplemd (MD) module, grouped by category, filterable by
// tier. Parameters the Simulation tab sets, or that qurcuma fixes for the interactive
// run, are shown read-only with the value that is sent; the rest are editable, and
// only values that differ from curcuma's default are stored (SimulationConfig::
// mdExtraParams) and sent. Runs in parallel to the hand-built Simulation tab so the
// two can be compared on real runs before either shrinks.
#pragma once

#include "simulationworker.h"  // SimulationConfig

#include <QJsonObject>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

class SimulationParametersWidget : public QWidget {
    Q_OBJECT
public:
    explicit SimulationParametersWidget(QWidget* parent = nullptr);

    /// Show @p cfg: the values the Simulation tab sends in the read-only rows, the
    /// stored extra parameters in the editable ones, and which parameters the current
    /// settings make relevant.
    void setConfig(const SimulationConfig& cfg);

signals:
    /// The editable rows changed; @p params holds their non-default values.
    void extraParamsChanged(const QJsonObject& params);

private:
    enum class Kind { Bool, Int, Double, Text, Json, Choice };
    struct Row {
        QString name;
        QString category;
        QString help;
        QString unit;
        QString relevantWhen;
        Kind kind = Kind::Text;
        bool numericChoice = false;  // Choice whose values are numbers
        int tier = 1;                // 0 primary, 1 advanced, 2 expert
        bool hand = false;           // set by the Simulation tab / qurcuma (read-only)
        QJsonValue defaultValue;
        QTreeWidgetItem* item = nullptr;
        QWidget* editor = nullptr;   // QLabel for hand rows
    };

    void build();
    void applyFilter();
    void refreshRows();
    void setEditorValue(const Row& row, const QJsonValue& value);
    QJsonValue editorValue(const Row& row, bool* ok) const;
    void onEdited(int index);
    bool isChanged(const Row& row) const;

    QVector<Row> m_rows;
    QTreeWidget* m_tree = nullptr;
    QLineEdit* m_search = nullptr;
    QComboBox* m_tierCombo = nullptr;
    QCheckBox* m_changedOnly = nullptr;
    QLabel* m_modeNote = nullptr;
    QJsonObject m_defaults;    // curcuma defaults, canonical names
    QJsonObject m_hand;        // what the Simulation tab / qurcuma send
    QJsonObject m_extra;       // non-default values of the editable rows
    bool m_updating = false;   // editors are being set from a config
};
