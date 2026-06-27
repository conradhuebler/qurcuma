// rmsdwidget.h
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - RMSD / align / reorder *workspace*, embedded in the
// SimulationDock "RMSD / Align" tab. Wraps curcuma's RMSDDriver.
//
// The workspace is a table of structures. Exactly one is flagged the reference
// (radio button) and shown as the primary molecule; every other structure is
// aligned to it and drawn as a tinted overlay. Per structure the table shows the
// plain and the permutation RMSD and offers a colour tint, a size and a
// visibility toggle, plus removal. Changing the reference re-aligns all others.
//
// The widget stays decoupled from the viewer: it emits overlayWorkspaceChanged()
// (full rebuild) plus cheap per-overlay live-edit signals, and MainWindow drives
// the MoleculeViewer. seedReferenceRequested() asks MainWindow to (re-)seed the
// reference from the current viewer frame.
#ifndef RMSDWIDGET_H
#define RMSDWIDGET_H

#include <QColor>
#include <QVector>
#include <QWidget>

#include <vector>

#include "view.h"  // MoleculeViewer::Atom / Bond / OverlaySpec

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

class RMSDWidget : public QWidget {
    Q_OBJECT

public:
    explicit RMSDWidget(QWidget* parent = nullptr);

    /** Seed (or refresh) the reference structure from the currently displayed molecule. */
    void setReferenceStructure(const QVector<MoleculeViewer::Atom>& atoms,
        const QVector<MoleculeViewer::Bond>& bonds, const QString& name);

    /** True once the workspace has a reference structure. */
    bool hasReference() const { return referenceIndex() >= 0; }

    /** Load a structure file, align it to the reference and add it to the workspace. */
    bool addStructureFromFile(const QString& path);

    /**
     * @brief Load a structure file into viewer atom/bond lists (no viewer side effects).
     * Supports .xyz / .pdb / .mol2 / .vtf via the existing parsers. Claude Generated.
     */
    static bool loadStructureFile(const QString& path,
        QVector<MoleculeViewer::Atom>& atoms, QVector<MoleculeViewer::Bond>& bonds);

    /** Reset the whole workspace (e.g. when a new molecule is loaded). */
    void clearWorkspace();

signals:
    /**
     * @brief Rebuild the viewer overlay set. The reference is drawn as the primary; each
     * overlay carries its aligned coords + tint/size/visibility. @p resetView true => the
     * reference changed (primary is reset, camera reframes); false => only the overlay set
     * changed (primary/camera untouched).
     */
    void overlayWorkspaceChanged(const QVector<MoleculeViewer::Atom>& refAtoms,
        const QVector<MoleculeViewer::Bond>& refBonds, bool refVisible,
        const QVector<MoleculeViewer::OverlaySpec>& overlays, bool resetView);

    /** Cheap per-overlay live edits (index into the current overlay set). */
    void overlayTintChanged(int overlayIndex, const QColor& tint);
    void overlaySizeChanged(int overlayIndex, float sizeScale);
    void overlayVisibilityChanged(int overlayIndex, bool visible);
    /** Reference (primary) structure visibility toggled. */
    void referenceVisibilityChanged(bool visible);

    /** A target was aligned + added to the workspace (status-bar feedback). */
    void structureAligned(const QString& name, double rmsd);

    /** Request MainWindow to (re-)seed the reference from the current viewer frame. */
    void seedReferenceRequested();

private slots:
    void onAddStructure();
    void onUseCurrentAsReference();
    void onRealignAll();
    void onMethodChanged(int index);
    void onSelectionChanged();

private:
    // One structure in the workspace (the reference or an aligned target).
    struct Structure {
        int id = 0;
        QString name;
        QVector<MoleculeViewer::Atom> original;  // as loaded / seeded
        QVector<MoleculeViewer::Bond> bonds;
        QVector<MoleculeViewer::Atom> aligned;   // aligned to current reference (== original if reference)
        bool isReference = false;
        QColor tint;
        float sizeScale = 0.8f;
        bool visible = true;
        bool hasResult = false;
        bool reordered = false;   // was permutation/reordering applied for this result?
        double rmsdPlain = 0.0;   // RMSDRaw (before reorder)
        double rmsdPerm = 0.0;    // RMSD (after reorder)
        std::vector<int> rules;   // reorder mapping (target index -> reference index)
    };

    void setupUI();
    void rebuildTable();
    void updateButtons();
    void pushWorkspace(bool referenceChanged);

    bool alignToReference(Structure& s);          // run RMSDDriver, fill aligned/rmsd/rules
    void realignAll();                            // re-align every non-reference structure
    bool addStructure(const QVector<MoleculeViewer::Atom>& atoms,
        const QVector<MoleculeViewer::Bond>& bonds, const QString& name);
    void setReferenceByIndex(int index);          // promote a structure to reference
    void removeStructure(int index);

    // per-row edit handlers (keyed by the structure's stable id)
    void onColorClicked(int id);
    void onSizeChanged(int id, double value);
    void onVisibilityToggled(int id, bool on);

    int referenceIndex() const;                    // index of the reference (-1 if none)
    int indexOfId(int id) const;
    // True if a structure with the same name (file name) is already in the workspace —
    // guards against adding the very same file twice.
    bool isDuplicateName(const QString& name) const;
    int overlayIndexOf(int structureIndex) const;  // position among the non-reference structures
    QColor nextDefaultTint();
    QString currentMethod() const;
    QWidget* centerCell(QWidget* inner);           // wrap a control centred in a table cell
    static void setSwatchColor(QPushButton* b, const QColor& c);

    // --- data ---
    QVector<Structure> m_structures;
    int m_nextId = 1;
    int m_nextTint = 0;

    // --- UI ---
    QPushButton* m_useReferenceButton = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_realignButton = nullptr;
    QComboBox* m_methodCombo = nullptr;
    QCheckBox* m_protonsCheck = nullptr;
    QCheckBox* m_reorderCheck = nullptr;
    QLineEdit* m_elementEdit = nullptr;
    QSpinBox* m_threadsSpin = nullptr;
    QTableWidget* m_table = nullptr;
    QButtonGroup* m_refGroup = nullptr;
    QLabel* m_reorderLabel = nullptr;
    QPlainTextEdit* m_reorderText = nullptr;
};

#endif // RMSDWIDGET_H
