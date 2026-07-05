// viewpresetwidget.cpp - UI for saving/loading reproducible camera+display presets.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Reproducible molecular views for uniform figures.

#include "viewpresetwidget.h"

#include "settings.h"
#include "view.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

ViewPresetWidget::ViewPresetWidget(MoleculeViewer* viewer, Settings* settings, QWidget* parent)
    : QWidget(parent)
    , m_viewer(viewer)
    , m_settings(settings)
{
    setupUI();
    // Claude Generated 2026 - one-time migration: remove the orphaned default
    // presets (Front/Top/Side with cameraDistance==0) that the removed
    // initializeDefaultViewPresets() used to create. User-saved presets with
    // the same names are kept (they carry a real cameraDistance).
    if (m_settings) {
        static const QStringList legacyDefaults = { QStringLiteral("Front"),
                                                    QStringLiteral("Top"),
                                                    QStringLiteral("Side") };
        bool changed = false;
        for (const QString& name : legacyDefaults) {
            const QVector<ViewPreset> presets = m_settings->viewPresets();
            auto it = std::find_if(presets.cbegin(), presets.cend(),
                [&name](const ViewPreset& p) { return p.name == name; });
            if (it != presets.cend() && it->cameraDistance <= 0.0f
                && it->zoomMode == ZoomMode::Absolute && it->zoomFactor == 3.0f) {
                m_settings->deleteViewPreset(name);
                changed = true;
            }
        }
        Q_UNUSED(changed);
    }
    refreshPresetList();
}

void ViewPresetWidget::setupUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    auto* title = new QLabel(tr("View Presets"));
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    root->addWidget(title);

    m_presetList = new QListWidget;
    m_presetList->setMaximumHeight(110);
    m_presetList->setToolTip(tr("Double-click a preset to restore camera and (optionally) display settings"));
    connect(m_presetList, &QListWidget::doubleClicked, this, &ViewPresetWidget::onListDoubleClicked);
    connect(m_presetList->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ViewPresetWidget::onSelectionChanged);
    root->addWidget(m_presetList);

    m_includeDisplayCheckBox = new QCheckBox(tr("Include display settings"));
    m_includeDisplayCheckBox->setChecked(true);
    m_includeDisplayCheckBox->setToolTip(tr("When checked, loading a preset also applies style, effects and lighting"));
    root->addWidget(m_includeDisplayCheckBox);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(4);

    m_saveButton = new QPushButton(tr("Save…"));
    m_saveButton->setToolTip(tr("Save the current viewer camera and display as a new preset"));
    connect(m_saveButton, &QPushButton::clicked, this, &ViewPresetWidget::onSaveCurrent);

    m_loadButton = new QPushButton(tr("Load"));
    m_loadButton->setToolTip(tr("Apply the selected preset to the viewer"));
    m_loadButton->setEnabled(false);
    connect(m_loadButton, &QPushButton::clicked, this, &ViewPresetWidget::onLoadSelected);

    m_deleteButton = new QPushButton(tr("Delete"));
    m_deleteButton->setToolTip(tr("Delete the selected preset"));
    m_deleteButton->setEnabled(false);
    connect(m_deleteButton, &QPushButton::clicked, this, &ViewPresetWidget::onDeleteSelected);

    buttonRow->addWidget(m_saveButton);
    buttonRow->addWidget(m_loadButton);
    buttonRow->addWidget(m_deleteButton);
    buttonRow->addStretch();
    root->addLayout(buttonRow);

    // Quick orientation buttons — only set rotation, leave zoom/display untouched.
    auto* quickLabel = new QLabel(tr("Quick orientation"));
    quickLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    root->addWidget(quickLabel);

    auto* quickRow = new QHBoxLayout;
    quickRow->setSpacing(4);
    m_frontButton = new QToolButton;
    m_frontButton->setText(tr("Front"));
    m_topButton = new QToolButton;
    m_topButton->setText(tr("Top"));
    m_sideButton = new QToolButton;
    m_sideButton->setText(tr("Side"));

    connect(m_frontButton, &QToolButton::clicked, this, [this]() { onQuickOrientation(0); });
    connect(m_topButton, &QToolButton::clicked, this, [this]() { onQuickOrientation(1); });
    connect(m_sideButton, &QToolButton::clicked, this, [this]() { onQuickOrientation(2); });

    quickRow->addWidget(m_frontButton);
    quickRow->addWidget(m_topButton);
    quickRow->addWidget(m_sideButton);
    quickRow->addStretch();
    root->addLayout(quickRow);
}

void ViewPresetWidget::refreshPresetList()
{
    if (!m_presetList || !m_settings)
        return;

    m_presetList->clear();
    const QVector<ViewPreset> presets = m_settings->viewPresets();
    for (const ViewPreset& p : presets) {
        QListWidgetItem* item = new QListWidgetItem(p.name, m_presetList);
        item->setData(Qt::UserRole, p.name);
    }
    onSelectionChanged();
}

void ViewPresetWidget::onSaveCurrent()
{
    if (!m_viewer || !m_settings)
        return;

    // Custom dialog: name + zoom mode.
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Save View Preset"));
    auto* l = new QVBoxLayout(&dialog);

    auto* nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(tr("Preset name"));
    l->addWidget(new QLabel(tr("Name:"), &dialog));
    l->addWidget(nameEdit);

    auto* modeCombo = new QComboBox(&dialog);
    modeCombo->addItem(tr("Absolute (same distance)"), static_cast<int>(ZoomMode::Absolute));
    modeCombo->addItem(tr("Relative (same on-screen size)"), static_cast<int>(ZoomMode::Relative));
    modeCombo->setToolTip(tr("Absolute: identical only for equally sized molecules.\n"
                             "Relative: the molecule keeps its on-screen size regardless of extent."));
    l->addWidget(new QLabel(tr("Zoom mode:"), &dialog));
    l->addWidget(modeCombo);

    auto* includeCheck = new QCheckBox(tr("Include display settings"), &dialog);
    includeCheck->setChecked(m_includeDisplayCheckBox->isChecked());
    l->addWidget(includeCheck);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    l->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString name = nameEdit->text().trimmed();
    if (name.isEmpty())
        return;

    if (m_settings->viewPresetExists(name)) {
        const auto reply = QMessageBox::question(this, tr("Overwrite Preset"),
            tr("A preset named '%1' already exists. Overwrite?").arg(name),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
    }

    const ZoomMode mode = static_cast<ZoomMode>(modeCombo->currentData().toInt());
    ViewPreset preset = m_viewer->currentViewPreset(mode);
    preset.name = name;

    if (!includeCheck->isChecked()) {
        // Keep only camera fields; reset display fields to defaults.
        ViewPreset defaults;
        preset.renderingMode = defaults.renderingMode;
        preset.colorScheme = defaults.colorScheme;
        preset.atomTransparency = defaults.atomTransparency;
        preset.atomShininess = defaults.atomShininess;
        preset.atomScaleFactor = defaults.atomScaleFactor;
        preset.bondThickness = defaults.bondThickness;
        preset.fogEnabled = defaults.fogEnabled;
        preset.fogIntensity = defaults.fogIntensity;
        preset.fogDistance = defaults.fogDistance;
        preset.ssaoEnabled = defaults.ssaoEnabled;
        preset.ssaoIntensity = defaults.ssaoIntensity;
        preset.ssaoRadius = defaults.ssaoRadius;
        preset.ssaoBias = defaults.ssaoBias;
        preset.bloomEnabled = defaults.bloomEnabled;
        preset.bloomThreshold = defaults.bloomThreshold;
        preset.bloomIntensity = defaults.bloomIntensity;
        preset.hdrEnabled = defaults.hdrEnabled;
        preset.exposure = defaults.exposure;
        preset.rotationMode = defaults.rotationMode;
        preset.wallVisible = defaults.wallVisible;
        preset.wallOpacity = defaults.wallOpacity;
        preset.backgroundColor = defaults.backgroundColor;
        for (int i = 0; i < 4; ++i)
            preset.cornerLightEnabled[i] = defaults.cornerLightEnabled[i];
    }

    m_settings->saveViewPreset(preset);
    refreshPresetList();

    for (int i = 0; i < m_presetList->count(); ++i) {
        if (m_presetList->item(i)->data(Qt::UserRole).toString() == name) {
            m_presetList->setCurrentRow(i);
            break;
        }
    }
}

void ViewPresetWidget::onLoadSelected()
{
    if (!m_viewer || !m_settings)
        return;

    QListWidgetItem* item = m_presetList->currentItem();
    if (!item)
        return;

    const QString name = item->data(Qt::UserRole).toString();
    const QVector<ViewPreset> presets = m_settings->viewPresets();
    auto it = std::find_if(presets.cbegin(), presets.cend(),
        [&name](const ViewPreset& p) { return p.name == name; });
    if (it == presets.cend())
        return;

    m_viewer->applyViewPreset(*it, true, m_includeDisplayCheckBox->isChecked());
}

void ViewPresetWidget::onDeleteSelected()
{
    if (!m_settings)
        return;

    QListWidgetItem* item = m_presetList->currentItem();
    if (!item)
        return;

    const QString name = item->data(Qt::UserRole).toString();
    const auto reply = QMessageBox::question(this, tr("Delete Preset"),
        tr("Delete the preset '%1'?").arg(name),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    m_settings->deleteViewPreset(name);
    refreshPresetList();
}

void ViewPresetWidget::onListDoubleClicked(const QModelIndex& index)
{
    Q_UNUSED(index)
    onLoadSelected();
}

void ViewPresetWidget::onSelectionChanged()
{
    const bool hasSelection = m_presetList && m_presetList->currentItem() != nullptr;
    if (m_loadButton) m_loadButton->setEnabled(hasSelection);
    if (m_deleteButton) m_deleteButton->setEnabled(hasSelection);
}

void ViewPresetWidget::onQuickOrientation(int axis)
{
    if (!m_viewer)
        return;

    QQuaternion q;
    switch (axis) {
    case 0: // Front
        q = QQuaternion();
        break;
    case 1: // Top — look down the Y axis
        q = QQuaternion::fromEulerAngles(-90.0f, 0.0f, 0.0f);
        break;
    case 2: // Side — look along the X axis
        q = QQuaternion::fromEulerAngles(0.0f, 90.0f, 0.0f);
        break;
    default:
        return;
    }
    m_viewer->setCameraOrientation(q);
}