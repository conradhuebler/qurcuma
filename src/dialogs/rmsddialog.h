// rmsddialog.h
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Dialog exposing curcuma's RMSDDriver (structure
// alignment + atom reordering/permutation) directly in qurcuma. Lets the user
// overlay two structures, pick the permutation method, read the RMSD value and
// the reorder mapping, and save the aligned target.
#ifndef RMSDDIALOG_H
#define RMSDDIALOG_H

#include <QDialog>
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
 * @brief RMSD / align / reorder dialog backed by curcuma's RMSDDriver.
 *
 * Reference = the structure currently displayed in the viewer (seeded by
 * MainWindow). Target = a second structure loaded from a file. On "Align" the
 * dialog runs RMSDDriver with the chosen method/options, then emits
 * overlayRequested() so MainWindow can superimpose both structures in the 3D
 * viewer (aligned target in a distinct colour).
 */
class RMSDDialog : public QDialog {
    Q_OBJECT

public:
    explicit RMSDDialog(QWidget* parent = nullptr);

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

private slots:
    void onLoadTarget();
    void onAlign();
    void onSaveAligned();
    void onMethodChanged(int index);

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

#endif // RMSDDIALOG_H
