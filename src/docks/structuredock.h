// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// StructureDock — right-side dock with a segmented [Structure | Atoms] area: the
// XYZ text editor (Apply → Viewer) and the editable atom table. The display
// settings moved to AppearanceDock in UX stage 4.
//
// Claude Generated 2026 - Dock system restructuring.

#pragma once

#include "dockconfig.h"

#include <QDockWidget>

class AtomListPanel;
class ModifiableTextEdit;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QToolButton;

class StructureDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit StructureDock(QWidget* parent = nullptr);

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



signals:
    /// "Apply → Viewer" was clicked in the structure editor.
    void structureApplyRequested();

private:
    void setupUI();
    QWidget* createStructurePage();
    QWidget* createAtomsPage();

    QToolButton* m_structureSegmentBtn = nullptr;
    QToolButton* m_atomsSegmentBtn = nullptr;
    QStackedWidget* m_topStack = nullptr;

    ModifiableTextEdit* m_structureView = nullptr;
    QLineEdit* m_structureFileEdit = nullptr;
    QLineEdit* m_structureFileEditExtension = nullptr;

    AtomListPanel* m_atomListPanel = nullptr;
};
