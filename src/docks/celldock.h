// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// CellDock — right-side dock for a loaded CIF: what the file contained (cell,
// sites, symmetry operations, disorder groups, reader notes), what is shown
// (asymmetric unit or unit cell, which disorder group) and how often the cell is
// repeated along a, b and c.
//
// Every choice is applied while the file is read -- the viewer's atoms carry no
// cell and no disorder labels -- so the dock asks MainWindow to read the file
// again. Content and disorder apply at once (cheap); the repeats wait for Apply,
// because a large supercell takes a moment to build.
//
// Claude Generated 2026.

#pragma once

#include "dockconfig.h"

#include "../moleculefileloader.h"

#include <QDockWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QStackedWidget;

class CellDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit CellDock(QWidget* parent = nullptr);

    /// Show @p info for @p fileName, and the choices it was read with.
    void setCif(const QString& fileName, const MoleculeFileLoader::CifInfo& info);
    /// Back to the "no cif loaded" page.
    void clearCif();
    bool hasCif() const { return m_hasCif; }
    /// Set the "Show cell" box without emitting cellShownChanged.
    void setCellShown(bool shown);
    /// Set the ellipsoid controls without emitting ellipsoidsChanged.
    void setEllipsoidSettings(bool shown, int percent);

signals:
    /// Read the file again, built this way.
    void optionsRequested(const MoleculeFileLoader::CifOptions& options);
    /// The "Show cell" box was toggled.
    void cellShownChanged(bool shown);
    /// Thermal ellipsoids switched or their probability changed (percent).
    void ellipsoidsChanged(bool shown, int percent);

private:
    /// The options the controls describe.
    MoleculeFileLoader::CifOptions currentOptions() const;
    void updatePreview();
    void updateEnabled();

    QStackedWidget* m_pages = nullptr;
    QLabel* m_fileLabel = nullptr;
    QRadioButton* m_showUnit = nullptr;
    QRadioButton* m_showCellContent = nullptr;
    QCheckBox* m_completeMolecules = nullptr;
    QLabel* m_spaceGroup = nullptr;
    QLabel* m_formula = nullptr;
    QGroupBox* m_disorderBox = nullptr;
    QComboBox* m_disorderCombo = nullptr;
    QLabel* m_a = nullptr;
    QLabel* m_b = nullptr;
    QLabel* m_c = nullptr;
    QLabel* m_alpha = nullptr;
    QLabel* m_beta = nullptr;
    QLabel* m_gamma = nullptr;
    QLabel* m_volume = nullptr;
    QCheckBox* m_showCell = nullptr;
    QLabel* m_sites = nullptr;
    QLabel* m_operations = nullptr;
    QLabel* m_unitAtoms = nullptr;
    QLabel* m_cellAtoms = nullptr;
    QGroupBox* m_repeatBox = nullptr;
    QSpinBox* m_na = nullptr;
    QSpinBox* m_nb = nullptr;
    QSpinBox* m_nc = nullptr;
    QLabel* m_preview = nullptr;
    QPushButton* m_apply = nullptr;
    QLabel* m_notes = nullptr;
    QGroupBox* m_ellipsoidBox = nullptr;
    QCheckBox* m_showEllipsoids = nullptr;
    QSpinBox* m_probability = nullptr;
    QLabel* m_adpCounts = nullptr;

    bool m_hasCif = false;
    MoleculeFileLoader::CifInfo m_info;   // what the shown structure was read with
};
