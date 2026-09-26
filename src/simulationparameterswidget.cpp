// simulationparameterswidget.cpp - "All parameters" tab of the Simulation dock.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 (UX stage 6 S3).

#include "simulationparameterswidget.h"

#include "simparameters.h"

#include "external/json.hpp"
#include <src/core/parameter_registry.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleValidator>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <climits>

namespace {

QJsonObject toQJsonObject(const nlohmann::json& object)
{
    return QJsonDocument::fromJson(QByteArray::fromStdString(object.dump())).object();
}

// Parse the text of a Json-kind editor; a bare value like 3 or "x" is accepted too.
QJsonValue parseJsonText(const QString& text, bool* ok)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(
        QStringLiteral("[%1]").arg(text.trimmed()).toUtf8(), &err);
    *ok = err.error == QJsonParseError::NoError && doc.isArray() && doc.array().size() == 1;
    return *ok ? doc.array().at(0) : QJsonValue();
}

} // namespace

SimulationParametersWidget::SimulationParametersWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* intro = new QLabel(tr("All parameters of curcuma's MD module (simplemd). Values that "
                                "differ from curcuma's default are sent with the run. Grey rows "
                                "are set in the Simulation tab or fixed by qurcuma for the "
                                "interactive run and are read-only here."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_modeNote = new QLabel(tr("The Simulation tab is in optimization mode: these parameters "
                               "only apply to MD runs."), this);
    m_modeNote->setWordWrap(true);
    m_modeNote->setStyleSheet(QStringLiteral("color: #c47f00;"));
    m_modeNote->setVisible(false);
    layout->addWidget(m_modeNote);

    auto* filterRow = new QHBoxLayout;
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search name or description"));
    m_search->setClearButtonEnabled(true);
    filterRow->addWidget(m_search, 1);
    m_tierCombo = new QComboBox(this);
    m_tierCombo->addItem(tr("Primary"), 0);
    m_tierCombo->addItem(tr("Primary + advanced"), 1);
    m_tierCombo->addItem(tr("All"), 2);
    m_tierCombo->setCurrentIndex(2);
    m_tierCombo->setToolTip(tr("curcuma's tier of each parameter: primary, advanced or expert."));
    filterRow->addWidget(m_tierCombo);
    m_changedOnly = new QCheckBox(tr("Changed only"), this);
    m_changedOnly->setToolTip(tr("Only parameters whose value differs from curcuma's default, "
                                 "whether set here or in the Simulation tab."));
    filterRow->addWidget(m_changedOnly);
    auto* resetButton = new QPushButton(tr("Reset"), this);
    resetButton->setToolTip(tr("Set every editable parameter back to curcuma's default."));
    filterRow->addWidget(resetButton);
    layout->addLayout(filterRow);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({ tr("Parameter"), tr("Value"), tr("Default"), tr("Unit") });
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    layout->addWidget(m_tree, 1);

    build();

    connect(m_search, &QLineEdit::textChanged, this, [this]() { applyFilter(); });
    connect(m_tierCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int) { applyFilter(); });
    connect(m_changedOnly, &QCheckBox::toggled, this, [this]() { applyFilter(); });
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        m_updating = true;
        for (const Row& row : m_rows)
            if (!row.hand)
                setEditorValue(row, row.defaultValue);
        m_updating = false;
        m_extra = QJsonObject();
        refreshRows();
        emit extraParamsChanged(m_extra);
    });
}

// One row per non-deprecated simplemd parameter, under its category, in curcuma's order.
void SimulationParametersWidget::build()
{
    auto& registry = ParameterRegistry::getInstance();
    m_defaults = toQJsonObject(registry.getDefaultJson("simplemd"));
    const QStringList handKeys = SimulationWorker::handSimplemdKeys();

    QHash<QString, QTreeWidgetItem*> categories;
    for (const ParameterDefinition& def : registry.getForModule("simplemd")) {
        if (def.deprecated)
            continue;
        Row row;
        row.name = QString::fromStdString(def.name);
        row.category = QString::fromStdString(def.category);
        row.help = QString::fromStdString(def.helpText);
        row.unit = QString::fromStdString(def.unit);
        row.relevantWhen = QString::fromStdString(def.relevantWhen);
        row.tier = def.tier == ParamTier::Primary ? 0 : def.tier == ParamTier::Expert ? 2 : 1;
        row.hand = handKeys.contains(row.name);
        row.defaultValue = m_defaults.value(row.name);
        switch (def.type) {
        case ParamType::Bool:   row.kind = Kind::Bool; break;
        case ParamType::Int:    row.kind = Kind::Int; break;
        case ParamType::Double: row.kind = Kind::Double; break;
        case ParamType::StringList:
        case ParamType::Json:   row.kind = Kind::Json; break;
        default:                row.kind = Kind::Text; break;
        }
        if (!def.allowed.empty()) {
            row.numericChoice = row.kind == Kind::Int || row.kind == Kind::Double;
            row.kind = Kind::Choice;
        }

        QTreeWidgetItem*& categoryItem = categories[row.category];
        if (!categoryItem) {
            categoryItem = new QTreeWidgetItem(m_tree, { row.category });
            categoryItem->setFirstColumnSpanned(true);
            QFont f = categoryItem->font(0);
            f.setBold(true);
            categoryItem->setFont(0, f);
            categoryItem->setExpanded(true);
        }
        row.item = new QTreeWidgetItem(categoryItem,
            { row.name, QString(), simparams::valueText(row.defaultValue), row.unit });

        QStringList tip;
        if (!row.help.isEmpty())
            tip << row.help;
        if (!def.aliases.empty()) {
            QStringList aliases;
            for (const std::string& a : def.aliases)
                aliases << QString::fromStdString(a);
            tip << tr("Also accepted as: %1").arg(aliases.join(QStringLiteral(", ")));
        }
        if (!row.relevantWhen.isEmpty())
            tip << tr("Only used when %1.").arg(row.relevantWhen);
        if (row.hand)
            tip << tr("Set in the Simulation tab or fixed by qurcuma for the interactive run.");
        row.item->setToolTip(0, tip.join(QStringLiteral("\n")));

        const int index = m_rows.size();
        if (row.hand) {
            auto* label = new QLabel(m_tree);
            label->setEnabled(false);  // read-only look
            row.editor = label;
        } else {
            switch (row.kind) {
            case Kind::Bool: {
                auto* box = new QCheckBox(m_tree);
                connect(box, &QCheckBox::toggled, this, [this, index]() { onEdited(index); });
                row.editor = box;
                break;
            }
            case Kind::Choice: {
                auto* combo = new QComboBox(m_tree);
                for (const std::string& value : def.allowed)
                    combo->addItem(QString::fromStdString(value));
                connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                    [this, index](int) { onEdited(index); });
                row.editor = combo;
                break;
            }
            case Kind::Int: {
                auto* spin = new QSpinBox(m_tree);
                spin->setRange(def.hasMinimum ? int(def.minimum) : INT_MIN,
                               def.hasMaximum ? int(def.maximum) : INT_MAX);
                connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                    [this, index](int) { onEdited(index); });
                row.editor = spin;
                break;
            }
            case Kind::Double: {
                auto* edit = new QLineEdit(m_tree);
                auto* validator = new QDoubleValidator(edit);
                validator->setLocale(QLocale::c());
                validator->setNotation(QDoubleValidator::ScientificNotation);
                if (def.hasMinimum)
                    validator->setBottom(def.minimum);
                if (def.hasMaximum)
                    validator->setTop(def.maximum);
                edit->setValidator(validator);
                connect(edit, &QLineEdit::editingFinished, this, [this, index]() { onEdited(index); });
                row.editor = edit;
                break;
            }
            case Kind::Text:
            case Kind::Json: {
                auto* edit = new QLineEdit(m_tree);
                connect(edit, &QLineEdit::editingFinished, this, [this, index]() { onEdited(index); });
                row.editor = edit;
                break;
            }
            }
        }
        m_rows.append(row);
        m_tree->setItemWidget(row.item, 1, row.editor);
        m_updating = true;
        if (!row.hand)
            setEditorValue(m_rows.last(), row.defaultValue);
        m_updating = false;
    }
    refreshRows();
}

void SimulationParametersWidget::setEditorValue(const Row& row, const QJsonValue& value)
{
    if (auto* box = qobject_cast<QCheckBox*>(row.editor))
        box->setChecked(value.toBool());
    else if (auto* combo = qobject_cast<QComboBox*>(row.editor))
        combo->setCurrentIndex(qMax(0, combo->findText(simparams::valueText(value))));
    else if (auto* spin = qobject_cast<QSpinBox*>(row.editor))
        spin->setValue(value.toInt());
    else if (auto* edit = qobject_cast<QLineEdit*>(row.editor))
        edit->setText(simparams::valueText(value));
}

QJsonValue SimulationParametersWidget::editorValue(const Row& row, bool* ok) const
{
    *ok = true;
    switch (row.kind) {
    case Kind::Bool:
        return qobject_cast<QCheckBox*>(row.editor)->isChecked();
    case Kind::Int:
        return qobject_cast<QSpinBox*>(row.editor)->value();
    case Kind::Choice: {
        const QString text = qobject_cast<QComboBox*>(row.editor)->currentText();
        if (!row.numericChoice)
            return text;
        const double v = QLocale::c().toDouble(text, ok);
        return v;
    }
    case Kind::Double: {
        const double v = QLocale::c().toDouble(qobject_cast<QLineEdit*>(row.editor)->text().trimmed(), ok);
        return v;
    }
    case Kind::Json:
        return parseJsonText(qobject_cast<QLineEdit*>(row.editor)->text(), ok);
    case Kind::Text:
        return qobject_cast<QLineEdit*>(row.editor)->text();
    }
    *ok = false;
    return QJsonValue();
}

// Store the edited value when it differs from curcuma's default, drop it otherwise.
void SimulationParametersWidget::onEdited(int index)
{
    if (m_updating || index < 0 || index >= m_rows.size())
        return;
    const Row& row = m_rows[index];
    bool ok = false;
    const QJsonValue value = editorValue(row, &ok);
    row.editor->setStyleSheet(ok ? QString() : QStringLiteral("color: #e05757;"));
    if (!ok)
        return;
    if (simparams::sameValue(value, row.defaultValue))
        m_extra.remove(row.name);
    else
        m_extra.insert(row.name, value);
    refreshRows();
    emit extraParamsChanged(m_extra);
}

void SimulationParametersWidget::setConfig(const SimulationConfig& cfg)
{
    m_modeNote->setVisible(cfg.mode != SimulationConfig::Mode::MolecularDynamics);
    m_hand = SimulationWorker::handSimplemdParams(cfg);
    if (cfg.mdExtraParams != m_extra) {  // changed outside this tab (lesson, recipe)
        m_updating = true;
        for (const Row& row : m_rows)
            if (!row.hand)
                setEditorValue(row, cfg.mdExtraParams.contains(row.name)
                                        ? cfg.mdExtraParams.value(row.name) : row.defaultValue);
        m_updating = false;
        m_extra = cfg.mdExtraParams;
    }
    refreshRows();
}

bool SimulationParametersWidget::isChanged(const Row& row) const
{
    if (row.hand)
        return m_hand.contains(row.name) && !simparams::sameValue(m_hand.value(row.name), row.defaultValue);
    return m_extra.contains(row.name);
}

// Hand rows show the value that is sent; every row shows whether it is relevant under
// the effective values (defaults, then the Simulation tab, then this tab) and whether
// it differs from curcuma's default (bold).
void SimulationParametersWidget::refreshRows()
{
    QJsonObject effective = m_defaults;
    for (auto it = m_extra.begin(); it != m_extra.end(); ++it)
        effective.insert(it.key(), it.value());
    for (auto it = m_hand.begin(); it != m_hand.end(); ++it)
        effective.insert(it.key(), it.value());

    const QColor dim = palette().color(QPalette::Disabled, QPalette::Text);
    for (const Row& row : m_rows) {
        if (row.hand) {
            auto* label = qobject_cast<QLabel*>(row.editor);
            label->setText(m_hand.contains(row.name) ? simparams::valueText(m_hand.value(row.name))
                                                      : tr("(not sent: default)"));
        }
        const bool relevant = simparams::conditionHolds(row.relevantWhen, effective);
        QFont f = row.item->font(0);
        f.setBold(isChanged(row));
        row.item->setFont(0, f);
        for (int c : { 0, 2, 3 })
            row.item->setForeground(c, relevant && !row.hand ? palette().color(QPalette::Text) : dim);
    }
    applyFilter();
}

void SimulationParametersWidget::applyFilter()
{
    const QString needle = m_search->text().trimmed();
    const int maxTier = m_tierCombo->currentData().toInt();
    const bool changedOnly = m_changedOnly->isChecked();
    QHash<QTreeWidgetItem*, bool> categoryVisible;
    for (const Row& row : m_rows) {
        bool visible = row.tier <= maxTier;
        if (visible && !needle.isEmpty())
            visible = row.name.contains(needle, Qt::CaseInsensitive)
                || row.help.contains(needle, Qt::CaseInsensitive);
        if (visible && changedOnly)
            visible = isChanged(row);
        row.item->setHidden(!visible);
        QTreeWidgetItem* category = row.item->parent();
        categoryVisible[category] = categoryVisible.value(category, false) || visible;
    }
    for (auto it = categoryVisible.begin(); it != categoryVisible.end(); ++it)
        it.key()->setHidden(!it.value());
}
