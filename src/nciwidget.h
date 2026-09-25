// nciwidget.h - Contact table of the non-covalent interaction analysis
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026
//
// The 3D overlay answers "where", this table answers "which and how strong".
// The widget holds no viewer pointer: it emits what the user asked for and
// MainWindow drives the viewer, the same split RMSDWidget uses.

#pragma once

#include <QVector>
#include <QWidget>

#include "ncitypes.h"
#include "view.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;
class Settings;

class NciWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NciWidget(QWidget* parent = nullptr);

    /// Show a new analysis result. @p atoms is the frame the contacts refer to and
    /// is used for the atom labels in the table.
    void setResult(const nci::Result& result, const QVector<MoleculeViewer::Atom>& atoms);
    /// Adopt the user's interaction colours so the table matches the 3D overlay.
    void setKindPalette(const nci::Palette& palette);
    /// Keep the source selector in sync with the Display panel.
    void setSource(int source);
    /// Disable the analyse button while a calculation is in flight.
    void setBusy(bool busy);
    /// Replace the header line, e.g. with an error from the analysis worker.
    void setStatus(const QString& text);

signals:
    /// The user picked a different overlay source (0=off, 1=geometry,
    /// 2=GFN-FF parameters, 3=population analysis).
    void sourceChanged(int source);
    /// Run the calculated source for the frame currently displayed.
    void analysisRequested(int source);
    /// A table row was activated: select these atoms in the 3D view.
    void contactSelected(const QVector<int>& atomIndices);
    /// A row was double-clicked: select and zoom to these atoms.
    void contactFocused(const QVector<int>& atomIndices);

private:
    void setupUI();
    void copyTable();
    QVector<int> atomsOfRow(int row) const;

    QComboBox* m_sourceCombo = nullptr;
    QPushButton* m_analyseButton = nullptr;
    QPushButton* m_copyButton = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QTableWidget* m_table = nullptr;

    nci::Result m_result;
    nci::Palette m_palette;
};

// Claude Generated 2026 - UX stage 4: the NCI detection options (contact kinds, the
// hydrogen-bond gate, colours per interaction class, distance labels, live GFN-FF during
// MD), moved here from the Display panel so NCI has one home. Unlike NciWidget it drives
// the viewer directly, as the panel did; the source stays in NciWidget's selector.
// Only the two hydrogen-bond numbers are exposed: they are the ones worth moving when
// looking at a structure; the halogen-bond and van-der-Waals fractions keep their
// literature defaults (see nci::detectGeometric).
class NciOptionsWidget : public QWidget
{
    Q_OBJECT
public:
    NciOptionsWidget(MoleculeViewer* viewer, Settings* settings, QWidget* parent = nullptr);
    /// Re-read every option from the viewer (read-only; never writes viewer state).
    void syncFromViewer();

signals:
    void liveMdChanged(bool enabled);

private:
    void applyOptions();     // collect the widgets into nci::Options and push them
    void refreshPalette();
    void updatePairKinds();  // electrostatics/dispersion exist only for the GFN-FF source

    MoleculeViewer* m_viewer = nullptr;
    Settings* m_settings = nullptr;
    QCheckBox* m_hbondCheck = nullptr;
    QCheckBox* m_xbondCheck = nullptr;
    QCheckBox* m_piCheck = nullptr;
    QCheckBox* m_contactCheck = nullptr;
    QCheckBox* m_electrostaticCheck = nullptr;
    QCheckBox* m_dispersionCheck = nullptr;
    QDoubleSpinBox* m_hbDistanceSpin = nullptr;
    QSpinBox* m_hbAngleSpin = nullptr;
    QComboBox* m_kindCombo = nullptr;
    QPushButton* m_kindColorButton = nullptr;
    QCheckBox* m_labelCheck = nullptr;
    QCheckBox* m_liveMdCheck = nullptr;
    bool m_applying = false;  // our own change: no re-sync (would reformat a spin box mid-typing)
};
