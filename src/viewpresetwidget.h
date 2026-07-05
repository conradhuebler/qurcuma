// viewpresetwidget.h - UI for saving/loading reproducible camera+display presets.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Reproducible molecular views for uniform figures.

#pragma once

#include <QWidget>

class MoleculeViewer;
class Settings;
class QListWidget;
class QCheckBox;
class QPushButton;
class QToolButton;
class ViewPreset;

class ViewPresetWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ViewPresetWidget(MoleculeViewer* viewer, Settings* settings, QWidget* parent = nullptr);

    /// Re-populate the list from Settings (e.g. after external changes).
    void refreshPresetList();

private slots:
    void onSaveCurrent();
    void onLoadSelected();
    void onDeleteSelected();
    void onListDoubleClicked(const QModelIndex& index);
    void onSelectionChanged();
    void onQuickOrientation(int axis);  // 0=front, 1=top, 2=side

private:
    void setupUI();

    MoleculeViewer* m_viewer = nullptr;
    Settings* m_settings = nullptr;

    QListWidget* m_presetList = nullptr;
    QCheckBox* m_includeDisplayCheckBox = nullptr;
    QPushButton* m_saveButton = nullptr;
    QPushButton* m_loadButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QToolButton* m_frontButton = nullptr;
    QToolButton* m_topButton = nullptr;
    QToolButton* m_sideButton = nullptr;
};