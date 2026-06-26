// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// EditorsDock — right-side dock with internal tabs for Structure editor,
// Input editor and RMSD / Align tool.
//
// Claude Generated 2026 - Dock system restructuring.

#pragma once

#include "dockconfig.h"

#include <QDockWidget>

class ModifiableTextEdit;
class QLineEdit;
class RMSDWidget;
class QTabWidget;

class EditorsDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit EditorsDock(QWidget* parent = nullptr);

    QTabWidget* editorsTabs() const;
    ModifiableTextEdit* structureView() const;
    ModifiableTextEdit* inputView() const;
    RMSDWidget* rmsdWidget() const;

    QLineEdit* structureFileEdit() const;
    QLineEdit* structureFileEditExtension() const;
    QLineEdit* inputFileEdit() const;
    QLineEdit* inputFileEditExtension() const;

    /// Switch to a tab by its intended role.
    void setCurrentTab(int index);

signals:
    /// "Apply → Viewer" was clicked in the structure editor.
    void structureApplyRequested();

private:
    void setupUI();

    QTabWidget* m_editorsTabs = nullptr;
    ModifiableTextEdit* m_structureView = nullptr;
    ModifiableTextEdit* m_inputView = nullptr;
    RMSDWidget* m_rmsdWidget = nullptr;

    QLineEdit* m_structureFileEdit = nullptr;
    QLineEdit* m_structureFileEditExtension = nullptr;
    QLineEdit* m_inputFileEdit = nullptr;
    QLineEdit* m_inputFileEditExtension = nullptr;
};
