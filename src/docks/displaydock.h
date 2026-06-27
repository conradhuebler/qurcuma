// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// DisplayDock — right-side dock with a segmented top area
// [Structure | Atoms] and a bottom Display panel.
//
// Claude Generated 2026 - Dock system restructuring.

#pragma once

#include "dockconfig.h"

#include <QDockWidget>

class AtomListPanel;
class DisplayPanel;
class ModifiableTextEdit;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QToolButton;
class MoleculeViewer;
class Settings;

class DisplayDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit DisplayDock(MoleculeViewer* viewer, Settings* settings, QWidget* parent = nullptr);

    enum class TopSegment {
        Structure,
        Atoms
    };

    TopSegment currentTopSegment() const;
    void setCurrentTopSegment(TopSegment segment);

    // Segment buttons
    QToolButton* structureSegmentButton() const;
    QToolButton* atomsSegmentButton() const;

    // Structure editor
    ModifiableTextEdit* structureView() const;
    QLineEdit* structureFileEdit() const;
    QLineEdit* structureFileEditExtension() const;

    // Atom table
    AtomListPanel* atomListPanel() const;

    // Display panel
    DisplayPanel* displayPanel() const;

signals:
    /// "Apply → Viewer" was clicked in the structure editor.
    void structureApplyRequested();

private:
    void setupUI(MoleculeViewer* viewer, Settings* settings);
    QWidget* createStructurePage();
    QWidget* createAtomsPage();

    QToolButton* m_structureSegmentBtn = nullptr;
    QToolButton* m_atomsSegmentBtn = nullptr;
    QStackedWidget* m_topStack = nullptr;

    ModifiableTextEdit* m_structureView = nullptr;
    QLineEdit* m_structureFileEdit = nullptr;
    QLineEdit* m_structureFileEditExtension = nullptr;

    AtomListPanel* m_atomListPanel = nullptr;
    DisplayPanel* m_displayPanel = nullptr;
};
