// rmsdwidget.h
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - RMSD / align / reorder tool widget, embedded in the
// "Analysis" dock. Replaces the former RMSDDialog. Wraps curcuma's RMSDDriver:
// aligns a target structure onto the reference (seeded from the viewer),
// optionally reorders atoms (permutation) so chemically equivalent atoms match,
// reports the RMSD + reorder mapping and emits the aligned target for 3D overlay.
//
// The widget stays decoupled from the viewer: the reference is pushed in by
// MainWindow (setReferenceStructure), and the "Use current as reference" button
// only emits seedReferenceRequested() — MainWindow reads the viewer and reseeds.
#ifndef RMSDWIDGET_H
#define RMSDWIDGET_H

#include <QWidget>
#include <QVector>

#include "view.h"  // MoleculeViewer::Atom / Bond

class QComboBox;
class QCheckBox;
class QLineEdit;
class QSpinBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

/**
 * @brief RMSD / align / reorder panel backed by curcuma's RMSDDriver.
 *
 * Reference = the structure currently displayed in the viewer (seeded by
 * MainWindow). Target = a second structure loaded from a file. On "Align" the
 * widget runs RMSDDriver with the chosen method/options, then emits
 * overlayRequested() so MainWindow can superimpose both structures in the 3D
 * viewer (aligned target in a distinct colour). seedReferenceRequested() asks
 * MainWindow to re-seed the reference from the current viewer frame.
 */
class RMSDWidget : public QWidget {
    Q_OBJECT

public:
    explicit RMSDWidget(QWidget* parent = nullptr);

    /** Seed the reference structure (the currently displayed molecule). */
    void setReferenceStructure(const QVector<MoleculeViewer::Atom>& atoms,
        const QVector<MoleculeViewer::Bond>& bonds,
        const QString& name);

    /** Preload a target file (e.g. when launched from the file-manager context menu). */
    void setTargetFile(const QString& path);

    /**
     * @brief Load a structure file into a viewer atom/bond list (no viewer side effects).
     * Supports .xyz / .pdb / .mol2 / .vtf via the existing parsers. Claude Generated.
     */
    static bool loadStructureFile(const QString& path,
        QVector<MoleculeViewer::Atom>& atoms,
        QVector<MoleculeViewer::Bond>& bonds);

signals:
    /** Request MainWindow to overlay reference + aligned target in the 3D viewer. */
    void overlayRequested(const QVector<MoleculeViewer::Atom>& refAtoms,
        const QVector<MoleculeViewer::Bond>& refBonds,
        const QVector<MoleculeViewer::Atom>& targetAtoms);

    /** Request MainWindow to re-seed the reference from the current viewer frame. */
    void seedReferenceRequested();

private slots:
    void onLoadTarget();
    void onAlign();
    void onSaveAligned();
    void onMethodChanged(int index);
    void onUseCurrentAsReference();

private:
    void setupUI();
    void updateReferenceLabel();
    void updateTargetLabel();
    QString currentMethod() const;  // selected method string for RMSDDriver

    // --- structures ---
    QVector<MoleculeViewer::Atom> m_refAtoms;
    QVector<MoleculeViewer::Bond> m_refBonds;
    QString m_refName;

    QVector<MoleculeViewer::Atom> m_targetAtoms;
    QVector<MoleculeViewer::Bond> m_targetBonds;
    QString m_targetName;

    QVector<MoleculeViewer::Atom> m_alignedAtoms;  // result of last alignment
    bool m_haveResult = false;

    // --- UI ---
    QLabel* m_referenceLabel = nullptr;
    QPushButton* m_useReferenceButton = nullptr;
    QLabel* m_targetLabel = nullptr;
    QPushButton* m_loadTargetButton = nullptr;
    QComboBox* m_methodCombo = nullptr;
    QCheckBox* m_protonsCheck = nullptr;
    QCheckBox* m_forceReorderCheck = nullptr;
    QCheckBox* m_noReorderCheck = nullptr;
    QLineEdit* m_elementEdit = nullptr;
    QSpinBox* m_threadsSpin = nullptr;
    QPushButton* m_alignButton = nullptr;
    QLabel* m_rmsdLabel = nullptr;
    QPlainTextEdit* m_reorderText = nullptr;
    QPushButton* m_saveButton = nullptr;
};

#endif // RMSDWIDGET_H